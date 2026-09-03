#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <QTextDocument>
#include <memory>

namespace markdawn::core {

class DocumentModel;

// Custom data role exposing a heading node's GitHub-style slug
// (heading_slug.h), used by the TOC panel (markdawn-app) to drive
// DocumentView::scrollToHeadingSlug() on click (§5 Phase 3).
inline constexpr int kTocSlugRole = Qt::UserRole + 1;
// Custom data role exposing the raw heading level (1-6). Unused by the
// tree view itself for now; kept so a future indent/style-by-level need
// (e.g. Phase 4 theming) doesn't require another model round of changes.
inline constexpr int kTocLevelRole = Qt::UserRole + 2;

// Builds a hierarchical outline of a DocumentModel's headings for the TOC
// panel (§5 Phase 3).
//
// Deliberately does NOT walk the DocumentModel's own QTextDocument's block
// formats directly -- that document is kept as plain text on purpose (§2.2,
// roadmap §6.2 Phase 1 log: "parsing is a view concern, never the model's"),
// so its blocks never carry a heading level. Instead this class keeps a
// private, never-displayed QTextDocument and runs the exact same
// QTextDocument::setMarkdown() call DocumentView (Phase 2) already uses on
// the same source text, then reads heading level/text off *that* copy's
// blockFormat().headingLevel(). This is a deliberate decision, not an
// implementation detail (roadmap §6.2 Phase 3 log has the full reasoning):
// it reuses Qt's own already-verified Markdown parser instead of a
// hand-rolled ATX/setext heading scanner (§4.9), and -- verified directly,
// not assumed -- it guarantees the heading text collected here is
// byte-identical to what DocumentView renders (inline formatting like
// "**bold**" or "`code`" is stripped from the block text by Qt's Markdown
// importer either way), which in turn guarantees slugifyHeading()
// (heading_slug.h) produces the exact same slug DocumentView's own
// click-to-scroll lookup uses.
//
// Reacts only to DocumentModel's QTextDocument::contentsChanged signal
// (§4.7: event-driven, never polled) -- there is no timer anywhere in this
// class.
class TocModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit TocModel(QObject* parent = nullptr);
    ~TocModel() override;

    // Points this model at documentModel, or at nullptr to clear it back to
    // the empty state. Disconnects from any previously-set DocumentModel
    // first. Rebuilds immediately, then again every time the DocumentModel's
    // QTextDocument reports a real content change.
    void setDocumentModel(DocumentModel* documentModel);

    // True once a rebuild has run and found at least one heading. Lets the
    // TOC panel (markdawn-app) distinguish "active tab, no headings" from
    // "no active tab at all" without inferring it from rowCount() itself.
    bool hasHeadings() const;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private slots:
    void rebuild();

private:
    struct Node;

    std::unique_ptr<Node> m_root;
    QTextDocument m_renderDocument; // private, never shown -- see class comment above
    DocumentModel* m_documentModel = nullptr;
};

} // namespace markdawn::core
