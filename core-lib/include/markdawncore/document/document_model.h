#pragma once

#include <QObject>
#include <QString>
#include <QTextDocument>

namespace markdawn::core {

enum class LoadResult {
    Ok,
    FileNotFound,
    ReadError,
};

// Thin wrapper around QTextDocument that owns loading a file's raw text
// into it (§5 Phase 1). Markdown rendering (Phase 2) and syntax
// highlighting (Phase 8) are layered on top of this by the view widgets —
// this class only ever deals in plain text, never parses Markdown itself
// (§3's rule: a view widget, not the model, does the parsing).
class DocumentModel : public QObject {
    Q_OBJECT
public:
    explicit DocumentModel(QObject* parent = nullptr);

    // Reads filePath as UTF-8 text and loads it into the underlying
    // QTextDocument as plain text. On failure, the document and filePath()
    // are left unchanged from before the call.
    LoadResult loadFromFile(const QString& filePath);

    QTextDocument* textDocument() { return &m_document; }
    const QString& filePath() const { return m_filePath; }

private:
    QTextDocument m_document;
    QString m_filePath;
};

} // namespace markdawn::core
