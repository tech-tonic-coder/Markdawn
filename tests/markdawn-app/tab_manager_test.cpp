// Regression test for a real, user-reported bug (not a hypothesized edge
// case): the very first tab ever opened showed an empty TOC until the user
// switched to another tab and back. Root cause: QTabWidget::addTab() fires
// currentChanged() synchronously for the very first tab ever added to an
// empty QTabWidget (its current index moves from -1 to 0 as part of that
// call, not a separate later one) -- but TabManager::openFile() only
// inserted the new tab into its tracking map *after* calling addTab(), so
// TabManager::handleCurrentChanged() ran against an empty map for that
// first tab and emitted activeDocumentChanged(nullptr, nullptr). Every
// subsequent tab was unaffected, because only setCurrentIndex() (called
// after the map insert) changes the current index for those -- which is
// exactly why the bug was invisible for the second tab onward and why
// switching back to the first tab "fixed" it (setCurrentIndex() to an
// already-populated map entry works correctly).
//
// TabManager/DocumentView are QWidget subclasses, so unlike
// tests/core-lib/toc_model_test.cpp's bare QCoreApplication, this needs a
// real QApplication -- run under the offscreen platform plugin in CI (see
// this test's CMakeLists.txt for the same static-triplet plugin-linkage
// requirement the real markdawn executable has).

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <iostream>
#include <vector>

#include "document_view/document_view.h"
#include "markdawncore/document/toc_model.h"
#include "tab_manager/tab_manager.h"

using markdawn::app::DocumentView;
using markdawn::app::TabManager;
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
    file.setFileTemplate(QDir::tempPath() + QStringLiteral("/tab_manager_test_XXXXXX.md"));
    check(file.open(), "temp file opens");
    QTextStream(&file) << markdownText;
    file.flush();
    return QFileInfo(file).absoluteFilePath();
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

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

    // --- The exact bug: opening the very first tab ever must immediately
    // --- deliver a populated TocModel, not (nullptr, nullptr) ---
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

    if (g_failures == 0) {
        std::cout << "All TabManager tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " TabManager test(s) failed.\n";
    return 1;
}
