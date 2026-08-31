#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QString>

#include "main_window/main_window.h"
#include "markdawncore/ipc/single_instance_coordinator.h"
#include "tab_manager/tab_manager.h"

using markdawn::app::MainWindow;
using markdawn::app::TabManager;
using markdawn::core::SingleInstanceServer;
using markdawn::core::tryForwardToRunningInstance;

namespace {

// `markdawn <path-to-file.md>` is the invocation contract the OS uses on
// double-click (§5 Phase 1). A full QCommandLineParser arrives once real
// flags exist (Phase 5+); for now the first non-flag argument is the path.
QString firstFileArgument(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith(QLatin1Char('-'))) {
            return arg;
        }
    }
    return {};
}

} // namespace

int main(int argc, char* argv[]) {
    QString filePath = firstFileArgument(argc, argv);

    // Resolve to an absolute path immediately, before the fast-forward
    // check. A relative path is meaningless once it crosses a process
    // boundary: the IPC message (§5 Phase 1) is sent by this process but
    // opened by whichever instance ends up owning the window, which may
    // have a different working directory than this one (roadmap §6.2
    // Phase 2 log -- this was a real gap in Phase 1's IPC path once file
    // opening became real in Phase 2, not a hypothetical one).
    if (!filePath.isEmpty()) {
        filePath = QFileInfo(filePath).absoluteFilePath();
    }

    // Fast path (§5 Phase 1: "no QApplication construction, no
    // theme/resource loading, just connect, send, exit"): a QCoreApplication
    // is enough to use QLocalSocket, and is destroyed at the end of this
    // scope -- before QApplication is ever constructed -- if it turns out a
    // running instance already exists.
    {
        QCoreApplication probe(argc, argv);
        qInfo() << "markdawn: checking for a running instance";
        if (tryForwardToRunningInstance(filePath)) {
            return 0;
        }
    }

    QApplication app(argc, argv);

    MainWindow window;

    SingleInstanceServer server;
    if (!server.start()) {
        qWarning() << "markdawn: single-instance server did not start;"
                   << "later launches may open extra windows until this one is closed.";
    }
    // Direct connection to TabManager, replacing Phase 1's log-only
    // placeholder lambda (roadmap §6.2 Phase 1 log: "Phase 2's TabManager
    // should connect to SingleInstanceServer::openFileRequested in place of
    // the placeholder lambda in main.cpp").
    QObject::connect(&server, &SingleInstanceServer::openFileRequested, window.tabManager(),
                      &TabManager::openFile);

    if (!filePath.isEmpty()) {
        window.tabManager()->openFile(filePath);
    }

    window.show();
    return app.exec();
}
