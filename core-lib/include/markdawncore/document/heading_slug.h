#pragma once

#include <QHash>
#include <QString>

namespace markdawn::core {

// Approximates GitHub's heading-slug algorithm: lowercase; spaces become
// hyphens; anything that isn't a letter, digit, hyphen or underscore is
// dropped. Shared by DocumentView (markdawn-app, click-to-scroll lookup)
// and TocModel (§5 Phase 3, TOC-node identity) so both always compute the
// identical slug for identical heading text -- moved here from
// document_view.cpp in Phase 3 for exactly that reason (roadmap §6.2 Phase
// 3 log). This function alone does not disambiguate duplicate headings --
// see HeadingSlugDisambiguator below for that.
QString slugifyHeading(const QString& headingText);

// Disambiguates repeated heading slugs across one top-to-bottom scan of a
// document's headings, mirroring GitHub's own behavior exactly (verified
// against a reference implementation, not assumed): the first heading with
// a given base slug keeps it as-is; the second occurrence of that same
// base slug gets "-1" appended, the third "-2", and so on.
//
// A fresh instance must be constructed for each full scan (DocumentView's
// rebuildHeadingIndex(), TocModel's rebuild()) -- it is not meant to
// persist across separate scans. That is not a shortcut: both of those
// call sites already discard and rebuild their heading data wholesale on
// every load/edit rather than updating it incrementally, so a fresh
// disambiguator naturally produces the same result a persistent one
// re-fed from scratch would, with no extra state to manage.
//
// Without this, two identically-worded headings in one document would
// collide on the same slug, and clicking a TOC entry or an in-document
// anchor link for either one would land on whichever heading happened to
// be inserted last into the lookup map -- a real, user-visible bug this
// was written to fix (roadmap §6.2 Phase 3 log has the full reasoning).
class HeadingSlugDisambiguator {
public:
    // Feed headings in document order; returns the disambiguated slug for
    // each. DocumentView and TocModel both walk their (separately rendered,
    // but content-identical -- see toc_model.h) documents top to bottom, so
    // feeding headings through this in that same order guarantees both
    // classes land on the exact same disambiguated slugs for the exact
    // same document.
    QString nextSlug(const QString& headingText);

private:
    QHash<QString, int> m_seenCounts;
};

} // namespace markdawn::core
