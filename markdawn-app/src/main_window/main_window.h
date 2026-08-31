#pragma once

#include <QMainWindow>

namespace markdawn::app {

class TabManager;

// Owns the tab manager, the File > Open action, and drag-and-drop handling
// (§5 Phase 2). All three ways of opening a file funnel into
// TabManager::openFile() -- this class does no file I/O or Markdown
// handling itself.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Exposed so main.cpp can connect SingleInstanceServer::openFileRequested
    // directly to the tab manager (per roadmap §6.2 Phase 1 log) without
    // MainWindow needing its own forwarding slot.
    TabManager* tabManager() const { return m_tabManager; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void openFileDialog();

private:
    TabManager* m_tabManager = nullptr;
};

} // namespace markdawn::app
