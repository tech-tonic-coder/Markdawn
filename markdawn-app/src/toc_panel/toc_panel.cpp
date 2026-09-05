#include "toc_panel.h"

#include <QAbstractItemModel>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>

#include "document_view/document_view.h"
#include "markdawncore/document/toc_model.h"

namespace markdawn::app {

TocPanel::TocPanel(QWidget* parent) : QWidget(parent) {
    m_noTabLabel = new QLabel(tr("Open a file to see its outline."), this);
    m_noTabLabel->setAlignment(Qt::AlignCenter);
    m_noTabLabel->setWordWrap(true);
    m_noTabLabel->setContentsMargins(8, 8, 8, 8);

    m_noHeadingsLabel = new QLabel(tr("This document has no headings."), this);
    m_noHeadingsLabel->setAlignment(Qt::AlignCenter);
    m_noHeadingsLabel->setWordWrap(true);
    m_noHeadingsLabel->setContentsMargins(8, 8, 8, 8);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    // The outline is always shown fully expanded (see updateEmptyState()'s
    // expandAll()) and never collapsed by the user, so the default +/-
    // branch indicator Qt draws for every heading that has sub-headings
    // serves no purpose here -- it only adds visual noise next to headings
    // that happen to have children. Removing it is Qt's own documented
    // technique for controlling branch decoration (Qt's "Customizing
    // QTreeView" docs use exactly this "branch { image: none; }" pattern),
    // not a fragile custom-paint workaround, and it's scoped to this one
    // QTreeView rather than a global stylesheet, so it doesn't affect any
    // other widget's native look.
    m_treeView->setStyleSheet(QStringLiteral("QTreeView::branch { border-image: none; image: none; }"));
    // Disabled to match: with the indicator gone, a still-active
    // double-click-to-collapse would silently hide a heading's children
    // with no visual cue why -- confusing rather than useful given the
    // tree is meant to always show the full outline.
    m_treeView->setItemsExpandable(false);
    // "Clicking a node scrolls..." (§5 Phase 3) -- clicked is the direct
    // single-click signal; activated is deliberately not also connected,
    // since on some styles it fires on the same single click and would
    // invoke the slot twice for one click.
    connect(m_treeView, &QTreeView::clicked, this, &TocPanel::handleNodeClicked);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_noTabLabel);      // index 0: no active tab
    m_stack->addWidget(m_noHeadingsLabel); // index 1: active tab, zero headings
    m_stack->addWidget(m_treeView);        // index 2: active tab, headings shown
    m_stack->setCurrentIndex(0);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
}

void TocPanel::setActiveDocument(markdawn::core::TocModel* tocModel, DocumentView* view) {
    if (m_activeModel) {
        disconnect(m_activeModel, nullptr, this, nullptr);
    }
    m_activeModel = tocModel;
    m_activeView = view;

    m_treeView->setModel(tocModel);
    if (tocModel) {
        // TocModel resets wholesale on every rebuild (roadmap §6.2 Phase 3
        // log); modelReset is QAbstractItemModel's own signal for exactly
        // that case, so no bespoke signal is needed on TocModel itself.
        connect(tocModel, &QAbstractItemModel::modelReset, this, &TocPanel::updateEmptyState);
    }
    updateEmptyState();
}

void TocPanel::updateEmptyState() {
    if (!m_activeModel) {
        m_stack->setCurrentIndex(0);
        return;
    }
    if (!m_activeModel->hasHeadings()) {
        m_stack->setCurrentIndex(1);
        return;
    }
    m_treeView->expandAll();
    m_stack->setCurrentIndex(2);
}

void TocPanel::handleNodeClicked(const QModelIndex& index) {
    if (!index.isValid() || !m_activeView) {
        return;
    }
    const QString slug = index.data(markdawn::core::kTocSlugRole).toString();
    m_activeView->scrollToHeadingSlug(slug);
}

} // namespace markdawn::app
