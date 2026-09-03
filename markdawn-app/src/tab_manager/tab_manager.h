#pragma once

#include <QMap>
#include <QString>
#include <QTabWidget>

namespace markdawn::core {
class TocModel;
} // namespace markdawn::core

namespace markdawn::app {

class DocumentView;

// One tab per open file, each owning a DocumentModel + DocumentView pair
// (§5 Phase 2), plus -- since Phase 3 -- a TocModel built from that same
// DocumentModel. openFile() is the single "open this path as a tab" entry
// point shared by File > Open, drag-and-drop, and Phase 1's IPC handoff --
// none of them talk to DocumentModel/DocumentView/TocModel directly.
//
// TocModel is owned here (by tab, i.e. by path), not by DocumentView: its
// whole point (§2.2) is to depend only on DocumentModel, so the same
// instance can keep being "reused unmodified" (roadmap §5 Phase 3) if a
// later phase ever swaps DocumentView out for an editor widget on the same
// tab. Owning it at the DocumentView level would tie its lifetime to
// whichever view widget happens to be showing right now, which is exactly
// what that reuse requirement rules out.
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

signals:
    // Emitted whenever the active tab changes, including to "no tabs open"
    // (both pointers null) -- wired straight through from QTabWidget's own
    // currentChanged, so it fires correctly for the very first tab opened
    // and after the last tab is closed without any extra bookkeeping here.
    // The TOC panel (§5 Phase 3) connects to this to swap which TocModel it
    // shows without ever touching a background tab's model.
    void activeDocumentChanged(markdawn::core::TocModel* tocModel, DocumentView* view);

private slots:
    void closeTabAt(int index);
    void handleCurrentChanged(int index);

private:
    struct OpenTab {
        DocumentView* view = nullptr;
        markdawn::core::TocModel* tocModel = nullptr;
    };

    // Keyed by absolute path rather than tab index, which would be
    // invalidated by closing/reordering tabs.
    QMap<QString, OpenTab> m_openTabs;
};

} // namespace markdawn::app
