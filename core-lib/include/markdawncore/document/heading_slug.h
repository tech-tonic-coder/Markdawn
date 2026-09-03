#pragma once

#include <QString>

namespace markdawn::core {

// Approximates GitHub's heading-slug algorithm: lowercase; spaces become
// hyphens; anything that isn't a letter, digit, hyphen or underscore is
// dropped. Shared by DocumentView (markdawn-app, click-to-scroll lookup)
// and TocModel (§5 Phase 3, TOC-node identity) so both always compute the
// identical slug for identical heading text -- moved here from
// document_view.cpp in Phase 3 for exactly that reason (roadmap §6.2 Phase
// 3 log). Known, accepted limitation carried over from Phase 2: does not
// disambiguate duplicate headings the way GitHub appends "-1", "-2", etc.,
// and does not replicate GitHub's exact Unicode/emoji handling.
QString slugifyHeading(const QString& headingText);

} // namespace markdawn::core
