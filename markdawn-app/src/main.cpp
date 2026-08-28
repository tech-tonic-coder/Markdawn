#include <QApplication>
#include <QMainWindow>

#include "markdawncore/version.h"

// Phase 0 deliverable: an empty window that builds and runs on all three
// platforms. Single-instance handoff, MainWindow structure, tabs, etc. are
// Phase 1+.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Markdawn"));
    window.resize(800, 600);
    window.show();

    // Proves markdawncore actually links; remove once Phase 1 gives this a
    // real caller.
    (void)markdawn::core::scaffoldingMarker();

    return app.exec();
}
