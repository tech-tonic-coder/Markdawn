#include "main_window.h"

#include <QAction>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QUrl>

#include "tab_manager/tab_manager.h"
#include "toc_panel/toc_panel.h"
// Needed here, not just forward-declared in tab_manager.h: QObject::connect's
// pointer-to-member-function overload requires TocModel and DocumentView to
// be complete types at this call site (it needs QMetaType completeness for
// both, even though this ends up being a direct connection at runtime) --
// confirmed by the actual compiler error, not assumed up front.
#include "document_view/document_view.h"
#include "markdawncore/document/toc_model.h"

namespace markdawn::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Markdawn"));
    resize(800, 600);
    setAcceptDrops(true);

    m_tabManager = new TabManager(this);
    setCentralWidget(m_tabManager);

    // Table of Contents dock (§5 Phase 3). Left dock area is the
    // conventional placement for an outline/navigator panel; nothing in the
    // roadmap mandates a side, and QDockWidget lets the user move or float
    // it regardless.
    m_tocPanel = new TocPanel(this);
    auto* tocDock = new QDockWidget(tr("Table of Contents"), this);
    tocDock->setObjectName(QStringLiteral("tocDock"));
    tocDock->setWidget(m_tocPanel);
    addDockWidget(Qt::LeftDockWidgetArea, tocDock);
    connect(m_tabManager, &TabManager::activeDocumentChanged, m_tocPanel,
            &TocPanel::setActiveDocument);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFileDialog);
}

void MainWindow::openFileDialog() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Markdown File"), QString(),
        tr("Markdown Files (*.md *.markdown *.mkd);;All Files (*)"));
    if (!path.isEmpty()) {
        m_tabManager->openFile(path);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            m_tabManager->openFile(url.toLocalFile());
        }
    }
    event->acceptProposedAction();
}

} // namespace markdawn::app
