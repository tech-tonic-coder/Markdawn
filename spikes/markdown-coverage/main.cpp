// Phase 0.5, Spike A — Qt6 Markdown coverage audit (Markdawn-Roadmap.md §5).
//
// This is a throwaway diagnostic tool, not shipped product code. It feeds
// one CommonMark/GFM construct at a time through QTextDocument::setMarkdown()
// (default features = MarkdownDialectGitHub, the same default Phase 2's
// DocumentView will use) and prints the resulting toHtml() so a human can
// read the real, current-Qt-version output.
//
// Deliberately does NOT declare its own pass/fail verdict: Qt's exact HTML
// shape for the borderline constructs (task-list checkboxes, footnote
// markers) is not something to guess at from documentation or from a
// different Qt version — it has to be read from real output produced by
// the exact Qt build this project compiles against. Read the printed HTML
// for each construct and record a pass/partial/fail decision directly in
// the roadmap's Learnings log (§6.2).

#include <QGuiApplication>
#include <QTextDocument>
#include <QDebug>
#include <QList>
#include <QString>

namespace {

struct Construct
{
    const char *name;
    QString markdown;
};

}  // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    const QList<Construct> constructs = {
        {"Table",
         QStringLiteral("| A | B |\n"
                         "|---|---|\n"
                         "| 1 | 2 |\n")},
        {"Strikethrough",
         QStringLiteral("This is ~~struck through~~ text.\n")},
        {"Task list",
         QStringLiteral("- [ ] unchecked\n"
                         "- [x] checked\n")},
        {"Footnote",
         QStringLiteral("Here is a claim needing a source.[^1]\n\n"
                         "[^1]: The source.\n")},
        {"Fenced code block with language tag",
         QStringLiteral("```cpp\n"
                         "int main() { return 0; }\n"
                         "```\n")},
        {"Nested list",
         QStringLiteral("- top\n"
                         "  - nested\n"
                         "    - nested twice\n")},
        {"Relative-path local image",
         QStringLiteral("![alt text](images/test.png)\n")},
    };

    for (const Construct &c : constructs) {
        QTextDocument doc;
        doc.setMarkdown(c.markdown);
        qInfo().noquote() << "=====" << c.name << "=====";
        qInfo().noquote() << "--- source markdown ---";
        qInfo().noquote() << c.markdown;
        qInfo().noquote() << "--- toHtml() output ---";
        qInfo().noquote() << doc.toHtml();
        qInfo().noquote() << "";
    }

    return 0;
}
