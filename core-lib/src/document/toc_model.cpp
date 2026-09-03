#include "markdawncore/document/toc_model.h"

#include <QTextBlock>
#include <algorithm>
#include <vector>

#include "markdawncore/document/document_model.h"
#include "markdawncore/document/heading_slug.h"

namespace markdawn::core {

// Root is a sentinel: level 0, never itself exposed as a QModelIndex (see
// index()/parent()). Top-level headings are its direct children. This is
// the same shape Qt's own "Simple Tree Model" example uses for a
// QAbstractItemModel-backed tree (§4.9: follow the documented reference
// shape rather than deriving the index()/parent() bookkeeping from
// scratch).
struct TocModel::Node {
    QString text;
    QString slug;
    int level = 0;
    Node* parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
};

TocModel::TocModel(QObject* parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<Node>()) {}

TocModel::~TocModel() = default;

void TocModel::setDocumentModel(DocumentModel* documentModel) {
    if (m_documentModel == documentModel) {
        return;
    }
    if (m_documentModel) {
        disconnect(m_documentModel->textDocument(), nullptr, this, nullptr);
    }
    m_documentModel = documentModel;
    if (m_documentModel) {
        // contentsChanged is the one signal QTextDocument guarantees on any
        // edit (§4.7: event-driven, never a polling timer). blockCountChanged
        // fires for a subset of the same edits, so connecting to it as well
        // would only risk a redundant second rebuild per edit, not add
        // coverage.
        connect(m_documentModel->textDocument(), &QTextDocument::contentsChanged, this,
                &TocModel::rebuild);
    }
    rebuild();
}

bool TocModel::hasHeadings() const {
    return !m_root->children.empty();
}

void TocModel::rebuild() {
    beginResetModel();
    m_root = std::make_unique<Node>();

    if (m_documentModel) {
        // Own private render pass -- deliberately not the DocumentModel's
        // own (plain-text) QTextDocument. See the class-level comment in
        // toc_model.h for why.
        m_renderDocument.setMarkdown(m_documentModel->textDocument()->toPlainText());

        // Stack-based flat-headings-to-tree construction: a heading of
        // level L becomes a child of the nearest preceding heading with a
        // strictly smaller level, or a root child if none exists yet
        // (standard shape for this problem; e.g. a document that jumps
        // straight from an H1 to an H3 attaches that H3 directly under the
        // H1 -- no synthesized placeholder H2 is invented, verified via a
        // standalone probe against this exact case before writing this).
        std::vector<Node*> stack{m_root.get()};
        for (QTextBlock block = m_renderDocument.begin(); block.isValid(); block = block.next()) {
            const int level = block.blockFormat().headingLevel();
            if (level <= 0) {
                continue;
            }
            while (stack.size() > 1 && stack.back()->level >= level) {
                stack.pop_back();
            }
            auto node = std::make_unique<Node>();
            node->text = block.text();
            node->slug = slugifyHeading(node->text);
            node->level = level;
            node->parent = stack.back();
            Node* raw = node.get();
            stack.back()->children.push_back(std::move(node));
            stack.push_back(raw);
        }
    }

    endResetModel();
}

QModelIndex TocModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) {
        return {};
    }
    Node* parentNode =
        parent.isValid() ? static_cast<Node*>(parent.internalPointer()) : m_root.get();
    if (row < 0 || static_cast<std::size_t>(row) >= parentNode->children.size()) {
        return {};
    }
    return createIndex(row, column, parentNode->children[static_cast<std::size_t>(row)].get());
}

QModelIndex TocModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) {
        return {};
    }
    Node* childNode = static_cast<Node*>(child.internalPointer());
    Node* parentNode = childNode->parent;
    if (parentNode == nullptr || parentNode == m_root.get()) {
        return {};
    }
    Node* grandparentNode = parentNode->parent;
    const auto& siblings = grandparentNode->children;
    const auto it =
        std::find_if(siblings.begin(), siblings.end(),
                      [parentNode](const std::unique_ptr<Node>& n) { return n.get() == parentNode; });
    const int row = static_cast<int>(std::distance(siblings.begin(), it));
    return createIndex(row, 0, parentNode);
}

int TocModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0) {
        return 0;
    }
    Node* parentNode =
        parent.isValid() ? static_cast<Node*>(parent.internalPointer()) : m_root.get();
    return static_cast<int>(parentNode->children.size());
}

int TocModel::columnCount(const QModelIndex& /*parent*/) const {
    return 1;
}

QVariant TocModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    const Node* node = static_cast<Node*>(index.internalPointer());
    switch (role) {
        case Qt::DisplayRole:
            return node->text;
        case kTocSlugRole:
            return node->slug;
        case kTocLevelRole:
            return node->level;
        default:
            return {};
    }
}

} // namespace markdawn::core
