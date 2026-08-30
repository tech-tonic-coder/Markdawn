#include "markdawncore/document/document_model.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace markdawn::core {

DocumentModel::DocumentModel(QObject* parent) : QObject(parent) {}

LoadResult DocumentModel::loadFromFile(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        qWarning() << "markdawn: file not found:" << filePath;
        return LoadResult::FileNotFound;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "markdawn: could not open file:" << filePath << "-" << file.errorString();
        return LoadResult::ReadError;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8); // already Qt6's default; explicit for clarity
    m_document.setPlainText(stream.readAll());
    m_filePath = filePath;
    return LoadResult::Ok;
}

} // namespace markdawn::core
