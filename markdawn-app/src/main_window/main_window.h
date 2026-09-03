#pragma once

#include <QMainWindow>

namespace markdawn::app {

class TabManager;
class TocPanel;

// Owns the tab manager, the TOC dock panel (§5 Phase 3), the File > Open
// action, and drag-and-drop handling (§5 Phase 2). All three ways of
// opening a file funnel into TabManager::openFile() -- this class does no
// file I/O, Markdown handling, or heading extraction itself; it only wires
// TabManager's activeDocumentChanged straight through to TocPanel.
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
    TocPanel* m_tocPanel = nullptr;
};

} // namespace markdawn::app
