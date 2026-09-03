#include "document_view.h"

#include <QDebug>
#include <QDesktopServices>
#include <QTextCursor>
#include <QTextDocument>

#include "markdawncore/document/heading_slug.h"

namespace markdawn::app {

DocumentView::DocumentView(QWidget* parent) : QTextBrowser(parent) {
    // We handle every link ourselves (see handleAnchorClicked) rather than
    // letting QTextBrowser's own setSource()-based navigation run: this
    // content was loaded via setMarkdown(), never setSource(), so letting
    // Qt's default navigation trigger a reload risks blanking the view for
    // no benefit (verified during Phase 2 development; see roadmap §6.2).
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &DocumentView::handleAnchorClicked);
}

void DocumentView::loadFromModel(markdawn::core::DocumentModel* model) {
    const QString rawText = model->textDocument()->toPlainText();
    const QUrl base = QUrl::fromLocalFile(model->filePath());
    document()->setBaseUrl(base);
    qInfo() << "markdawn: DocumentView base URL set to" << base.toString() << "for"
            << model->filePath();
    // No explicit MarkdownFeatures argument: the single-argument overload
    // already defaults to MarkdownDialectGitHub on every Qt6 version this
    // project has checked (5.14 through 6.11+), which is also exactly what
    // Phase 0.5's Spike A tested against -- passing it explicitly would add
    // a dependency on the two-argument overload's exact signature for zero
    // behavioral difference (verified: the two-argument overload does not
    // exist in Qt 6.4, only from a later 6.x -- see roadmap §6.2 Phase 2
    // log; this project's pinned vcpkg baseline resolves qtbase to 6.11.1,
    // which does have it, but there is no reason to depend on that).
    setMarkdown(rawText);
    rebuildHeadingIndex();
}

void DocumentView::showLoadError(markdawn::core::LoadResult result, const QString& filePath) {
    const QString reason = (result == markdawn::core::LoadResult::FileNotFound)
                                ? QStringLiteral("File not found")
                                : QStringLiteral("Could not read file");
    qWarning() << "markdawn: showing in-tab error state for" << filePath << "-" << reason;
    setHtml(QStringLiteral("<div style='padding:24px; color:#b00020;'>"
                            "<h2>Unable to open file</h2>"
                            "<p><b>%1:</b><br>%2</p>"
                            "</div>")
                .arg(reason, filePath.toHtmlEscaped()));
    m_headingSlugs.clear();
}

void DocumentView::rebuildHeadingIndex() {
    m_headingSlugs.clear();
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0) {
            m_headingSlugs.insert(markdawn::core::slugifyHeading(block.text()), block);
        }
    }
}

void DocumentView::scrollToHeadingSlug(const QString& slug) {
    const auto it = m_headingSlugs.constFind(slug);
    if (it == m_headingSlugs.constEnd() || !it.value().isValid()) {
        qWarning() << "markdawn: no heading found for slug" << slug;
        return;
    }
    QTextCursor cursor(it.value());
    setTextCursor(cursor);
    ensureCursorVisible();
}

void DocumentView::handleAnchorClicked(const QUrl& link) {
    if (!link.scheme().isEmpty()) {
        // http(s), mailto, etc. -- always open via the system handler, never
        // inside Markdawn (§5 Phase 2 requirement).
        QDesktopServices::openUrl(link);
        return;
    }

    if (link.path().isEmpty() && link.hasFragment()) {
        // A same-document heading link, e.g. "#some-heading".
        scrollToHeadingSlug(link.fragment());
        return;
    }

    // A relative link to another local file (with or without its own
    // fragment). Cross-file in-app navigation is out of §5 Phase 2's scope;
    // resolve it against this document's base URL and hand it to the OS,
    // the same as an external link (deliberate scope decision, roadmap
    // §6.2 Phase 2 log).
    const QUrl resolved = document()->baseUrl().resolved(link);
    QDesktopServices::openUrl(resolved);
}

} // namespace markdawn::app
