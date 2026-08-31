#pragma once

#include <QMap>
#include <QString>
#include <QTabWidget>

namespace markdawn::app {

// One tab per open file, each owning a DocumentModel + DocumentView pair
// (§5 Phase 2). openFile() is the single "open this path as a tab" entry
// point shared by File > Open, drag-and-drop, and Phase 1's IPC handoff --
// none of them talk to DocumentModel/DocumentView directly.
class TabManager : public QTabWidget {
    Q_OBJECT
public:
    explicit TabManager(QWidget* parent = nullptr);

public slots:
    // Opens filePath as a new tab, or focuses its tab if already open
    // (compared by absolute path, so "notes.md" and "./notes.md" from
    // different working directories are recognized as the same file).
    // Connected directly to SingleInstanceServer::openFileRequested (per
    // roadmap §6.2 Phase 1 log) and called directly from MainWindow's
    // File > Open action and drop handler -- all three routes are this one
    // function, per §5 Phase 2's requirement.
    void openFile(const QString& filePath);

private slots:
    void closeTabAt(int index);

private:
    // Keyed by absolute path rather than tab index, which would be
    // invalidated by closing/reordering tabs.
    QMap<QString, QWidget*> m_openPaths;
};

} // namespace markdawn::app
