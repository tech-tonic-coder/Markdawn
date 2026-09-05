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

QString HeadingSlugDisambiguator::nextSlug(const QString& headingText) {
    const QString base = slugifyHeading(headingText);
    int& count = m_seenCounts[base];
    const QString result =
        (count == 0) ? base : (base + QLatin1Char('-') + QString::number(count));
    ++count;
    return result;
}

} // namespace markdawn::core
