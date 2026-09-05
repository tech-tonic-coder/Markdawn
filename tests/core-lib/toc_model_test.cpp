// Headless unit test for TocModel (§5 Phase 3's required [Auto] check:
// "Feed a document with nested headings (H1/H2/H3) into TocModel directly
// and confirm the resulting hierarchy is correct"). No GUI, no platform
// plugin: a bare QCoreApplication is sufficient for QTextDocument's
// Markdown parsing and QAbstractItemModel's signal machinery -- verified
// directly before writing this file (roadmap §6.2 Phase 3 log), not
// assumed.
//
// Plain assert-and-count-failures style rather than a QtTest-based test:
// no new dependency is needed (Qt6::Test is not currently in vcpkg.json),
// and CTest already treats a non-zero exit code as a failing test, which is
// all this needs.

#include <QCoreApplication>
#include <QModelIndex>
#include <iostream>

#include "markdawncore/document/document_model.h"
#include "markdawncore/document/toc_model.h"

using markdawn::core::DocumentModel;
using markdawn::core::kTocLevelRole;
using markdawn::core::kTocSlugRole;
using markdawn::core::TocModel;

namespace {

int g_failures = 0;

void check(bool condition, const QString& description) {
    if (!condition) {
        std::cerr << "FAIL: " << description.toStdString() << "\n";
        ++g_failures;
    }
}

QString textAt(const TocModel& model, int row, const QModelIndex& parent = {}) {
    return model.data(model.index(row, 0, parent), Qt::DisplayRole).toString();
}

int levelAt(const TocModel& model, int row, const QModelIndex& parent = {}) {
    return model.data(model.index(row, 0, parent), kTocLevelRole).toInt();
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // --- Nested H1/H2/H3 hierarchy: the roadmap's required case ---
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText(
            "# Intro\n"
            "text\n"
            "## Background\n"
            "text\n"
            "### Details\n"
            "text\n"
            "## Approach\n"
            "text\n"
            "# Conclusion\n"
            "text\n");

        TocModel toc;
        toc.setDocumentModel(&doc);

        check(toc.hasHeadings(), "nested doc: hasHeadings() is true");
        check(toc.rowCount() == 2, "nested doc: 2 top-level headings");
        check(textAt(toc, 0) == "Intro", "nested doc: row 0 is Intro");
        check(levelAt(toc, 0) == 1, "nested doc: Intro is level 1");
        check(textAt(toc, 1) == "Conclusion", "nested doc: row 1 is Conclusion");

        const QModelIndex introIndex = toc.index(0, 0);
        check(toc.rowCount(introIndex) == 2, "nested doc: Intro has 2 children");
        check(textAt(toc, 0, introIndex) == "Background", "nested doc: Intro child 0 is Background");
        check(textAt(toc, 1, introIndex) == "Approach", "nested doc: Intro child 1 is Approach");

        const QModelIndex backgroundIndex = toc.index(0, 0, introIndex);
        check(toc.rowCount(backgroundIndex) == 1, "nested doc: Background has 1 child");
        check(textAt(toc, 0, backgroundIndex) == "Details", "nested doc: Background child 0 is Details");
        check(levelAt(toc, 0, backgroundIndex) == 3, "nested doc: Details is level 3");

        // parent() round-trips back to the exact index it came from.
        const QModelIndex detailsIndex = toc.index(0, 0, backgroundIndex);
        check(toc.parent(detailsIndex) == backgroundIndex, "nested doc: Details.parent() == Background");
        check(toc.parent(backgroundIndex) == introIndex, "nested doc: Background.parent() == Intro");
        check(!toc.parent(introIndex).isValid(), "nested doc: Intro.parent() is the invalid root");

        // Slug matches the shared slugifyHeading() algorithm DocumentView uses.
        check(toc.data(backgroundIndex, kTocSlugRole).toString() == "background",
              "nested doc: Background's slug is \"background\"");
    }

    // --- Level skip: H1 -> H3 with no H2 attaches directly, no synthesized node ---
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText("# Top\ntext\n### Deep\ntext\n");
        TocModel toc;
        toc.setDocumentModel(&doc);

        check(toc.rowCount() == 1, "level-skip doc: 1 top-level heading");
        const QModelIndex topIndex = toc.index(0, 0);
        check(toc.rowCount(topIndex) == 1, "level-skip doc: Top has 1 child");
        check(textAt(toc, 0, topIndex) == "Deep", "level-skip doc: Top's child is Deep");
        check(levelAt(toc, 0, topIndex) == 3, "level-skip doc: Deep is level 3");
    }

    // --- No headings at all: explicit empty state, no crash ---
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText("Just a paragraph.\n\nAnother paragraph.\n");
        TocModel toc;
        toc.setDocumentModel(&doc);

        check(!toc.hasHeadings(), "headless doc: hasHeadings() is false");
        check(toc.rowCount() == 0, "headless doc: rowCount() is 0");
    }

    // --- Inline markdown formatting is stripped from heading text/slug ---
    // (This is *why* TocModel runs its own setMarkdown() pass instead of
    // scanning raw "#" characters -- see toc_model.h's class comment.)
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText("# **Bold** and `code` heading\ntext\n");
        TocModel toc;
        toc.setDocumentModel(&doc);

        check(textAt(toc, 0) == "Bold and code heading",
              "inline-formatted doc: heading text has markdown syntax stripped");
        check(toc.data(toc.index(0, 0), kTocSlugRole).toString() == "bold-and-code-heading",
              "inline-formatted doc: slug matches the stripped text");
    }

    // --- setDocumentModel(nullptr) clears back to the empty state ---
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText("# Only Heading\n");
        TocModel toc;
        toc.setDocumentModel(&doc);
        check(toc.hasHeadings(), "clear test: has headings before clearing");

        toc.setDocumentModel(nullptr);
        check(!toc.hasHeadings(), "clear test: hasHeadings() false after setDocumentModel(nullptr)");
        check(toc.rowCount() == 0, "clear test: rowCount() 0 after setDocumentModel(nullptr)");
    }

    // --- Live edits rebuild the tree via contentsChanged, never polled ---
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText("# Original\n");
        TocModel toc;
        toc.setDocumentModel(&doc);
        check(toc.rowCount() == 1 && textAt(toc, 0) == "Original", "live-edit doc: initial heading present");

        doc.textDocument()->setPlainText("# Changed\n## New Sub\n");
        check(toc.rowCount() == 1 && textAt(toc, 0) == "Changed",
              "live-edit doc: rebuilt after textDocument() content changed");
        check(toc.rowCount(toc.index(0, 0)) == 1, "live-edit doc: new child picked up after edit");
    }

    // --- Duplicate headings get disambiguated slugs, matching GitHub's
    // --- own "-1", "-2", ... behavior (added after comparing against a
    // --- reference implementation; see roadmap §6.2 Phase 3 log) ---
    {
        DocumentModel doc;
        doc.textDocument()->setPlainText(
            "# Overview\n"
            "text\n"
            "## Background\n"
            "text\n"
            "## Background\n"
            "text\n"
            "## Background\n"
            "text\n");
        TocModel toc;
        toc.setDocumentModel(&doc);

        const QModelIndex overviewIndex = toc.index(0, 0);
        check(toc.rowCount(overviewIndex) == 3, "duplicate-heading doc: 3 Background children");
        check(toc.data(toc.index(0, 0, overviewIndex), kTocSlugRole).toString() == "background",
              "duplicate-heading doc: 1st Background keeps the plain slug");
        check(toc.data(toc.index(1, 0, overviewIndex), kTocSlugRole).toString() == "background-1",
              "duplicate-heading doc: 2nd Background gets \"-1\"");
        check(toc.data(toc.index(2, 0, overviewIndex), kTocSlugRole).toString() == "background-2",
              "duplicate-heading doc: 3rd Background gets \"-2\"");
    }

    if (g_failures == 0) {
        std::cout << "All TocModel tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " TocModel test(s) failed.\n";
    return 1;
}
