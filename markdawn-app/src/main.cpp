#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QMainWindow>
#include <QString>

#include "markdawncore/document/document_model.h"
#include "markdawncore/ipc/single_instance_coordinator.h"

using markdawn::core::DocumentModel;
using markdawn::core::LoadResult;
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
    const QString filePath = firstFileArgument(argc, argv);

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

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Markdawn"));
    window.resize(800, 600);

    SingleInstanceServer server;
    if (!server.start()) {
        qWarning() << "markdawn: single-instance server did not start;"
                   << "later launches may open extra windows until this one is closed.";
    }
    QObject::connect(&server, &SingleInstanceServer::openFileRequested, &window,
                      [](const QString& path) {
                          // Phase 2's TabManager consumes this signal for real; Phase 1
                          // only needs to prove the message arrives (§5's "How to verify").
                          qInfo() << "markdawn: OpenFile received via IPC for" << path;
                      });

    if (!filePath.isEmpty()) {
        auto* model = new DocumentModel(&window);
        const LoadResult result = model->loadFromFile(filePath);
        if (result == LoadResult::Ok) {
            qInfo() << "markdawn: loaded" << filePath << "(" << model->textDocument()->characterCount()
                   << "characters )";
        } else {
            qWarning() << "markdawn: failed to load" << filePath;
        }
    }

    window.show();
    return app.exec();
}
