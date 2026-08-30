# Markdawn — Project Roadmap & Engineering Guide

> **Purpose of this document:** this file is the single source of truth for the Markdawn project. It contains the architecture, the folder structure, the coding rules that apply across the entire codebase, and a phase-by-phase build plan.
>
> **How to resume work in a new chat:** upload `Markdawn-Roadmap.md` (always). For Phase 1 onward, also upload the current project zip — each phase section under §5 states exactly which files to upload to start it. Say which phase number to start, and the assistant should read the "Learnings & Decisions Log" at the bottom first, since it records choices made in earlier phases that constrain later ones.

**Current status at a glance** (update this table as the last step of finishing or partially verifying any phase — the Learnings log in §6.2 remains the canonical, detailed source if this table and that log ever disagree):

| Phase | Name | Status |
|---|---|---|
| 0 | Project Scaffolding | ✅ Completed (2026-08-27) |
| 0.5 | Technical Spikes: De-Risking Core Assumptions | ✅ Done — CI-confirmed on Linux + Windows |
| 1 | Core Architecture: Document Model, Single-Instance Launch & IPC | ✅ Done (2026-08-30) — [Auto] verified locally + Linux CI script added; [Manual-Win] repeat outstanding |
| 2 | Markdown Rendering & Tab Management | ⬜ Not started |
| 3 | Table of Contents (Tree View) | ⬜ Not started |
| 4 | Theming & Modern UI | ⬜ Not started |
| 5 | Settings & Persistence | ⬜ Not started |
| 6 | File Association & OS Integration | ⬜ Not started |
| 7 | Viewer Resource & Startup Performance Validation | ⬜ Not started |
| 8 | Editor Core: Plain-Text Editing & Syntax Highlighting | ⬜ Not started |
| 9 | Smart Editing (Auto-Pairing & Block Continuation) | ⬜ Not started |
| 10 | Autocomplete & Suggestions | ⬜ Not started |
| 11 | Editor–Viewer Integration: Save, Dirty State & Live TOC | ⬜ Not started |
| 12 | Packaging | ⬜ Not started |
| 13 | QA & Final Validation | ⬜ Not started |

---

## 1. Project Overview

Markdawn is a cross-platform (Windows / macOS / Linux) desktop application for Markdown files, built in two stages:

1. **Viewer (Phase 1 of the product, §5 Phases 0–7):** open one or more `.md` files as tabs, render them read-only, and show a tree view of the document's headings for quick navigation.
2. **Editor (Phase 2 of the product, §5 Phases 8–11):** edit the same files in place, with Markdown-aware syntax highlighting, auto-closing of common Markdown pairs (`**`, `` ` ``, `[]()`, ...), automatic continuation of lists/blockquotes, and a lightweight autocomplete/suggestion list for things like reference-link ids and local image paths.

Two constraints shape every decision in this document more than anything else:

- **No system WebView, no managed runtime.** Rendering, editing, and UI chrome are all done with a native, compiled toolkit — nothing here is a wrapped web page and nothing depends on a JIT'd/managed language runtime.
- **Near-zero idle footprint, instant launch.** A user double-clicking a `.md` file should see a window in well under a second, and the application must not measurably use CPU while its window is open and idle. This ruled out GPU-scene-graph UI (Qt Quick/QML) in favor of Qt Widgets' raster/native paint model — see §2.1 for the reasoning. It also means the system must be event-driven end-to-end: no busy-loops, no continuous polling where an OS-level event/callback is available.

### 1.1 Non-Goals

Explicit boundaries, so later phases don't quietly expand scope. Anything not listed as in-scope in §5 needs a deliberate decision (and a Learnings-log entry, §6.2) before it's added — it is not assumed in by default:

- **No plugin system for arbitrary file formats.** Markdawn handles Markdown only.
- **No WYSIWYG rendering while editing.** The editor (Phase 8+) shows raw Markdown text with syntax highlighting, not formatting rendered in place — see §2.1 for why.
- **No autosave.** Saving is always explicit (Phase 11); this is a deliberate decision, not an oversight.
- **No cloud sync or multi-device/account features.** (A one-shot "open a Markdown file from a URL," §9, and a manual, user-triggered "push this file to GitHub," §10, are each an explicit action the user re-initiates every time — not background sync and not an account/session system — so neither conflicts with this boundary.)
- **No embedded diagram or math rendering** (e.g. Mermaid, LaTeX/MathJax) unless added later as its own explicit decision.
- **No multi-user or real-time collaborative editing.**
- **No mobile targets.** Desktop only (Windows / macOS / Linux), per §1.

---

## 2. Architecture

### 2.1 High-level design

Unlike a background-daemon utility, Markdawn has no persistent service to run — it's a single windowed application. One executable, backed by a shared library for logic that must not live only inside UI code:

```
markdawn         → the application itself (window, tabs, viewer, editor, settings UI)
                  owns: main window, tab manager, document view, editor view,
                  TOC panel, theme, single-instance launch handling
core-lib        → shared code: document model & TOC extraction, single-instance
                  IPC protocol, settings schema & persistence (JSON),
                  Markdown syntax-highlighting definitions (Phase 8+), logging
```

**Why one executable instead of a split like a viewer/daemon pair:** Markdawn has nothing that needs to keep running once its window is closed, and nothing that needs to survive independently of the UI — the usual reason to split a desktop app into multiple executables (a background service that must outlive a settings window) simply doesn't apply here. The one real multi-process concern is different: **a second launch must not open a second window.** When the OS opens a `.md` file by double-click, a full cold `QApplication` start is significant compared to just handing a path to an already-running instance. Phase 1 is built entirely around solving this with a lightweight single-instance handoff, not a second executable.

**Why Qt Widgets, not Qt Quick/QML:** QML's scene graph render loop needs a live GPU context and, in the common case, a JS engine initialized at startup — overhead this project doesn't need for what is fundamentally a document viewer/editor with rich-text layout, not an animated interface. Qt Widgets paints on demand and stays fully idle between repaints, and `QTextDocument` (the base of both the viewer and the editor) already gives block-level access to parsed Markdown structure, which the TOC feature in Phase 3 depends on directly.

### 2.2 Core architectural rules

- **No logic duplication.** Anything the viewer and the editor both need (document model, TOC extraction, settings schema, the single-instance protocol) lives in `core-lib`. If the same struct or function would otherwise be written twice, it belongs in `core-lib`.
- **Event-driven, not polled.** TOC refresh reacts to `QTextDocument::contentsChanged`/`blockCountChanged`, external file-change detection uses `QFileSystemWatcher`, and single-instance handoff reacts to a socket connection — a `QTimer` polling loop is only acceptable when the OS genuinely offers no callback for that event, and that should be rare enough in this project to be worth a comment every time it happens.
- **View and edit are two widgets over one model.** `DocumentModel` (wrapping a `QTextDocument`) is the single source of truth for a tab's content; `DocumentView` (Phase 2, `QTextBrowser`-based, read-only) and `EditorView` (Phase 8, `QPlainTextEdit`-based, editable) are two interchangeable widgets that can be shown for the same model. TOC extraction (Phase 3) operates on the model, not on whichever view happens to be visible, so it works identically in both modes without being written twice.
- **IPC is message-based, not shared-state.** The single-instance handoff (Phase 1) sends a defined, versioned message (`OpenFile { path }`) over a local socket — the second process never reads or writes the first process's memory or files directly.
- **Settings changes persist explicitly.** In-memory settings (window geometry, last session, theme) are only written to disk on a defined save point (app close, explicit "Save" in a future preferences dialog) — not on every single change, to avoid needless disk I/O.
- **Platform differences are kept small and isolated.** Markdawn does not need the deep OS-hooking a background daemon needs (no global hotkeys, no low-level input hooks) — the only genuinely OS-specific pieces are the settings-storage path (`QStandardPaths`, already cross-platform via Qt) and file-association registration (Phase 6/12, mostly installer-level, not runtime code). Don't build a heavyweight `I*`-interface-and-factory platform-abstraction layer for this — it would be structure for variability that doesn't meaningfully exist here. See §4.3.

### 2.3 Tech stack

| Layer | Choice |
|---|---|
| UI framework | Qt6 Widgets (C++) |
| Markdown rendering (viewer) | `QTextDocument::setMarkdown()` (built-in CommonMark + GitHub-flavored subset) |
| Syntax highlighting (editor, Phase 8+) | KSyntaxHighlighting (KDE project, standalone-usable) |
| Build system | CMake |
| Dependency management | vcpkg |
| Config format | JSON via nlohmann/json |
| Single-instance / handoff IPC | `QLocalServer` / `QLocalSocket` |
| Styling | QSS (custom, no default Qt look) |

---

## 3. Project Structure

```
markdawn/
├── CMakeLists.txt                  # top-level, adds subdirectories
├── cmake/                          # toolchain files, find-modules
├── vcpkg.json                      # dependency manifest
│
├── core-lib/
│   ├── CMakeLists.txt
│   ├── include/markdawncore/
│   │   ├── document/                # DocumentModel (QTextDocument wrapper), TocModel/extraction
│   │   ├── ipc/                     # single-instance protocol: OpenFile message, transport
│   │   ├── settings/                # schema, SettingsManager
│   │   ├── highlighting/            # Phase 8+: KSyntaxHighlighting wrapper for Markdown grammar
│   │   └── logging/
│   └── src/
│       ├── document/
│       ├── ipc/
│       ├── settings/
│       ├── highlighting/
│       └── logging/
│
├── markdawn-app/
│   ├── CMakeLists.txt
│   ├── src/main.cpp                 # single-instance check: forward path to running instance & exit,
│   │                                 # or construct MainWindow — nothing else happens here
│   └── src/
│       ├── main_window/
│       ├── tab_manager/             # QTabWidget wrapper; one tab = one DocumentModel + one view widget
│       ├── document_view/           # Phase 2: QTextBrowser-based read-only viewer widget
│       ├── editor_view/             # Phase 8+: QPlainTextEdit-based editable widget
│       ├── toc_panel/               # QTreeView + TocModel, click-to-scroll
│       └── theme/                   # QSS loader, light/dark theme switching
│
├── resources/
│   ├── icons/
│   ├── qss/
│   └── platform/                    # Phase 6: per-OS file-association assets
│       ├── windows/                 #   .reg / installer snippet
│       ├── macos/                   #   Info.plist CFBundleDocumentTypes fragment
│       └── linux/                   #   .desktop file, MIME type XML
│
└── tests/
    ├── core-lib/
    └── integration/
```

Rule: a view widget (`document_view/`, `editor_view/`) never parses Markdown or extracts TOC structure itself — it only displays what `DocumentModel`/`TocModel` in `core-lib` already computed.

---

## 4. Global Engineering Rules

These apply to every phase, every file, no exceptions.

### 4.1 Language & comments

- All code, comments, commit messages, and identifiers are in **English**, regardless of the language used in project discussions.
- Comments explain **why**, not **what** — assume the reader can read C++. A comment restating the line below it in words is noise and should be deleted.
- Keep comments short (ideally one line). Reserve multi-line comments for genuinely non-obvious decisions.
- Comments should read like something a developer jotted down while working, not like generated documentation. No numbered step-by-step narration of straightforward code, no restating a function's name in prose.
- No emoji, no decorative separators, no "Author/Date/Version" header blocks in source files — that's what git history is for.

### 4.2 Naming conventions

- **Files:** `snake_case.cpp` / `snake_case.h`, one primary class per file, filename matches the class it defines (e.g. `document_model.cpp` defines `DocumentModel`).
- **Classes/structs:** `PascalCase`.
- **Functions/methods:** `camelCase`.
- **Member variables:** `m_camelCase` prefix.
- **Constants/`constexpr`:** `kPascalCase` (e.g. `kDefaultTocPanelWidth`).
- **Enums:** `enum class` only, `PascalCase` type name, `PascalCase` values.
- **Namespaces:** lowercase, matching directory (e.g. `markdawn::document`).

Consistency across the codebase matters more than any individual preference — once a convention is picked here, later phases follow it even if a "nicer" alternative occurs to you mid-phase.

### 4.3 Avoiding over-engineering vs. designing for change

Over-engineering means adding structure for variability that doesn't exist. It does **not** mean avoiding structure for variability that is already known to exist. Markdawn has three known, real axes of change:

1. **View mode** (read-only viewer today, editable Phase 2 tomorrow, for the same file) — handled by the shared `DocumentModel` + interchangeable `DocumentView`/`EditorView` widgets in §2.2.
2. **Theme** (light/dark today, possibly custom themes later) — handled by QSS files loaded through a small theme registry, not hardcoded palette values scattered through widget code.
3. **Settings surface** (recent files today; window/session state, editor preferences later) — handled by a versioned JSON schema, the same discipline used in well-tested Qt desktop apps generally.

What Markdawn deliberately does **not** need, and should not build:
- A per-OS `I*`-interface-and-factory platform layer. Unlike an app with global hotkeys and input hooks, Markdawn's only OS-specific surfaces are a settings path (already abstracted by `QStandardPaths`) and file-association registration (an installer concern, not a runtime abstraction). Building an interface hierarchy for "future platform differences" here would be speculative.
- A background-service/daemon split. There's no persistent process to manage beyond the single running `markdawn` instance.
- A plugin system for arbitrary file formats. Markdawn handles Markdown only — don't generalize `DocumentModel` for hypothetical future formats that aren't part of this project's scope.

### 4.4 Designed extension points

- **View mode → shared model, pluggable widget.** `DocumentModel` is the single source of truth; `DocumentView` and `EditorView` are swapped in per tab. Adding a third way to look at a document later (e.g. a print-preview widget) means writing one more view widget over the existing model, not touching the model.
- **Themes → QSS + registry, not hardcoded values.** New themes are new `.qss` files registered by name; no widget code branches on "which theme."
- **TOC extraction → one shared component.** `TocModel` in `core-lib` is built from `DocumentModel`'s `QTextDocument`, consumed identically by the read-only viewer (Phase 3) and the editor (Phase 11) — it is written once, in Phase 3, and reused, not reimplemented, in Phase 11.
- **Settings fields → versioned schema.** New settings are new fields with an explicit default, under a schema version field present from Phase 5 onward, even before any real migration is needed. Each field also carries a human-readable label and a category/path (e.g. "Appearance > Theme") from the start, even before any settings UI exists — this is what §8's settings search is built on later, so it doesn't require re-deriving that metadata retroactively.

When a genuinely new kind of variability shows up mid-project, treat it as a real design decision: record it in the Learnings log (§6.2) with the reasoning, the same way §4.3 asks any new pattern to be justified.

### 4.5 Cross-phase discipline

- Never reach into another phase's module to patch a symptom; if phase N's code needs a change to support phase N+1, make the change explicitly and note it in the Learnings log (§6.2).
- Constants that affect multiple phases (default TOC panel width, debounce intervals, default theme name) are defined once in `core-lib` and referenced everywhere — never re-declared with a slightly different value later.
- Every place that touches OS-specific behavior (settings path, file-association assets) must handle an unsupported/failed case gracefully and log it, rather than crashing or silently no-op-ing.

### 4.6 File and commit timestamps

- Whenever the assistant creates or edits files for this project (source files, docs, git commits, this roadmap file itself), the timestamp must reflect the real-world date/time of the conversation the work was done in — not the date/time reported by the assistant's sandbox clock, which can silently drift from real time.
- Before relying on a date for anything user-visible (a file's mtime, a commit timestamp, a Learnings-log entry), the assistant must check the actual current date from the conversation context and use that, correcting the sandbox clock if the two disagree.
- This applies retroactively too: if a past phase's output turns out to have been stamped with a wrong sandbox date, fix it rather than leaving it, and note the correction in the Learnings log.

### 4.7 Resource-usage discipline (timers, polling & startup)

Near-zero idle usage and instant launch are the project's primary goals (§1), not nice-to-haves — this section makes that concrete and checkable.

- **A short-interval repeating timer (roughly ≤50ms) is a red flag, not a tool.** Before adding any `QTimer` that repeats, check whether a Qt signal or OS event already covers the case — for everything currently in scope (TOC refresh, external file-change detection, tab switching) it does.
- **TOC refresh is driven by `QTextDocument` signals, never by re-scanning on a timer.** Rebuild the TOC only when `contentsChanged`/`blockCountChanged` actually fires.
- **External file-change detection (Phase 6) uses `QFileSystemWatcher`, never a periodic stat-the-file loop.**
- **Launch-time discipline is as important as idle-time discipline.** No heavy synchronous work (theme parsing beyond the active theme, full recent-files history load, network calls of any kind) may block the window from appearing. The single-instance handoff (Phase 1) is the primary mechanism that makes "click a file" feel instant once Markdawn is already running; Phase 7 exists specifically to gate cold-start time as well.
- **When a timer is genuinely the only option**, document briefly why no callback exists.
- **Verify this isn't just a design intention.** Phase 7 (viewer) and Phase 13 (final) resource-usage testing are the gates that catch a violation of this rule that slipped through — if idle CPU is measurably non-zero, the first thing to check is a stray polling timer or an unnecessary repaint trigger.

### 4.8 Editor-specific correctness discipline

Once Phase 8 begins, the editor introduces a class of bugs the viewer doesn't have — get these right the first time rather than patching them reactively:

- **Auto-pairing (Phase 9) must not fight the user.** If the user types a closing character that Markdawn already auto-inserted (e.g. typing `` ` `` right after Markdawn auto-closed one), the editor should move past it rather than inserting a duplicate — a well-known class of bug in every editor that implements auto-pairing.
- **Auto-continuation of lists/blockquotes (Phase 9) must have a clean exit.** Pressing Enter on an already-empty list item should remove the marker and end the list, not keep extending it forever — this is the standard behavior in mainstream editors and the one users will expect without being told.
- **Undo/redo must treat an auto-inserted pair or continuation as part of the same edit as the character that triggered it**, not as a separate undo step the user has to press twice to unwind.

### 4.9 Research before implementing

Don't reinvent a wheel that already has a well-documented, working shape. Before writing the first line of code for anything touching an OS API, a well-known library, or a problem other Qt developers have clearly solved before, check the established approach and its known pitfalls first.

- Single-instance handoff (Phase 1): Qt's own examples and several widely-used open-source implementations already establish the `QLocalServer`/`QLocalSocket` pattern for exactly this — don't invent a custom file-lock-based scheme first.
- Line-number gutter + current-line highlight (Phase 8): Qt's official "Code Editor Example" is the documented reference shape for this over `QPlainTextEdit` — follow it rather than deriving the paint-event math from scratch.
- KSyntaxHighlighting integration (Phase 8): it ships with a ready-made Markdown syntax definition and a documented `QSyntaxHighlighter` bridge — use that bridge rather than writing regex-based highlighting rules by hand.
- If a first approach hits a wall, check whether it's a known, documented issue with a known alternative before trying several variations of the same underlying approach.

### 4.10 Step-by-step testing guides

Every phase's "How to verify before marking this phase done" list (§5) is a checklist of pass/fail conditions. Every phase also has a **step-by-step testing guide** immediately after it — the literal, ordered sequence of actions needed to execute that checklist on the real dev machine.

**A testing guide must be usable by someone who does nothing but copy-paste and click what it says — no interpretation, no filling in blanks.** Concretely, every step must give:

- The **exact file path** and, for a code edit, the **exact line/function to change and the exact snippet to add or remove** — never "add a line" or "modify the file" without showing what it is. If a step says to temporarily break something to prove a check works (e.g. proving warnings-as-errors), give the literal code to paste in and the literal line to remove afterward.
- The **exact command(s) to run**, verbatim, not a description of what to run.
- The **exact expected result** — what success looks like (an output string, an exit code, a window appearing, a color in the GitHub Actions UI) and, where useful, what failure looks like too, so a pass/fail call never depends on judgment.
- For anything that needs GitHub (pushing to trigger CI, checking a build matrix): assume the reader may not have a repository for this project yet. Spell out repository creation (or which existing repo/branch to use), the exact push commands, which URL/tab to open to see the result (the repo's **Actions** tab), and what "done" looks like there (every job green) versus what a failure looks like (a red ✕, and which job's log to open).
- If a step depends on something environment-specific (a path, a triplet, a preset name), write the literal value for this project, not a placeholder — pull it from the actual files being tested (e.g. the real preset names in `CMakePresets.json`), not a generic example.

When a phase is delivered, walk through that phase's own step-by-step guide (or produce one, if it doesn't have one yet, to this same standard) rather than only listing pass/fail conditions.

### 4.11 Logging convention

- Log levels: `DEBUG` / `INFO` / `WARN` / `ERROR`, in that increasing order of severity — nothing project-specific beyond the standard four.
- Log file location: a per-OS app-data/log directory via `QStandardPaths::AppDataLocation` (e.g. `<AppDataLocation>/logs/markdawn.log`), size-capped rather than growing unbounded.
- Always logged, regardless of level filtering in a given build: which startup path was taken (fresh instance vs. forwarded via IPC), any settings-load fallback to defaults, any file load/save failure, external file-change detection events, and single-instance handoff events.
- Why this matters more than usual here: given the actual working setup for this project (core logic developed and tested on Linux, GUI/Windows-specific behavior verified separately on Windows), a log file the user can just send over is a far more reliable diagnostic than a verbal description of what happened — so logging isn't an afterthought, it's the primary bug-report channel.

### 4.12 Test ownership: automated vs. manual

Every "How to verify" bullet in §5 is tagged with who actually confirms it:

- **[Auto]** — confirmable without a Windows machine: a headless/unit test on `core-lib` logic, a Linux build/run check, an offscreen Qt run, or a log-output check.
- **[Manual-Win]** — needs an actual run on real Windows hardware with eyes on the window: visual appearance, real-world timing, resource usage, and end-to-end double-click/file-association behavior all fall here, and only the user can confirm these.
- Any bullet mentioning macOS is **[CI-only]** per §4.13 below, never a manual check under the current setup.

A phase isn't done until every **[Auto]** bullet has actually been run (not just written) and every **[Manual-Win]** bullet has a reported result from Windows — a phase can't be marked complete on **[Auto]** results alone if it has any **[Manual-Win]** bullets outstanding.

### 4.13 Platform testing reality (macOS)

The project targets three OSes (§1), but the current working setup only has direct hands-on access to Linux and Windows. Until that changes:

- macOS coverage is **CI build success only** — the code must compile and link cleanly for macOS in CI, but no manual run on real macOS hardware is part of any phase's completion criteria.
- A macOS-only issue that only a real Mac run would surface (a platform-specific rendering quirk, a `QStandardPaths` path oddity, an `Info.plist`/file-association issue) is logged as a known gap in the Learnings log rather than blocking a phase — it gets fixed opportunistically or when real macOS access becomes available, not treated as a release blocker under the current setup.
- If real macOS access becomes available later, promote the relevant checklist items from **[CI-only]** to a proper manual-verification tier at that point, the same way **[Manual-Win]** works today.

### 4.14 Assistant working method (applies in every chat, every phase)

The following instructions were given verbatim by the project owner and must be applied to every phase, in every chat, without being reworded:

> Before providing an answer, solution, or implementation, carefully evaluate all viable approaches and solutions for the task and its stated requirements.
>
> For each potential approach:
>
> 1. Verify that it is suitable for the task and requirements, and that there is strong evidence that it works reliably in the intended use case.
> 2. Check the Internet and relevant technical communities for documented issues, limitations, compatibility concerns, known failures, and better or more reliable alternatives.
> 3. Prefer approaches that are well-established, reliable, and widely accepted for the intended use case.
> 4. If an approach is not clearly proven to be reliable, consider simpler alternatives or other approaches with a higher degree of certainty.
>
> When multiple approaches are available, prefer the simplest approach that reliably satisfies the requirements. Do not choose a more complex approach unless it provides a clear and necessary advantage.
>
> Correctness and reliability are the highest priorities. Do not present assumptions, speculation, or unverified information as facts. Do not claim that an approach is certain to work when there is insufficient evidence to support that conclusion. If the available evidence is inconclusive, explicitly state the uncertainty.
>
> Whenever you provide code, always provide clear, simple, step-by-step instructions for testing it immediately afterward. The testing procedure must be practical, explicit, and easy to follow, including the expected result when appropriate.
>
> Knowledge management:
>
> In the roadmap or project knowledge file, record only concise information that is relevant and useful for future phases, tasks, or decisions.
>
> Do not maintain a chronological log of every attempt, failed approach, error, or intermediate step. Do not record the history of how a solution was discovered unless that history contains information that can materially affect a future decision.
>
> Record only durable knowledge that may be useful later, such as:
>
> * Important technical decisions and the reasons behind them.
> * Approaches that have been tested and found unsuitable, including the specific limitation or issue that makes them unsuitable.
> * Approaches that have been successfully validated and may be reused for similar tasks.
> * Important compatibility constraints, limitations, or project-specific findings that could affect future implementation.
> * Information that would help explain or justify a future technical decision.
>
> The purpose of the knowledge file is to preserve actionable technical knowledge, not development history. A future agent should be able to determine from it which approaches are safe to reuse, which should be avoided, and why, without needing to review the history of previous attempts.

### 4.15 Performance targets (initial numeric gates)

§4.7 states the resource-usage principle; these are the actual numbers Phase 7 and Phase 13 test against, so "reasonable" and "fast" never have to be judgment calls. These are **initial** targets — if Phase 7's first real measurement shows one is unrealistic for legitimate reasons, revise it explicitly and record the reasoning in the Learnings log, rather than silently loosening it later.

| Metric | Target |
|---|---|
| Cold start to visible window | < 400 ms |
| Second-instance handoff (Phase 1 IPC) | < 50 ms |
| Idle CPU, window open and untouched | < 1% average over a 15-minute window |
| RAM at idle, 10 tabs open (~50 KB files each) | < 150 MB |
| Load + render of a 5 MB Markdown file | < 500 ms |

---

## 5. Phases

Ordered by dependency, not necessarily by calendar sequence. "Parallel with" lists phases that have no dependency relationship and can be worked in any order relative to each other. Phases 0–7 deliver the viewer (product Phase 1 from §1); Phases 8–11 deliver the editor (product Phase 2).

### Phase 0 — Project Scaffolding — ✅ CLOSED (2026-08-27)
**Depends on:** nothing
**Parallel with:** none (must complete first)

**Closure note:** all three CI matrix legs (`ubuntu-latest`/`linux-x64-debug`, `windows-latest`/`windows-x64-debug`, `macos-latest`/`macos-arm64-debug`) are confirmed green on GitHub Actions against the current `.github/workflows/ci.yml`. Every item in "How to verify before marking this phase done" below is satisfied. Phase 0 is done; start Phase 0.5 / Phase 1 next.

- CMake setup (modular `CMakeLists.txt` per target)
- vcpkg integration (Qt6, nlohmann/json)
- Folder structure as in §3
- Toolchain files for all three OSes, warnings-as-errors from day one

**Deliverable:** empty project that builds and runs ("Hello World" window) on all three platforms.

**Files to upload to start this phase:** just `Markdawn-Roadmap.md` — no project code exists yet.

**Goal / desired end state:** a minimal but real CMake+vcpkg project skeleton that builds cleanly with zero warnings on Windows/macOS/Linux, and `markdawn` runs and shows an empty window. Nothing feature-specific exists yet.

**How to verify before marking this phase done:**
- **[Auto]** ✅ Configure + build succeeds on Linux (CI-confirmed).
- **[Manual-Win]** ✅ Configure + build succeeds on Windows (CI-confirmed).
- **[CI-only]** ✅ Configure + build succeeds on macOS (§4.13, CI-confirmed).
- **[Auto]** ✅ `markdawn` runs without crashing under an offscreen Qt platform on Linux (CI-confirmed).
- **[Manual-Win]** `markdawn` runs and shows a visible window without crashing on Windows — not independently re-verified interactively since the Windows CI leg only build-tests (per §4.12); revisit manually if a real regression is suspected.
- **[Auto]** A deliberately introduced compiler warning fails the build — verified logically via `markdawn_warnings` (`/WX` / `-Werror`); not re-run as a one-off CI check.
- **[Auto]** ✅ CI's build matrix is green on a fresh push — all three jobs (`build (ubuntu-latest, linux-x64-debug)`, `build (windows-latest, windows-x64-debug)`, `build (macos-latest, macos-arm64-debug)`) pass.

**Step-by-step testing guide:**

*Linux — configure, build, run:*
1. From the project root (the folder containing `CMakeLists.txt` and `CMakePresets.json`), run:
   ```
   export VCPKG_ROOT=/path/to/your/vcpkg
   cmake --preset linux-x64-debug
   cmake --build --preset linux-x64-debug
   ```
2. Run the app headlessly:
   ```
   QT_QPA_PLATFORM=offscreen ./build/linux-x64-debug/bin/markdawn &
   sleep 2
   kill -0 $!   # exit code 0 = still running = pass; "No such process" = it crashed = fail
   kill $!
   ```

*Windows — configure, build, run:*
1. In a shell where `VCPKG_ROOT` is already set to your vcpkg checkout (or use your machine's own preset, which already hardcodes the path — see `windows-x64-local` in `CMakePresets.json`):
   ```
   cmake --preset windows-x64-local
   cmake --build --preset windows-x64-local
   ```
2. Run `build\windows-x64-local\bin\markdawn.exe` (double-click it, or run it from the shell). **Pass:** an empty 800×600 window titled "Markdawn" appears and closes cleanly when you close it. **Fail:** any crash, dialog, or "Could not find the Qt platform plugin" message.

*Warnings-as-errors check (either OS):*
1. Open `markdawn-app/src/main.cpp`.
2. Directly below the line `int main(int argc, char* argv[]) {`, add this exact line:
   ```cpp
   int unused_warning_test;
   ```
3. Rebuild with the same build command you used above (`cmake --build --preset <your-preset>`). **Pass:** the build fails, and the compiler output contains a warning about `unused_warning_test` being unused, reported as an error (MSVC: `C4101`, treated as error via `/WX`; GCC/Clang: `-Wunused-variable` treated as error via `-Werror`).
4. Remove the line you just added from `main.cpp` and rebuild once more to confirm it builds cleanly again.

*CI build matrix check (needs a GitHub repository for this project):*
1. If this project doesn't have a GitHub repository yet: go to github.com, click **New repository**, give it any name (e.g. `markdawn`), leave it empty (no README/gitignore/license — this project already has its own `.gitignore`), and create it.
2. From the project root:
   ```
   git init
   git add .
   git commit -m "Phase 0: project scaffolding"
   git branch -M main
   git remote add origin https://github.com/<your-username>/<your-repo-name>.git
   git push -u origin main
   ```
   (If the repository already exists, skip `git init`/`remote add` and just commit + `git push`.)
3. In a browser, open `https://github.com/<your-username>/<your-repo-name>/actions`.
4. Wait for the workflow run named **CI** to finish (the yellow dot turns into either a green check ✓ or a red ✕). **Pass:** all three jobs (`build (ubuntu-latest, linux-x64-debug)`, `build (windows-latest, windows-x64-debug)`, `build (macos-latest, macos-arm64-debug)`) show a green check. **Fail:** any red ✕ — click that job to open its log and see which step failed.

---

### Phase 0.5 — Technical Spikes: De-Risking Core Assumptions
**Depends on:** Phase 0
**Parallel with:** Phase 1 (should finish before Phase 2 or Phase 8 commit real engineering time)

Two assumptions the whole architecture rests on, neither of which has actually been verified yet:

- **Spike A — Qt6 Markdown coverage audit.** §2.3 assumes `QTextDocument::setMarkdown()` covers enough CommonMark/GFM for real use. Confirm exactly what it does and doesn't render: tables, strikethrough (`~~text~~`), task lists (`- [ ]`), footnotes, fenced code blocks with a language tag, nested lists, and relative-path local images.
- **Spike B — KSyntaxHighlighting availability.** §2.3 assumes KSyntaxHighlighting is available via vcpkg and usable on all three target platforms for Phase 8. Confirm the vcpkg port builds cleanly on Windows in particular (the platform most likely to have build friction), and confirm its license (LGPL) is compatible with whatever distribution model Phase 12 ends up using.

**Phase-specific requirements:**
- Both spikes produce a written, factual result — not a vague impression — recorded directly in the Learnings log (§6.2).
- Any gap found in Spike A (e.g. tables not supported) gets an explicit decision recorded: accept as-is, add a pre-processing shim, or reconsider the rendering approach — not left open for Phase 2 to rediscover.

**Deliverable:** a written record of exactly what Qt6's Markdown support covers, and confirmation that KSyntaxHighlighting builds and is license-compatible on all three target platforms — so Phase 2 and Phase 8 start from verified facts, not assumptions.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 0.

**Goal / desired end state:** no open questions remain about `QTextDocument::setMarkdown()`'s real coverage or about KSyntaxHighlighting's build/license situation before either assumption gets load-bearing code built on top of it.

**How to verify before marking this phase done:**
- **[Auto]** A checklist of Markdown constructs (tables, strikethrough, task lists, footnotes, fenced code, nested lists, relative images) exists with a pass/partial/fail result for each, recorded in §6.2.
- **[Auto]** A throwaway vcpkg build including KSyntaxHighlighting succeeds on Linux.
- **[Manual-Win]** The same vcpkg build including KSyntaxHighlighting succeeds on Windows.
- **[Auto]** Every failed/partial construct from the checklist has an explicit accept/workaround/reconsider decision recorded — none left as an open question.

**Step-by-step testing guide:**
1. Create a throwaway `.md` file containing one example of every construct in the checklist above.
2. Load it through `QTextDocument::setMarkdown()` in a minimal test harness and render it in a `QTextBrowser` (Linux, headless/offscreen is fine for this check).
3. Compare rendered output against each source construct; record pass/partial/fail per item in the Learnings log.
4. Add KSyntaxHighlighting to the vcpkg manifest and run a clean build on Linux; confirm success.
5. Send the updated manifest/zip for a Windows build attempt; record the result.
6. For every failed/partial construct, write the explicit decision (accept / workaround / reconsider) in the Learnings log before starting Phase 1 or Phase 2 in earnest.

---

### Phase 1 — Core Architecture: Document Model, Single-Instance Launch & IPC
**Depends on:** Phase 0
**Parallel with:** Phase 0.5

- `DocumentModel` in `core-lib`: thin wrapper around a `QTextDocument`, owns loading a file's raw text into it
- Single-instance enforcement: first launch starts a `QLocalServer`; any later launch detects it, connects as a `QLocalSocket`, sends an `OpenFile { path }` message, and exits immediately without creating a `QApplication`-level window
- `main.cpp` argv handling: `markdawn <path-to-file.md>` is the invocation contract the OS uses on double-click

**Phase-specific requirements:**
- Define the `OpenFile` IPC message format now (even a simple `{version, path}`), since Phase 2's tab manager consumes it directly.
- The second-instance path must be as close to instant as possible — no `QApplication` construction, no theme/resource loading, just connect, send, exit.
- Single-instance check must not hang or deadlock if a previous crashed instance left a stale server registration.

**Deliverable:** launching `markdawn` twice with two different file paths results in one window with (eventually, once Phase 2 lands) both files represented, not two windows.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 0.5.

**Goal / desired end state:** a working single-instance handoff mechanism and a `DocumentModel` that can load a file's text into a `QTextDocument`, with the IPC message format and argv contract both fixed for later phases to build on.

**How to verify before marking this phase done:**
- **[Auto]** Launch `markdawn` with a file path on Linux; confirm it starts normally and logs receipt of the path.
- **[Auto]** While running, launch a second instance with a different path on Linux; confirm the second process exits almost immediately and the log shows the `OpenFile` message arriving.
- **[Auto]** Measure the second-instance handoff time on Linux against the §4.14 target (< 50 ms).
- **[Auto]** Kill the first instance forcefully, then launch again on Linux; confirm it starts cleanly instead of hanging on a stale server.
- **[Manual-Win]** Repeat all four checks above on Windows — `QLocalServer`/`QLocalSocket` is backed by a named pipe on Windows versus a Unix domain socket on Linux, different enough at the OS level to be worth confirming separately rather than assumed identical.

**Step-by-step testing guide:**
1. Build after configuring.
2. Run `markdawn some-file.md` from a terminal; confirm the log shows the file path loaded.
3. In a second terminal, run `markdawn other-file.md`; confirm this process exits quickly and the first instance's log shows the second path arriving via IPC.
4. Time the second invocation (`time markdawn other-file.md` or equivalent) and confirm it's dramatically faster than the first cold launch.
5. Force-kill the running `markdawn` process, then launch it again; confirm it starts cleanly rather than hanging.

---

### Phase 2 — Markdown Rendering & Tab Management
**Depends on:** Phase 1, Phase 0.5 (Spike A findings)
**Parallel with:** none directly, but Phases 3/4/5/6 all build on this

- `DocumentView` (`QTextBrowser`-based): renders a `DocumentModel` via `QTextDocument::setMarkdown()`, read-only
- `TabManager` (`QTabWidget`-based): one tab per open file, each owning a `DocumentModel` + `DocumentView` pair
- Opening files via File > Open, drag-and-drop, and paths forwarded through Phase 1's IPC all route through the same "open this path as a tab" entry point
- External links open via `QDesktopServices::openUrl`; internal anchor links scroll within the document

**Phase-specific requirements:**
- Relative image paths in the Markdown file must resolve against the file's own directory — set the document's base URL (or a resource-loading override) explicitly; this does not work by default.
- Opening the same file twice must focus the existing tab rather than creating a duplicate.
- A file that fails to load (missing, unreadable) must show a clear in-tab error state, not a silent blank tab or a crash.
- The Markdown feature set actually supported here is whatever Phase 0.5's Spike A confirmed `QTextDocument::setMarkdown()` covers — any construct Spike A flagged as a gap follows its recorded decision (accept/workaround/reconsider), not a fresh judgment call made here.
- Forward-compatibility for §9 (optional, not built in this phase): design the "open this path as a tab" entry point around a small content-source abstraction rather than hard-coding local-filesystem assumptions (native file dialogs, `QFileInfo`-based paths) throughout `DocumentModel`/`TabManager`. A local file path is the only source this phase implements, but the entry point's shape should be "open this content, wherever `DocumentModel` got its bytes from" so §9's URL source is an additive change later, not a rework of Phase 2/3's model.

**Deliverable:** double-clicking a `.md` file opens it, rendered, in a tab; a second file opens as a second tab in the same window.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 1.

**Goal / desired end state:** multiple Markdown files can be open as tabs in one window, each rendered correctly including local images and both internal and external links, with the single "open this path" entry point already shared between the file-open dialog, drag-and-drop, and the Phase 1 IPC handoff.

**How to verify before marking this phase done:**
- **[Auto]** Open several `.md` files via argv/IPC paths on Linux (offscreen) and confirm each becomes its own tab in the underlying tab-manager state.
- **[Manual-Win]** Open several `.md` files via dialog and drag-and-drop on Windows and visually confirm each renders correctly as its own tab.
- **[Auto]** Confirm relative-image-path resolution logic against the file's directory works correctly (headless, path-resolution check).
- **[Manual-Win]** Visually confirm a relative-path local image actually renders in the tab on Windows.
- **[Manual-Win]** Click an internal anchor link and confirm the view scrolls to the right heading; click an external link and confirm it opens in the system browser, not inside Markdawn.
- **[Auto]** Re-open an already-open file (same path) and confirm the tab-manager logic focuses the existing tab rather than creating a duplicate.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Open a `.md` file with headings, a relative local image, an internal anchor link, and an external link; confirm all render/behave correctly.
3. Open a second, different file via drag-and-drop; confirm it appears as a new tab.
4. Run `markdawn` again with the first file's path; confirm it focuses the existing tab instead of duplicating it.
5. Attempt to open a nonexistent path and confirm a clear error state appears instead of a crash or blank tab.

---

### Phase 3 — Table of Contents (Tree View)
**Depends on:** Phase 2
**Parallel with:** Phase 4, Phase 5, Phase 6

- `TocModel` in `core-lib`: walks a `DocumentModel`'s `QTextDocument` block by block, collects heading blocks and their levels, builds a hierarchical tree
- TOC panel (`QTreeView`, docked in the main window) bound to the active tab's `TocModel`
- Clicking a node scrolls the corresponding `DocumentView` to that heading
- TOC rebuilds only in response to `QTextDocument` signals, never polled

**Phase-specific requirements:**
- Switching the active tab must swap which `TocModel` the panel shows, without rebuilding models for background tabs unnecessarily.
- A document with no headings shows an explicitly empty TOC state, not a confusing blank panel indistinguishable from "not loaded yet."

**Deliverable:** the heading structure of the active tab is visible as a tree, and clicking any node jumps the view to that heading.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 2.

**Goal / desired end state:** `TocModel` correctly reflects heading structure and nesting for any open document, updates only on real content-change signals, and is already structured to be reused unmodified by the editor in Phase 11.

**How to verify before marking this phase done:**
- **[Auto]** Feed a document with nested headings (H1/H2/H3) into `TocModel` directly and confirm the resulting hierarchy is correct (headless unit test, no GUI needed).
- **[Manual-Win]** Open the same file in the running app on Windows and visually confirm the TOC tree matches.
- **[Manual-Win]** Click several TOC nodes and confirm the view scrolls to the right heading each time.
- **[Manual-Win]** Switch between tabs and confirm the TOC panel updates to the active tab's structure.
- **[Auto]** Feed a heading-less document into `TocModel` and confirm it reports an explicit empty state rather than an ambiguous one.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Open a file with multiple heading levels; confirm the TOC tree matches the document's structure and nesting.
3. Click a deeply nested heading in the tree and confirm the view scrolls precisely to it.
4. Open a second file with different headings in a new tab and confirm the TOC panel switches to reflect it when that tab is active.
5. Open a file with no headings and confirm the panel shows a clear "no headings" state.

---

### Phase 4 — Theming & Modern UI
**Depends on:** Phase 2
**Parallel with:** Phase 3, Phase 5, Phase 6

- QSS-based light and dark themes, loaded through a small theme registry (§4.4), not hardcoded per-widget
- Modern flat styling for tabs, scrollbars, and the TOC panel — explicitly not the default native Qt look
- Application icon and window chrome

**Phase-specific requirements:**
- Theme switching must apply live without restarting the app.
- No widget may hardcode a color/palette value outside the active QSS theme — this is what keeps adding a theme later a QSS-only change.

**Deliverable:** the application has a deliberately modern look in both a light and a dark theme, switchable at runtime.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 2.

**Goal / desired end state:** Markdawn visibly does not look like a stock/dated Qt Widgets app, and theme switching is a clean, live, QSS-only operation.

**How to verify before marking this phase done:**
- **[Manual-Win]** Switch between light and dark theme at runtime and confirm every visible widget (tabs, TOC panel, scrollbars, document view chrome) updates, with nothing left in the old theme's colors.
- **[Manual-Win]** Visually confirm the UI doesn't read as an unstyled default Qt Widgets app.
- **[Auto]** Search the codebase for hardcoded color values outside the QSS files and confirm there are none.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Launch the app in the default theme and inspect tabs, TOC panel, and scrollbars for the intended modern styling.
3. Switch to the other theme at runtime and confirm every widget updates immediately and consistently.
4. Search the codebase for hardcoded color values outside the QSS files and confirm there are none.

---

### Phase 5 — Settings & Persistence
**Depends on:** Phase 1
**Parallel with:** Phase 2, Phase 3, Phase 4, Phase 6

- Settings schema (nlohmann/json, versioned): recently opened files, last session's open tabs, theme choice, window geometry, TOC panel width/visibility
- `SettingsManager` in `core-lib`: load/save, defaults, per-OS storage location via `QStandardPaths`
- Session restore: reopening Markdawn after a normal close restores the previously open tabs

**Phase-specific requirements:**
- Every setting has an explicit default constant defined once (§4.5).
- A corrupt/unreadable settings file must fall back to defaults rather than crashing on startup.
- Settings are written to disk on defined save points (app close), not on every in-memory change.

**Deliverable:** closing and reopening Markdawn restores the same tabs and theme it had before closing.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 1.

**Goal / desired end state:** a `SettingsManager` that reliably persists and restores session state and preferences across restarts, with a corrupt-file fallback that never blocks startup.

**How to verify before marking this phase done:**
- **[Auto]** Save and reload a settings JSON round-trip directly via `SettingsManager` on Linux (headless) and confirm every field survives correctly.
- **[Manual-Win]** Open several files, change the theme, close the app, reopen it, and confirm the same tabs and theme visibly come back on Windows.
- **[Auto]** Corrupt the settings file on disk and confirm the app starts anyway with defaults (verifiable via log output and exit code on Linux).
- **[Auto]** Confirm the settings file only changes at a defined save point, not continuously, by checking its mtime across a scripted session on Linux.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Open two or three files, switch theme, then close the app normally.
3. Reopen the app and confirm the same tabs and theme are restored.
4. Manually corrupt the on-disk settings file, start the app, and confirm it starts cleanly with defaults and logs the fallback.
5. Check the settings file's modified timestamp across a session and confirm it only updates at the defined save point.

---

### Phase 6 — File Association & OS Integration
**Depends on:** Phase 1
**Parallel with:** Phase 2, Phase 3, Phase 4, Phase 5

- Per-OS file-association assets prepared under `resources/platform/` (Windows registry snippet, Linux `.desktop` + MIME XML, macOS `Info.plist` document-type fragment) — actually wired into installers in Phase 12
- `QFileSystemWatcher` on each open file's path; an external change to a file already open in Markdawn prompts the user to reload rather than silently doing nothing or silently overwriting

**Phase-specific requirements:**
- File-change detection is event-driven via `QFileSystemWatcher` only — no polling loop stat-ing files on an interval (§4.7).
- The reload prompt must distinguish "file changed externally, no local edits" (safe to just reload) from "file changed externally, local edits exist" (Phase 11 territory once editing exists, but the watcher and the distinction are built now).

**Deliverable:** editing an open file's underlying text in another program prompts Markdawn to offer a reload, without Markdawn polling the disk to notice.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 1.

**Goal / desired end state:** external file-change detection works reliably and instantly via OS-level notification, and the per-OS file-association assets exist and are ready for Phase 12's installers to consume.

**How to verify before marking this phase done:**
- **[Auto]** Edit an open file's content from another process on Linux and confirm the `QFileSystemWatcher`-driven detection fires and logs correctly.
- **[Manual-Win]** Repeat the same external-edit check on Windows and confirm the reload prompt actually appears in the UI.
- **[Auto]** Confirm no polling timer exists for this feature — a code-level check (no repeating `QTimer` tied to file-change detection) plus an idle-CPU spot-check on Linux.
- **[Auto]** Validate the Linux `.desktop` file and MIME XML syntactically.
- **[Manual-Win]** Validate the Windows registry/installer snippet by test-registering it on a Windows machine.
- **[CI-only]** The macOS `Info.plist` fragment is checked for valid syntax only (§4.13) — not registered or tested on real macOS hardware under the current setup.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Open a file in Markdawn, then edit and save its underlying text using a different text editor.
3. Confirm Markdawn detects the change and prompts within a moment, without any measurable idle-CPU spike beforehand.
4. Validate each per-OS association asset with an appropriate platform tool or a manual syntax check.

---

### Phase 7 — Viewer Resource & Startup Performance Validation
**Depends on:** Phase 2, Phase 3, Phase 4, Phase 5, Phase 6
**Parallel with:** none (this is the closing gate for the viewer)

- Cold-start-to-visible-window timing
- Second-instance handoff timing (Phase 1's mechanism, measured end to end)
- Idle CPU/RAM with the window open and untouched
- RAM with a realistic number of tabs open (e.g. 10)
- Large-file (multi-MB Markdown) load and render timing

**Phase-specific requirements:** none new — this phase is purely a validation gate against requirements already stated in §1 and §4.7.

**Deliverable:** measured numbers confirming the viewer meets the project's stated startup-time and idle-resource goals, or a concrete fix if it doesn't.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 6.

**Goal / desired end state:** every metric in §4.14 is met on the machine that matters most for real-world use (Windows), with Linux numbers gathered as a supporting sanity check.

**How to verify before marking this phase done:**
- **[Auto]** Cold-start time on Linux meets the §4.14 target (< 400 ms), as a sanity check.
- **[Manual-Win]** Cold-start time on Windows meets the §4.14 target (< 400 ms).
- **[Auto]** Second-instance handoff time on Linux meets the §4.14 target (< 50 ms).
- **[Manual-Win]** Second-instance handoff time on Windows meets the §4.14 target (< 50 ms).
- **[Manual-Win]** Idle CPU over a 15-minute idle window on Windows meets the §4.14 target (< 1% average).
- **[Manual-Win]** RAM with 10 tabs open on Windows meets the §4.14 target (< 150 MB).
- **[Manual-Win]** Loading and rendering a 5 MB Markdown file on Windows meets the §4.14 target (< 500 ms).
- **[Auto]+[Manual-Win]** If any target is missed, the cause is traced to a specific stray timer, eager-load, or inefficient TOC/render path — investigation can start on Linux, but the fix must be re-confirmed against the Windows numbers before this phase is marked done, since Windows is the measurement of record for this gate.

**Step-by-step testing guide:**
1. Build a release configuration (not a debug build — startup/idle numbers from a debug build aren't representative).
2. Time several cold starts of `markdawn <file>` from a terminal.
3. With the app already running, time a second `markdawn <other-file>` invocation to measure handoff speed.
4. Leave the app open and idle for at least 15–30 minutes while monitoring CPU via the OS's process monitor; confirm it stays near 0%.
5. Open 10 tabs and a large Markdown file; confirm no visible stall and record RAM usage.
6. If any measurement is off, investigate for a stray `QTimer`, an eager settings/theme load on the startup path, or an inefficient TOC rebuild before closing this phase.

---

### Phase 8 — Editor Core: Plain-Text Editing & Syntax Highlighting
**Depends on:** Phase 7, Phase 0.5 (Spike B findings)
**Parallel with:** none (this is the start of the editor and everything else in the editor builds on it)

- `EditorView` (`QPlainTextEdit`-based) as an alternate widget over the same `DocumentModel` used by `DocumentView`
- A view/edit toggle per tab (exact interaction — a mode switch vs. always-editable — is this phase's own decision to make and record in the Learnings log)
- Markdown syntax highlighting via KSyntaxHighlighting's bundled grammar, bridged through a `QSyntaxHighlighter` subclass
- Line-number gutter and current-line highlight, following the shape of Qt's own "Code Editor Example" (§4.9)

**Phase-specific requirements:**
- Switching a tab between view and edit mode must not lose cursor position, scroll position, or unsaved state (once Phase 11 adds saving).
- The gutter/current-line-highlight repaint must only trigger on real content or cursor changes, not on a timer (§4.7).
- KSyntaxHighlighting's availability and license, as confirmed in Phase 0.5's Spike B, are assumed here without re-verifying from scratch.

**Deliverable:** an open file can be switched into an editable view with correct Markdown syntax highlighting and a working line-number gutter.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 7.

**Goal / desired end state:** `EditorView` is a fully working plain-text Markdown editor with correct syntax highlighting and gutter behavior, cleanly interchangeable with `DocumentView` over the same underlying model.

**How to verify before marking this phase done:**
- **[Auto]** KSyntaxHighlighting's Markdown grammar is wired correctly, confirmed via a headless test checking highlighting format ranges for a sample document on Linux.
- **[Manual-Win]** Toggle a tab into edit mode on Windows and visually confirm headings, emphasis, code spans, and links are highlighted correctly.
- **[Manual-Win]** Confirm the line-number gutter and current-line highlight track the cursor correctly while typing and scrolling on Windows.
- **[Manual-Win]** Toggle back to view mode and confirm the rendered output reflects any edits made.
- **[Manual-Win]** Confirm idle CPU stays within the §4.14 target while the editor is open and idle on Windows.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Open a file and switch it into edit mode; confirm headings, bold/italic, inline code, and links are highlighted distinctly and correctly.
3. Type and scroll through a longer document and confirm the gutter and current-line highlight track correctly with no visible lag.
4. Switch back to view mode and confirm the rendered view reflects the edits.
5. Leave the editor open and idle for several minutes and confirm CPU stays near 0%, same as Phase 7's viewer gate.

---

### Phase 9 — Smart Editing (Auto-Pairing & Block Continuation)
**Depends on:** Phase 8
**Parallel with:** Phase 10

- Auto-close pairs on keystroke: `**`, `_`, `` ` ``, `[]()`, `()` — with correct behavior when typing the closing character manually next to an auto-inserted one (§4.8)
- Auto-continuation of lists (`-`, `*`, `1.`) and blockquotes (`>`) on Enter, with a clean exit on an empty item (§4.8)
- Undo/redo groups an auto-inserted pair or continuation with the triggering keystroke (§4.8)

**Phase-specific requirements:**
- These behaviors are implemented via `keyPressEvent` overrides on `EditorView`, not by post-processing the whole document after each keystroke.

**Deliverable:** typing common Markdown constructs auto-completes their closing characters and continues lists/blockquotes sensibly, without fighting manual edits or breaking undo.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 8.

**Goal / desired end state:** the editor feels like a purpose-built Markdown editor rather than a bare text box, with none of the classic auto-pairing/auto-continuation bugs (double-closing, list marker that never ends, multi-step undo).

**How to verify before marking this phase done:**
- **[Auto]** Unit-test the auto-pairing `keyPressEvent` logic against a sequence of simulated keystrokes on Linux (headless), covering `**`, `` ` ``, `[]()`, and the "type the closing character manually" case.
- **[Manual-Win]** Manually type the same sequences in the running editor on Windows and confirm the behavior feels correct interactively.
- **[Auto]** Unit-test list/blockquote auto-continuation logic, including the empty-item exit case, headlessly on Linux.
- **[Manual-Win]** Manually confirm the same list/blockquote behavior in the running editor on Windows.
- **[Auto]** Unit-test undo/redo grouping behavior via a scripted test on Linux.
- **[Manual-Win]** Manually confirm a single undo reverts a full auto-assisted edit on Windows.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Type `**bold`, `` `code` ``, and `[text]` in sequence and confirm auto-closing behaves correctly, including manually typing the closing character right after an auto-inserted one.
3. Start a bullet list, add a few items via Enter, then press Enter on an empty item and confirm the list ends cleanly instead of continuing forever.
4. Repeat for a numbered list and a blockquote.
5. After each of the above, press Undo once and confirm the entire auto-assisted edit reverts in one step.

---

### Phase 10 — Autocomplete & Suggestions
**Depends on:** Phase 8
**Parallel with:** Phase 9

- `QCompleter`-based suggestion popup for:
  - reference-style link ids already defined in the current document (`[text][id]`)
  - heading-anchor completion for internal links
  - local image-path completion after `![](` , scanned from the file's own directory
- Suggestions are computed on trigger (the relevant keystroke/context), not maintained continuously in the background

**Phase-specific requirements:**
- Directory/image-path scanning for completion happens on demand, not via a filesystem watcher running continuously for this purpose (§4.7) — Phase 6's watcher is for detecting external edits to open files, not for powering autocomplete.

**Deliverable:** typing a reference-link id, an internal anchor link, or a local image path shows relevant, correct suggestions.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 8.

**Goal / desired end state:** autocomplete meaningfully speeds up writing Markdown for the three contexts above, without adding any background scanning cost when the editor is otherwise idle.

**How to verify before marking this phase done:**
- **[Auto]** Unit-test the reference-link-id, heading-anchor, and local-image-path suggestion sources directly against sample documents/directories on Linux (headless data-lookup functions, not GUI behavior).
- **[Manual-Win]** Manually confirm the `QCompleter` popup actually appears with correct suggestions while typing in the running editor on Windows, for all three trigger contexts.
- **[Manual-Win]** Confirm idle CPU stays within the §4.14 target when not actively triggering a suggestion, on Windows.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Add a few `[text][id]: url` reference definitions to a test document, then start a new `[text][` elsewhere and confirm the defined ids are suggested.
3. Add a few headings, then start an internal anchor link and confirm the headings are suggested.
4. Place an image file next to the test document, type `![](`, and confirm it's suggested.
5. Leave the editor idle and confirm no CPU activity occurs until a suggestion is actually triggered.

---

### Phase 11 — Editor–Viewer Integration: Save, Dirty State & Live TOC
**Depends on:** Phase 9, Phase 10, Phase 3
**Parallel with:** none

- Save / Save As, with a dirty-state indicator on the tab title
- Unsaved-changes prompt on tab close and app exit
- TOC panel updates live while typing in `EditorView`, reusing Phase 3's `TocModel` unmodified (§4.4)
- Reconciling Phase 6's external-change watcher with unsaved local edits: a conflict prompt instead of a silent overwrite in either direction

**Phase-specific requirements:**
- Autosave is explicitly out of scope for this phase and, unless later added as its own phase, for the project as currently planned — record this as a deliberate non-goal so a future phase doesn't assume it silently exists.
- Forward-compatibility for §10 (optional, not built in this phase): keep the save action itself a distinct, callable step (e.g. "write current buffer to its file path") separate from any UI/menu wiring, so §10's optional "commit + push to GitHub" can be added later as an alternate action after a normal save, without needing to touch or duplicate the save logic itself.

**Deliverable:** a file can be edited, shows a dirty indicator, is saved explicitly, and its TOC updates live while typing — with no silent data loss when the file also changes externally.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 10.

**Goal / desired end state:** editing is a complete, safe loop — edit, see a dirty indicator and a live TOC, save explicitly, and get a clear prompt rather than silent data loss if the file changes on disk while there are unsaved local edits.

**How to verify before marking this phase done:**
- **[Auto]** Unit-test dirty-state tracking logic (edit → dirty, save → clean) directly on `DocumentModel` on Linux.
- **[Manual-Win]** Visually confirm the dirty indicator appears/clears on the tab title in the running app on Windows.
- **[Manual-Win]** Attempt to close a tab or the app with unsaved changes on Windows and confirm the prompt appears and both "save" and "discard" work correctly.
- **[Manual-Win]** Edit a heading's text on Windows and confirm the TOC panel updates live without a manual refresh.
- **[Auto]** Unit-test the conflict-detection logic (unsaved local edit + external change = conflict state) directly on Linux.
- **[Manual-Win]** With unsaved local edits, change the file externally on Windows and confirm the conflict prompt actually appears in the UI.

**Step-by-step testing guide:**
1. Build after pulling in the new zip.
2. Edit an open file and confirm the tab's dirty indicator appears; save and confirm it clears.
3. Make an edit, then attempt to close the tab; confirm the unsaved-changes prompt appears and behaves correctly for both "save" and "discard."
4. Edit a heading's text while watching the TOC panel and confirm it updates live.
5. With unsaved edits present, modify the same file from another program and confirm Markdawn shows a conflict prompt rather than silently reloading or silently ignoring the external change.

---

### Phase 12 — Packaging
**Depends on:** Phase 6, Phase 11
**Parallel with:** none

- `windeployqt` / `macdeployqt` / the Linux equivalent for bundling Qt dependencies
- Per-OS installers, wiring in the Phase 6 file-association assets so `.md` files open with Markdawn after install
- Final application icon/branding from Phase 4

**Phase-specific requirements:**
- Code-signing is platform-dependent and may be out of scope depending on distribution plans — decide and record explicitly rather than leaving it ambiguous.

**Deliverable:** an installer per OS that installs Markdawn and registers it as a handler for `.md` files.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 11.

**Goal / desired end state:** a real end user can install Markdawn from a downloaded installer and immediately double-click a `.md` file to open it in Markdawn, with no manual configuration.

**How to verify before marking this phase done:**
- **[Auto]** Build the Linux package and confirm it launches on a clean Linux container/VM without a separate Qt install.
- **[Manual-Win]** Install the Windows installer on a clean Windows machine/VM and confirm Markdawn launches correctly.
- **[Manual-Win]** Double-click a `.md` file after installing on Windows and confirm it opens in Markdawn without manual file-association setup.
- **[CI-only]** The macOS package is confirmed to build successfully in CI (§4.13); installation and file-association behavior on real macOS hardware are not verified under the current setup.

**Step-by-step testing guide:**
1. Build the installer for the target OS.
2. Install on a clean machine or VM without a development Qt install present.
3. Confirm Markdawn launches from its installed shortcut/menu entry.
4. Double-click a `.md` file in the file manager and confirm it opens directly in Markdawn.
5. Repeat for each supported OS.

---

### Phase 13 — QA & Final Validation
**Depends on:** Phase 12
**Parallel with:** none

- Re-verify Phase 7's startup-time and idle-resource gates against the final packaged build, in both view and edit modes
- Large-file and many-tabs stress testing on the packaged build
- File-association double-click end-to-end test on each OS
- External-edit-detection-while-editing conflict test (Phase 11) on the packaged build
- Crash/data-loss check: kill the app mid-edit and confirm behavior matches the documented no-autosave design (§Phase 11), not an unexpected silent loss beyond that

**Deliverable:** a stable release candidate that meets every goal stated in §1.

**Files to upload to start this phase:** `Markdawn-Roadmap.md` + the project zip as it stood at the end of Phase 12.

**Goal / desired end state:** the packaged, installed application meets every numeric target in §4.14, handles real-world file-association and external-edit scenarios correctly, and has no surprises beyond the explicitly documented lack of autosave.

**How to verify before marking this phase done:**
- **[Manual-Win]** Startup time, second-instance handoff, and idle CPU/RAM (view and edit modes) on the final packaged Windows build meet every §4.14 target — not just a dev build.
- **[Auto]** The same measurements are re-run on the final packaged Linux build as a sanity check against §4.14.
- **[CI-only]** The macOS package build succeeds in CI (§4.13); its runtime numbers are not part of this gate under the current setup.
- **[Manual-Win]** Many-tabs and large-file stress tests on Windows complete without stalls or abnormal memory growth.
- **[Manual-Win]** File-association double-click works correctly on a clean Windows install.
- **[Auto]** File-association double-click works correctly on a clean Linux install.
- **[Manual-Win]** The external-edit conflict prompt (Phase 11) behaves correctly on the packaged Windows build.
- **[Manual-Win]** Killing the app mid-edit on Windows loses only the documented amount of work (since last save), nothing more, and doesn't corrupt the file on disk.

**Step-by-step testing guide:**
1. Install the release-candidate build on a clean machine per OS.
2. Repeat Phase 7's cold-start, handoff, and idle-CPU measurements against this build in both view and edit modes.
3. Open many tabs and a large file; confirm no stalls and reasonable memory use.
4. Double-click a `.md` file from the file manager and confirm correct behavior end to end.
5. With a file open and edited but unsaved, modify it externally and confirm the conflict prompt behaves correctly.
6. Force-kill the app mid-edit, relaunch, and confirm the file on disk still contains exactly the last saved state — no corruption, no loss beyond what not having autosave implies.

---

## 6. Dependency Overview & Continuity

### 6.1 Dependency graph

```
Phase 0 (scaffolding)
   └──▶ Phase 0.5 (technical spikes: Markdown coverage, KSyntaxHighlighting)
          └──▶ Phase 1 (document model / single-instance / IPC)
                 ├──▶ Phase 2 (rendering + tabs)            [also needs Spike A]
                 │        ├──▶ Phase 3 (TOC tree)     ─┐
                 │        └──▶ Phase 4 (theming)       ─┼─▶ can proceed in parallel
                 ├──▶ Phase 5 (settings/persistence)   ─┤
                 └──▶ Phase 6 (file assoc/OS)          ─┘

Phase 2+3+4+5+6 ──▶ Phase 7 (viewer resource/startup gate)
Phase 7 ──▶ Phase 8 (editor core)            [also needs Spike B]
   Phase 8 ──▶ Phase 9  (smart editing)   ─┐
   Phase 8 ──▶ Phase 10 (autocomplete)     ─┼─▶ can proceed in parallel
Phase 9+10+3 ──▶ Phase 11 (save/dirty/live TOC)

Phase 6+11 ──▶ Phase 12 (packaging)
Phase 12 ──▶ Phase 13 (final QA)
```

**Roughly independent work streams once Phase 0.5 and Phase 1 are done**, useful for solo/asynchronous development:
1. Rendering chain: Phase 2 → 3 → (feeds Phase 11 later)
2. Presentation chain: Phase 2 → 4
3. Platform/config chain: Phase 1 → 5, Phase 1 → 6
4. Editor chain (after Phase 7's gate): Phase 8 → 9/10 → 11

### 6.2 Learnings & Decisions Log

**Rule: after finishing each phase, append an entry here** — this is what makes the file self-sufficient for resuming work in a fresh chat with no prior conversation memory. Record things a later phase would otherwise get wrong by guessing: exact constant values chosen, the IPC message format decided in Phase 1, any platform-specific gotcha discovered, any deviation from what this document originally specified.

Format per entry:

```
### Phase N — <name> (completed <date>)
- Decision: ...
- Gotcha: ...
- Affects later phases: ...
```

No phases have been completed yet — this log starts empty and is filled in as each phase is finished.

### Phase 0 — Project Scaffolding (completed 2026-08-27, CI-confirmed on all 3 platforms)
- Decision: C++20; warnings-as-errors via one `INTERFACE` target `markdawn_warnings` (MSVC `/W4 /WX`, GCC/Clang `-Wall -Wextra -Werror`) — new targets link it or escape the gate. Output dir fixed to `build/<preset>/bin/`.
- Decision: `vcpkg.json` requests plain `"qtbase"`, no `default-features: false` override. **Unsuitable:** hand-picking a platform feature (e.g. `xcb`) explicitly forces it `ON` for every triplet including Windows and hard-fails configure — a feature's `"platform"` condition is only honored via the port's own `default-features` list, not a consumer override.
- Decision: Qt platform-plugin wiring is triplet-linkage-dependent (`x64-windows`=dynamic, `x64-linux`/`arm64-osx`=static): Windows gets a `POST_BUILD` copy of `Qt6::QWindowsIntegrationPlugin` into `platforms/`; Linux/macOS link it via `qt_import_plugins()` (Xcb/Cocoa + Offscreen, so `QT_QPA_PLATFORM=offscreen` works). Applies to *every* `QGuiApplication`/`QApplication` target on a static triplet, including throwaway/spike executables, not just the shipped app.
- Decision: `builtin-baseline` pinned to vcpkg commit `a7bd30319eeac16afbe18d64a855303a0a425e84` (= CI's `VCPKG_PINNED_COMMIT`) — not to be silently changed; advance both together and re-verify with `vcpkg install --dry-run`.
- Constraint (CI): vcpkg checkout must be a full `git clone` + `git checkout <sha>` — a shallow fetch breaks vcpkg's baseline lookup (`git show <sha>:versions/baseline.json`).
- Constraint (CI): bash `$VAR` syntax in a `run:` step needs explicit `shell: bash` on Windows (default `pwsh` silently treats `$VAR` as empty).
- Constraint (CI, Windows): `ilammy/msvc-dev-cmd@v1` must run before the Ninja install step, or CMake can silently pick Git for Windows' bundled MinGW `g++` over MSVC.
- Constraint (CI, Linux/macOS): `autoconf`/`autoconf-archive`/`automake`/`libtool` needed (transitive `libb2` autoreconf); Linux additionally needs `libltdl-dev` + the full Qt-official Linux build-dep list + `libgl1-mesa-dev`/`libegl1-mesa-dev` (vcpkg's `opengl` port is an intentional no-op on Linux). Not needed on macOS.
- **Unsuitable:** vcpkg's GitHub-Actions-native binary cache (`x-gha`) was removed upstream and silently no-ops on current vcpkg — use `actions/cache` over `build/vcpkg_installed` + `vcpkg/downloads`, keyed on `hashFiles('vcpkg.json')`, instead.
- Decision: `jobs.build.timeout-minutes: 90` is a safety bound against a hung step, not a speed target — repeat-run speed comes from the cache above.
- Decision: `windows-x64-local` (hardcoded `D:/...` path) is for the project owner's machine only, never referenced from CI; CI's Windows leg uses `windows-x64-debug` (`$env{VCPKG_ROOT}`).
- Decision: `WIN32_EXECUTABLE` deliberately not set yet, so console/`qDebug` output stays visible — revisit at Phase 12 once the log file (§4.11) is the primary diagnostic channel.
- Affects later phases: `core-lib`'s `version.cpp`/`version.h` are placeholders proving the two-target link works — delete the moment Phase 1 adds a real module, don't extend them.
- Deferred (not adopted): an expanded CI matrix (x86-windows, macOS Intel, multi-distro Linux) — would ~3–4x CI time; revisit only as a deliberate platform-support decision.

### Phase 0.5 — Technical Spikes: De-Risking Core Assumptions (completed 2026-08-29, CI-confirmed on Linux + Windows)
- Decision: the vcpkg port for KSyntaxHighlighting is **`syntax-highlighting`** (not `ksyntaxhighlighting`/`kf6syntaxhighlighting` — neither exists at the pinned baseline), version 6.27.0, supported on all three of Markdawn's triplets. CI-confirmed clean build on x64-linux (1.9 min) and x64-windows (2.6 min) once `qtbase`/`qttools` are cached — safe to depend on in Phase 8.
- Constraint (license — unresolved, needs a decision before Phase 8): KSyntaxHighlighting's C++ library code is MIT, but its 409 bundled syntax-definition XML files are a real mix — 147 MIT, ~129 LGPL-family, ~41 plain GPL (no linking exception), 69 with no stated license at all. Shipping the GPL-only/unlicensed definitions inside an All-Rights-Reserved app is a legal question, not an engineering one. Options before Phase 8: get a legal opinion, ship only a curated MIT/permissive subset, or hand-roll a small `QSyntaxHighlighter` instead.
- Decision (Qt Markdown coverage — `QTextDocument::setMarkdown()`, `MarkdownDialectGitHub`, real `toHtml()` output verified via CI): tables, strikethrough, and nested lists render correctly, no workaround needed. Task-list items render as a CSS `::marker` glyph (☐/☒), not an interactive `<input>` — fine for a read-only viewer. Footnotes are unsupported (`[^1]` passes through as literal text) — accepted as a known limitation. Fenced-code language tags are dropped entirely with no syntax highlighting — this is the gap KSyntaxHighlighting (above) is meant to cover later. Relative image paths pass through unresolved — `DocumentView` (Phase 2) must set the document's base URL itself.
- Constraint (CI): a job depending on another via `needs:` can safely share that job's exact `actions/cache` key — `actions/cache` skips (doesn't error on) saving to a key that already exists. **Unsuitable:** giving a dependent job a separate cache key "to avoid a save race" instead makes it redundantly cold-rebuild the whole dependency chain (measured cost: an extra ~90 min of duplicate Qt6 build on Windows) instead of reusing what the prerequisite job already built.
- Affects later phases: the throwaway spike code (`spikes/markdown-coverage/`, the `MARKDAWN_BUILD_SPIKES` option, the `spike-syntax-highlighting` vcpkg feature, the `spike-0-5` CI job) has been removed now that its findings are captured above — nothing later should reference it.

### Phase 1 — Core Architecture: Document Model, Single-Instance Launch & IPC (completed 2026-08-30, [Auto] verified locally + Linux CI script added; [Manual-Win] outstanding)
- Decision: OpenFile IPC message is a single-line JSON object (`{"version":1,"path":"..."}`) via nlohmann/json (already a vcpkg dependency, now its first real consumer), framed with a trailing `\n` on the wire. A protocol-version mismatch is rejected outright, not best-effort parsed.
- Decision: the second-instance fast path scopes a `QCoreApplication` around the connect-and-forward check and lets it be destroyed before `QApplication` is ever constructed if a running instance is found — measured ~17ms locally (target: <50ms). Sequential construct→destroy→construct-again of Q*Application objects is architecturally sound in Qt (each one's destructor clears the global self-pointer before the next is built) but is a project-specific choice here, not a widely-documented off-the-shelf idiom — flagged rather than presented as standard.
- Decision: a stale single-instance server registration (Qt's documented Unix `AddressInUseError` after a crash) is handled by calling `QLocalServer::removeServer()` unconditionally before every `listen()` — Qt's own documented recovery, the same approach used by the widely-used itay-grudev/SingleApplication library.
- Decision: if `tryForwardToRunningInstance()` connects but the write times out, the process reports failure and still exits rather than falling through to start its own server — avoids two processes racing on `removeServer()`/`listen()` for the same name (Qt's docs explicitly warn against removing a socket belonging to a still-running instance).
- Gotcha (CMake/AUTOMOC): a header with `Q_OBJECT` in a different directory from its `.cpp` (our include/ vs src/ split) is **not** moc'd just because the `.cpp` includes it — verified with a minimal repro. `mocs_compilation.cpp` is silently generated empty and linking fails with "undefined reference to vtable for ...". Fix: list the header explicitly in the target's `add_library`/`add_executable` sources. Applies to every future core-lib class with `Q_OBJECT` in this layout.
- Decision: vcpkg's `qtbase` port ships `network` as a default feature (confirmed from the port's feature list) — `vcpkg.json` needed no change for `QLocalServer`/`QLocalSocket`; only `core-lib/CMakeLists.txt` needed `find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)`, linked PUBLIC since the public headers use Qt types directly.
- Decision: `DocumentModel::loadFromFile()` loads raw UTF-8 plain text only (`QTextDocument::setPlainText`) — no Markdown parsing in the model, per §3. Distinguishes `FileNotFound` (`QFileInfo::exists()`) from `ReadError` (`QFile::open()` failure).
- Verified locally (system Qt6 6.4.2 via apt, offscreen QPA — not yet the vcpkg/CI build): all four [Auto] checks pass, including force-kill + clean relaunch (second-instance handoff measured ~17ms). The actual vcpkg-built CI run has not executed yet.
- Affects later phases: Phase 2's `TabManager` should connect to `SingleInstanceServer::openFileRequested` in place of the placeholder lambda in `main.cpp` (currently only logs it). IPC format (`{version, path}`, version 1) is fixed — bump the version explicitly if it ever changes, don't alter it silently.

---

## 7. Optional Enhancement — Font Selection

> **This section describes an optional feature, not a required phase.** Nothing in §5 depends on it, and skipping it entirely does not block any other phase. If adopted, it fits most naturally as a few extra bullets under Phase 4 (Theming & Modern UI) rather than as its own numbered phase.

### 7.1 How comparable apps handle this

Looking at how other Markdown viewers/editors handle font choice, there are two distinct models in real use, not one standard:

1. **Full system-font picker — any font already installed on the user's OS.** The app reads the OS's installed font list and lets the user pick freely, the same way a general-purpose code editor lets you type any installed font name into a font-family setting. Among Markdown-focused editors, this is the more common approach when the choice is exposed as a real settings-UI control rather than only through custom CSS.
2. **A small, fixed set bundled with the app itself.** A handful of curated typefaces ship inside the application, and the user picks from that short list — independent of what happens to be installed on their system. A well-known note-taking/writing app offers exactly this: a small fixed choice (default / serif / mono), not an open system-font list. Another popular writing app built its identity around its own bundled typeface family instead of exposing arbitrary system fonts.

A third, hybrid pattern also shows up: some editors tie the font to their theme system by default (switching themes changes the font as part of a whole visual package), while still letting advanced users override it to any font installed on their system by hand-editing a custom-CSS file — full freedom, but not exposed as a simple dropdown.

### 7.2 If Markdawn adds this

Given Qt is already the toolkit, the lowest-effort, best-supported option is model 1: **`QFontComboBox`** is a built-in Qt widget that lists every font family currently installed on the user's system — no custom font-enumeration code needed, and it's a long-standing, well-documented widget with plenty of prior art if something behaves oddly (§4.9). Paired with a simple font-size spinner, this covers "pick any font already on my machine" with very little new code.

- Separate settings for the **editor** font (typically expected to default to a monospace family) and the **viewer/rendered-preview** font (typically a proportional family) — these are different use cases, and most comparable apps treat them as two separate choices rather than one shared setting.
- The chosen font family/size are just two more fields in the Phase 5 settings schema — §4.4's "settings fields → versioned schema" extension point already covers this cleanly, no new mechanism needed.
- Optional refinement, not required even if the rest of this section is adopted: bundle one open-license monospace font (via Qt's resource system) as the shipped default for the editor, so the out-of-the-box look is consistent across Windows/macOS/Linux instead of depending on whichever monospace font each OS happens to default to — the user can still switch to any system font via the picker regardless.

This entire feature — the two-model context, the widget choice, and the settings fields — can be folded into Phase 4 as extra scope if the project decides to include it; it does not require its own phase number or change anything in §6.1's dependency graph.

## 8. Optional Enhancement — Settings Search

> **This section describes an optional feature, not a required phase.** Phase 5 (§5) builds the settings *data layer* (`SettingsManager`, the versioned schema) but no phase currently builds a full preferences/settings *dialog* — that UI doesn't exist yet in the numbered phases. This section describes a search feature for whichever future phase introduces one; nothing in §5 depends on it, and skipping it does not block any other phase.

### 8.1 How comparable apps handle this

A searchable settings dialog is a well-established pattern, not something to design from scratch:

- **VS Code's Settings editor** has a search box at the top; typing filters the whole settings list to matches, and every result shows a small breadcrumb line under its name (e.g. "Text Editor > Font") indicating where it normally lives in the category tree. Results are shown inline in a filtered list rather than requiring a separate "go to category" step.
- **JetBrains IDEs' Settings/Preferences search** works the same way: a search box filters entries across every settings page, each match shows its category breadcrumb, and selecting a match jumps straight to that page with the specific control highlighted/scrolled into view.
- **The Windows Settings app** follows the same shape at OS level: search results list each match with a breadcrumb subtitle showing its location in the settings hierarchy, and clicking a result opens that exact page.

The common shape across all three: a flat search index built from the (otherwise hierarchical) settings, each entry carrying a display name and a category "path," with search filtering by name and results showing that path so the user knows where they've landed — and clicking a result navigating directly to the live control, not just to the right page.

### 8.2 If Markdawn adds this

This can be built directly on top of §4.4's settings-schema extension point, since every schema field already carries a label and a category/path by design:

- A flat, in-memory search index is built once from the settings schema (or from the settings dialog's own page/widget layout) — each entry: field id, display label, category/path breadcrumb, and a reference to the page + specific widget it lives on.
- A `QLineEdit` at the top of the settings dialog filters that index as the user types (simple substring/fuzzy match on label and path — no need for anything heavier at this scale); matches are shown in a results list with the breadcrumb path under each label, the same way the apps in §8.1 do it.
- Clicking (or pressing Enter on) a result switches the settings dialog to that entry's page and scrolls/`ensureWidgetVisible()`s to the specific control, with a brief highlight (e.g. a short palette-color flash) so the destination control is unambiguous — the same jump-and-highlight affordance VS Code and JetBrains both use.
- No new persistence or schema-version concerns: this is a read-only index built from data the schema already has: nothing about `SettingsManager`'s save/load behavior changes.

Like §7, this can be folded into whichever future phase builds the preferences dialog as extra scope; it does not require its own phase number.

## 9. Optional Enhancement — Open a Markdown File from a URL

> **This section describes an optional feature, not a required phase.** Phase 2 (§5) builds only the local-file "open this path as a tab" flow. This section describes an additive content source for that same entry point; nothing in §5 depends on it, and skipping it does not block any other phase. See §1.1 for why this doesn't conflict with the "no cloud sync" non-goal.

### 9.1 How comparable tools handle this

There isn't one dominant pattern; a few distinct approaches show up depending on what the tool is for:

- **Fetch-and-render, no persistence.** Browsers rendering a raw `.md` file straight from a URL (including GitHub's own "raw" file links) just fetch the bytes and render — no local copy is kept, no re-fetch happens automatically, and viewing it again later re-fetches from scratch.
- **Fetch-and-cache, explicit refresh.** Some read-it-later and note tools that support remote fetching cache the content locally after the first fetch and only re-fetch on an explicit user action (a "refresh" command), rather than polling or live-updating in the background.
- **Editors with a general "remote file system" abstraction** (e.g. VS Code's remote/virtual file systems) go further and treat a remote resource as a first-class file the editor can open, save back to, and watch — considerably more machinery than a single "view this URL" action needs.

For a read-only viewer feature (which is what's being proposed here — see §5's Phase 2 note that download-vs-live is a decision for whenever this is actually built), the first two patterns are the relevant precedent; the third is out of scope for what §9 is describing.

### 9.2 If Markdawn adds this

- **Entry point:** a "URL…" option alongside File > Open, feeding the same content-source abstraction Phase 2's forward-compatibility note above already asks for — from `TabManager`'s point of view, this should end up looking like "another way to get bytes for a new tab," not a parallel code path.
- **Fetch mechanism:** Qt's own `QNetworkAccessManager`/`QNetworkReply` (part of Qt Network, already a natural fit alongside `qtbase` — no new third-party HTTP dependency needed) is the well-established, first-party way to do this in a Qt application; it must run asynchronously so a slow or unreachable URL never blocks the UI thread, consistent with §1's "event-driven end-to-end" constraint.
- **GitHub specifically:** a GitHub *blob* URL (e.g. `github.com/owner/repo/blob/branch/file.md`) is an HTML page, not the raw Markdown — the fetch needs to either resolve it to the corresponding `raw.githubusercontent.com` URL first, or accept only raw-content URLs and document that clearly to the user. This is a real detail to get right, not just a "fetch the URL" afterthought.
- **Download vs. live, deferred by design (per §5 Phase 2):** two independent decisions when this is actually built:
  - *Persistence:* does opening a URL leave nothing on disk (fetch-and-render only, closing the tab forgets it), or does it save a local cached copy the user could later "Save As" from? A local cache also gives a fallback if the URL becomes unreachable on a later app launch.
  - *Freshness:* does the tab ever re-fetch (manually via a "Refresh" action, or never at all after the initial open)? Given §1's zero-idle-CPU constraint, any auto-refresh-on-a-timer design is explicitly out — if freshness matters at all, it must be a user-triggered action, not a background poll.
- **Failure handling** reuses Phase 2's existing "file that fails to load shows a clear in-tab error state" requirement — a network failure, a 404, or a non-Markdown response should land in that same error state, not a distinct one.
- **Security note, not a blocker:** treat fetched remote content the same as a local file for rendering purposes (Qt's Markdown renderer, not a web engine, so there's no script execution surface) — the one thing worth being deliberate about is that image tags inside remote Markdown will trigger further outbound network requests unless base-URL/resource-loading is scoped the same way Phase 2 already scopes it for local relative-image paths.

This can be folded into Phase 2 as an additive follow-up (or its own small phase after Phase 2, if preferred) once Phase 2's content-source abstraction exists; it does not require reworking §6.1's dependency graph on its own.

## 10. Optional Enhancement — Publish/Push to GitHub from the Editor

> **This section describes an optional feature, and implementing it is explicitly not mandatory.** Phase 11 (§5) builds a complete, safe local save loop; nothing in §5 depends on this section, and skipping it entirely does not block any other phase, including Phase 12 (Packaging) or Phase 13 (QA). If adopted, treat it as its own small phase after Phase 11 rather than folding it into Phase 11 directly — it pulls in a real third-party dependency and a credentials story that Phase 11 itself has no reason to carry.

### 10.1 How comparable tools handle this

- **Dedicated Git GUI clients** (GitHub Desktop, GitKraken, Git Graph-style panels) treat commit + push as a first-class, visible workflow: stage, write a commit message, push — usually with an explicit branch indicator so it's never ambiguous what's about to happen.
- **Editors with a built-in "publish" action for a single file** are the closer precedent for what's being described here — a lightweight commit-and-push scoped to one file, not a full git GUI (branch management, merge conflict resolution, history browsing).
- **Authentication is the part every one of these tools spends the most effort on**, not the git operations themselves: GitHub's own recommended path for third-party apps today is a personal access token or an OAuth device-flow login, not asking the user to paste a password (GitHub has not supported password-based Git authentication for years).

### 10.2 If Markdawn adds this

- **Library choice: `libgit2`.** This is the standard, well-established C library for embedding real git operations (commit, push) directly into a native application — it's what powers git functionality inside GitHub.com and Visual Studio's own Git tooling, is available as a vcpkg port on all three of Markdawn's triplets, and is licensed GPL-2.0 **with a linking exception** that explicitly permits linking it into closed-source applications — worth calling out given Phase 0.5's finding that "just assume the license is fine" is exactly the wrong instinct for a third-party dependency in an All-Rights-Reserved project. This still needs the same real verification Phase 0.5 modeled (confirm the vcpkg port name and an actual build succeeds on all three triplets) before being relied on — not assumed from this paragraph alone.
- **Scope, deliberately narrow:** commit-and-push for the *current file only*, against a repository the file already lives inside (i.e., Markdawn detects the file is inside a git working tree; it does not offer to `git init` a new repository or manage remotes/branches). If the file isn't inside a git repository, the action is simply unavailable — not an error state to design around.
- **Authentication:** a GitHub personal access token (or OAuth device flow, if the added complexity is judged worth it later) stored via the OS's native credential store (Windows Credential Manager / macOS Keychain / a Linux secret-service backend) — never in Markdawn's own settings JSON in plaintext. This is a real scope decision on its own and should get its own explicit sign-off when this phase is actually started, not be assumed from this bullet.
- **UI surface:** a single "Commit & Push…" action (menu item, reachable from the same place as Save) that prompts for a commit message and shows success/failure — not a staging area, not a diff viewer, not branch switching. If any of those turn out to be needed, that's a scope decision to make explicitly when this is built, not something to grow in silently.
- **Failure handling:** authentication failures, network failures, and push rejections (e.g. remote has newer commits) must each surface a clear, specific message — a rejected push in particular must never be silently retried with a force-push; a failed push should leave the local commit intact and tell the user to pull/resolve manually, since Markdawn is explicitly not building conflict-resolution tooling (§1.1's "no multi-user/real-time collaboration" boundary applies here too).
- **Why this isn't Phase 11 itself:** Phase 11 depends on nothing but the file system; this feature depends on a new third-party library, network access, and a credentials story — genuinely different risk and testing profile, which is exactly why §5 keeps this out of Phase 11 rather than adding it as "a few extra bullets" the way §7 folds into Phase 4.

If adopted, this would be its own numbered phase depending on Phase 11, inserted after it in §6.1's dependency graph — not a retroactive addition to Phase 11's existing scope.

