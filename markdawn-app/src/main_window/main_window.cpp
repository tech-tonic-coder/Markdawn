#include "main_window.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QUrl>

#include "tab_manager/tab_manager.h"

namespace markdawn::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Markdawn"));
    resize(800, 600);
    setAcceptDrops(true);

    m_tabManager = new TabManager(this);
    setCentralWidget(m_tabManager);

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
