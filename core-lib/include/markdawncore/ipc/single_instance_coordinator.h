#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace markdawn::core {

// Local server/socket name for the handoff. Not user-facing; QLocalServer
// maps this to a Unix domain socket under the temp dir on Linux/macOS and
// to a named pipe on Windows — see single_instance_coordinator.cpp.
inline constexpr auto kSingleInstanceServerName = "markdawn-single-instance";

// Attempts to connect to an already-running instance and hand off
// `filePath` (may be empty, e.g. when launched with no arguments).
// Returns true if a running instance accepted the handoff — the caller
// should exit immediately without ever constructing QApplication. Returns
// false if no instance is running, meaning this process should become the
// primary instance. Requires a QCoreApplication to already exist.
bool tryForwardToRunningInstance(const QString& filePath);

// Owned by the primary instance once it has confirmed (via
// tryForwardToRunningInstance() returning false) that it should become the
// primary instance. Listens for handoff connections from later launches and
// re-emits each valid one as openFileRequested().
class SingleInstanceServer : public QObject {
    Q_OBJECT
public:
    explicit SingleInstanceServer(QObject* parent = nullptr);

    // Starts listening. Recovers from a stale registration left behind by a
    // crashed previous instance (see .cpp). Returns false on failure, in
    // which case this process still runs as a normal window, just without
    // being reachable by later launches — logged, not fatal.
    bool start();

signals:
    void openFileRequested(const QString& filePath);

private:
    void handleNewConnection();

    QLocalServer* m_server = nullptr;
};

} // namespace markdawn::core
