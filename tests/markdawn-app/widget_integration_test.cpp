// Widget-level integration tests for markdawn-app: things that can only be
// verified with real QWidget instances wired together, unlike
// tests/core-lib/toc_model_test.cpp's pure-model logic tests. Covers two
// real, user-reported bugs so far (not hypothesized edge cases) -- see each
// section below for the specific root cause.
//
// TabManager/DocumentView are QWidget subclasses, so unlike
// toc_model_test.cpp's bare QCoreApplication, this needs a real
// QApplication -- run under the offscreen platform plugin in CI (see this
// test's CMakeLists.txt for the same static-triplet plugin-linkage
// requirement the real markdawn executable has).

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextStream>
#include <iostream>
#include <vector>

#include "document_view/document_view.h"
#include "markdawncore/document/document_model.h"
#include "markdawncore/document/toc_model.h"
#include "tab_manager/tab_manager.h"

using markdawn::app::DocumentView;
using markdawn::app::TabManager;
using markdawn::core::DocumentModel;
using markdawn::core::TocModel;

namespace {

int g_failures = 0;

void check(bool condition, const QString& description) {
    if (!condition) {
        std::cerr << "FAIL: " << description.toStdString() << "\n";
        ++g_failures;
    }
}

// Writes markdownText into file (already open via QTemporaryFile) and
// returns its absolute path. The caller keeps file alive for as long as
// the path needs to stay valid -- its destructor deletes the file.
QString writeTempMarkdownFile(QTemporaryFile& file, const QString& markdownText) {
    file.setFileTemplate(QDir::tempPath() + QStringLiteral("/widget_integration_test_XXXXXX.md"));
    check(file.open(), "temp file opens");
    QTextStream(&file) << markdownText;
    file.flush();
    return QFileInfo(file).absoluteFilePath();
}

void runTabManagerFirstTabTest() {
    QTemporaryFile file1;
    const QString path1 = writeTempMarkdownFile(file1, QStringLiteral("# First File\n## Sub A\n"));
    QTemporaryFile file2;
    const QString path2 = writeTempMarkdownFile(file2, QStringLiteral("# Second File\n## Sub B\n"));

    TabManager tabs;

    // Records every activeDocumentChanged emission -- a plain connect() to
    // a lambda rather than QSignalSpy (Qt6::Test), which this project has
    // no other reason to depend on (see tests/core-lib/toc_model_test.cpp's
    // own note on avoiding a new test-framework dependency for the same
    // reason).
    std::vector<std::pair<TocModel*, DocumentView*>> emissions;
    QObject::connect(&tabs, &TabManager::activeDocumentChanged,
                      [&emissions](TocModel* tocModel, DocumentView* view) {
                          emissions.emplace_back(tocModel, view);
                      });

    // --- The bug: opening the very first tab ever must immediately
    // --- deliver a populated TocModel, not (nullptr, nullptr). Root cause:
    // --- QTabWidget::addTab() fires currentChanged() synchronously for the
    // --- very first tab added to an empty QTabWidget (index -1 -> 0 as
    // --- part of that same call) -- TabManager::openFile() used to insert
    // --- into its tracking map only *after* calling addTab(), so this
    // --- first-tab case ran handleCurrentChanged() against an empty map
    // --- and emitted (nullptr, nullptr). Every later tab was unaffected,
    // --- since only the later setCurrentIndex() call (by then after the
    // --- map insert) changes the index for those -- which is exactly why
    // --- switching away and back "fixed" it for the user who reported this.
    tabs.openFile(path1);
    check(!emissions.empty(), "opening the first-ever tab emits activeDocumentChanged at least once");
    if (!emissions.empty()) {
        auto [tocModel, view] = emissions.back();
        check(tocModel != nullptr, "first tab: activeDocumentChanged's TocModel is not null");
        check(view != nullptr, "first tab: activeDocumentChanged's DocumentView is not null");
        check(tocModel && tocModel->hasHeadings(),
              "first tab: TocModel already has headings on the very first signal");
    }
    emissions.clear();

    // --- A second tab must deliver its own, different, populated TocModel ---
    tabs.openFile(path2);
    check(!emissions.empty(), "opening a second tab emits activeDocumentChanged");
    TocModel* secondTabToc = nullptr;
    if (!emissions.empty()) {
        secondTabToc = emissions.back().first;
        check(secondTabToc != nullptr, "second tab: TocModel is not null");
        check(secondTabToc && secondTabToc->hasHeadings(), "second tab: TocModel has headings");
    }
    emissions.clear();

    // --- Switching back to the first tab must re-deliver *that* tab's own
    // --- TocModel (not the second tab's, not null) ---
    tabs.setCurrentIndex(0);
    check(!emissions.empty(), "switching back to the first tab emits activeDocumentChanged");
    if (!emissions.empty()) {
        TocModel* tocModel = emissions.back().first;
        check(tocModel != nullptr, "switching back: TocModel is not null");
        check(tocModel != secondTabToc, "switching back: TocModel is the first tab's, not the second tab's");
        check(tocModel && tocModel->hasHeadings(), "switching back: TocModel still has its headings");
    }
}

void runScrollToHeadingLandsAtTopTest() {
    // The bug: clicking a TOC entry (or an in-document anchor link) scrolled
    // to the right heading, but left it at the *bottom* edge of the
    // viewport rather than the top. Root cause: DocumentView used to call
    // QTextEdit::ensureCursorVisible(), which scrolls the *minimum* distance
    // needed to bring the cursor into view -- for a heading below what was
    // currently shown (the common case), that lands it right at the bottom
    // edge. Fixed by scrolling the vertical scrollbar directly to the
    // target block's own document-layout position, confirmed empirically
    // (a standalone probe) to land the cursor at the top of the viewport
    // instead.
    QTemporaryFile file;
    QString markdown = QStringLiteral("# Heading 1\n\n");
    for (int section = 2; section <= 6; ++section) {
        markdown += QStringLiteral("## Heading %1\n\n").arg(section);
        for (int line = 0; line < 20; ++line) {
            markdown += QStringLiteral("Paragraph line %1 for section %2.\n\n").arg(line).arg(section);
        }
    }
    const QString path = writeTempMarkdownFile(file, markdown);

    DocumentModel model;
    check(model.loadFromFile(path) == markdawn::core::LoadResult::Ok, "scroll test: temp file loads");

    DocumentView view;
    view.resize(400, 300);
    view.show();
    view.loadFromModel(&model);
    QApplication::processEvents();

    view.scrollToHeadingSlug(QStringLiteral("heading-5"));
    QApplication::processEvents();

    // Locate the same block again to compute where its top landed relative
    // to the viewport, the same way the probe that found this bug did.
    QTextBlock targetBlock;
    for (QTextBlock b = view.document()->begin(); b.isValid(); b = b.next()) {
        if (b.blockFormat().headingLevel() > 0 && b.text() == QStringLiteral("Heading 5")) {
            targetBlock = b;
            break;
        }
    }
    check(targetBlock.isValid(), "scroll test: target heading block found");
    if (targetBlock.isValid()) {
        const QRect rect = view.cursorRect(QTextCursor(targetBlock));
        check(rect.top() >= 0 && rect.top() <= 10,
              "scroll test: target heading lands at the top of the viewport, not the bottom");
    }
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    runTabManagerFirstTabTest();
    runScrollToHeadingLandsAtTopTest();

    if (g_failures == 0) {
        std::cout << "All widget integration tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " widget integration test(s) failed.\n";
    return 1;
}
