#pragma once

#include <QModelIndex>
#include <QWidget>

class QStackedWidget;
class QTreeView;
class QLabel;

namespace markdawn::core {
class TocModel;
} // namespace markdawn::core

namespace markdawn::app {

class DocumentView;

// Docked panel (§5 Phase 3) showing the active tab's heading outline as a
// tree; clicking a node scrolls that tab's DocumentView to the heading.
// Owns no heading-extraction logic of its own -- TocModel (core-lib)
// already does that; this class only displays whichever TocModel
// TabManager says is currently active and forwards clicks back to that
// tab's DocumentView. Three distinct, explicit states (§5 Phase 3
// requirement: a document with no headings must not look like "not loaded
// yet"): no tab open, active tab with zero headings, active tab with a
// tree to show.
class TocPanel : public QWidget {
    Q_OBJECT
public:
    explicit TocPanel(QWidget* parent = nullptr);

public slots:
    // Connected to TabManager::activeDocumentChanged. Only retargets which
    // model/view this panel points at -- never rebuilds or touches
    // TocModel's own content, so switching tabs never touches a
    // background tab's model (§5 Phase 3 requirement). Both null means "no
    // tab open".
    void setActiveDocument(markdawn::core::TocModel* tocModel, DocumentView* view);

private slots:
    void handleNodeClicked(const QModelIndex& index);
    void updateEmptyState();

private:
    QStackedWidget* m_stack = nullptr;
    QLabel* m_noTabLabel = nullptr;
    QLabel* m_noHeadingsLabel = nullptr;
    QTreeView* m_treeView = nullptr;

    markdawn::core::TocModel* m_activeModel = nullptr;
    DocumentView* m_activeView = nullptr;
};

} // namespace markdawn::app
