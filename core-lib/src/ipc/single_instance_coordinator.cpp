#include "markdawncore/ipc/single_instance_coordinator.h"
#include "markdawncore/ipc/ipc_message.h"

#include <QDebug>
#include <QLatin1String>
#include <QLocalServer>
#include <QLocalSocket>

namespace markdawn::core {

namespace {

// Generous but not user-visible: this only covers a local IPC round trip on
// the same machine, never a network hop, so a slow reply means something is
// actually wrong rather than "the network is slow".
constexpr int kConnectTimeoutMs = 200;
constexpr int kWriteTimeoutMs = 200;

} // namespace

bool tryForwardToRunningInstance(const QString& filePath) {
    QLocalSocket socket;
    socket.connectToServer(QLatin1String(kSingleInstanceServerName));
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        // No running instance (or nothing accepted within the timeout) —
        // this process should become the primary instance.
        return false;
    }

    const OpenFileMessage message{kOpenFileProtocolVersion, filePath.toStdString()};
    QByteArray payload = QByteArray::fromStdString(serializeOpenFile(message));
    payload.append('\n'); // frames the message for the server's readyRead handler

    socket.write(payload);
    if (!socket.waitForBytesWritten(kWriteTimeoutMs)) {
        // We know a server is genuinely alive (connect succeeded) — treat
        // this as "forwarding failed" rather than falling through to also
        // start a server ourselves, which risks two instances contending
        // for the same socket name.
        qWarning() << "markdawn: connected to running instance but could not send the file path;"
                   << "the running window was not updated.";
        return true;
    }

    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) {
        socket.waitForDisconnected(kWriteTimeoutMs);
    }

    qInfo() << "markdawn: forwarded" << filePath << "to the running instance";
    return true;
}

SingleInstanceServer::SingleInstanceServer(QObject* parent) : QObject(parent) {}

bool SingleInstanceServer::start() {
    m_server = new QLocalServer(this);

    // Qt's documented recovery for a crashed previous instance: "On Unix if
    // the server crashes without closing, listen() will fail with
    // AddressInUseError." removeServer() is a no-op on Windows and, on
    // Unix, only deletes the socket file if one is present — safe to call
    // unconditionally on every startup rather than reacting to the error.
    QLocalServer::removeServer(QLatin1String(kSingleInstanceServerName));

    if (!m_server->listen(QLatin1String(kSingleInstanceServerName))) {
        qWarning() << "markdawn: failed to start single-instance server:" << m_server->errorString();
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceServer::handleNewConnection);
    qInfo() << "markdawn: single-instance server listening as" << kSingleInstanceServerName;
    return true;
}

void SingleInstanceServer::handleNewConnection() {
    while (m_server->hasPendingConnections()) {
        QLocalSocket* socket = m_server->nextPendingConnection();

        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QByteArray data = socket->readAll();
            const auto message = parseOpenFile(data.trimmed().toStdString());
            if (message) {
                qInfo() << "markdawn: received OpenFile via IPC:" << QString::fromStdString(message->path);
                emit openFileRequested(QString::fromStdString(message->path));
            } else {
                qWarning() << "markdawn: received a malformed IPC message, ignoring";
            }
        });
        // The client disconnects right after writing (see
        // tryForwardToRunningInstance); clean up the socket once it does.
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }
}

} // namespace markdawn::core
