#include "markdawncore/document/heading_slug.h"

namespace markdawn::core {

QString slugifyHeading(const QString& headingText) {
    QString out;
    out.reserve(headingText.size());
    for (const QChar& c : headingText.toLower()) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_')) {
            out += c;
        } else if (c.isSpace()) {
            out += QLatin1Char('-');
        }
    }
    return out;
}

} // namespace markdawn::core
