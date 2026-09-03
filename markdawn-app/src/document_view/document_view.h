#pragma once

#include <QMap>
#include <QString>
#include <QTextBlock>
#include <QTextBrowser>
#include <QUrl>

#include "markdawncore/document/document_model.h"

namespace markdawn::app {

// Read-only Markdown renderer for a single tab (§5 Phase 2). Renders a
// markdawn::core::DocumentModel's raw text via QTextDocument::setMarkdown()
// (QTextBrowser-based). The model's own QTextDocument stays plain text
// (§2.2: parsing is a view concern, never the model's) -- this class owns
// the only Markdown-parsed QTextDocument for the tab; the model's document
// is read from (toPlainText()) but never modified here.
class DocumentView : public QTextBrowser {
    Q_OBJECT
public:
    explicit DocumentView(QWidget* parent = nullptr);

    // Renders model's current plain text as Markdown (GitHub dialect, per
    // Phase 0.5 Spike A's verified coverage) and sets the document's base
    // URL to the model's file so relative image paths resolve against the
    // file's own directory (§5 Phase 2 requirement -- "this does not work
    // by default"; see roadmap §6.2 Phase 2 log for the verified mechanism).
    void loadFromModel(markdawn::core::DocumentModel* model);

    // Replaces the view with a clear, styled in-tab error message instead
    // of a blank tab or a crash (§5 Phase 2 requirement). filePath is the
    // path that was attempted -- passed explicitly rather than read from
    // the model, since DocumentModel::loadFromFile() leaves filePath()
    // unchanged (empty) when loading fails.
    void showLoadError(markdawn::core::LoadResult result, const QString& filePath);

public slots:
    // Scrolls to the heading whose slug matches slug (see
    // markdawncore/document/heading_slug.h for how the slug is computed).
    // Public so the TOC panel (§5 Phase 3) can drive navigation through the
    // same lookup this class already builds for internal anchor links --
    // no separate mechanism needs writing later.
    void scrollToHeadingSlug(const QString& slug);

private slots:
    void handleAnchorClicked(const QUrl& link);

private:
    void rebuildHeadingIndex();

    // slug -> heading QTextBlock, rebuilt every time content is (re)rendered.
    // Gotcha (verified empirically, roadmap §6.2 Phase 2 log):
    // QTextDocument::setMarkdown() does not register named anchors for
    // headings -- its own generated href="#..." links have no matching
    // anchor target in the document -- so QTextBrowser::scrollToAnchor()
    // silently finds nothing for a markdown heading link. This map plus
    // QTextCursor-based navigation (setTextCursor + ensureCursorVisible) is
    // the verified-working replacement.
    QMap<QString, QTextBlock> m_headingSlugs;
};

} // namespace markdawn::app
