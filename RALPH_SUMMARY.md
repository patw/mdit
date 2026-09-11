# RALPH_SUMMARY.md — mdit

Final summary for the **mdit** build loop. This file is the standing
"the loop is done" marker that the last subtask (Final polish) produces.

## What was built

**mdit** is a C++20 / **Qt6 (QtWidgets, pure Qt)** / CMake desktop
specialty markdown editor. A `.md` / `.markdown` file opens into a 50/50
split pane: a syntax-highlighted `QPlainTextEdit` editor on the left and a
live-rendered preview on the right, rendered by Qt's built-in
`QTextDocument::setMarkdown()` with the full CommonMark/GFM feature set.
No QtWebEngine, no QtMultimedia, no QML, no external markdown library.

## Feature checklist (all complete, all tested)

- **Document model** (`Document`, pure): UTF-8 load (`\r\n`→`\n` normalize),
  save (detected line ending restored, default `\n`), dirty transitions,
  title / next-untitled.
- **Rendering** (`RenderedDocument`, pure): `QTextDocument::setMarkdown()`
  with the FULL flag set (Tables, CommonMark, + available feature enums),
  base-URL-relative image resolution, light/dark stylesheet; `PreviewPane`
  widget hosting the doc with scroll-ratio sync.
- **Editor** (`MarkdownHighlighter` + `EditorPane`): theme-aware highlighting
  for h1–h6, emphasis/strong, inline + fenced code, links, blockquotes,
  horizontal rules, list markers; a line-number margin that tracks vertical
  scroll/font/resize; word/char counts.
- **Live preview** (`MarkdownModel` + `PreviewDebouncer`, pure): heading
  extraction, word/char counts, debounce policy; wired so a re-render is
  scheduled (~150 ms after the last keystroke) **only while the preview is
  visible**, base URL from the current file, feedback-guarded scroll sync.
- **Save / dirty / title** (Ctrl+S / Ctrl+Shift+S): Save As prompt when
  untitled, `*` in the title when dirty, status-bar feedback, dirty guard on
  New/Open/Exit.
- **Find / Replace** (Ctrl+F / Ctrl+H): overlay bar, `"i of n"` match count,
  next/prev with wrap, case toggle, Replace (advances) + Replace All.
- **Theme & settings** (`Settings`, `Theme`): typed `QSettings` accessors
  (theme auto/light/dark, previewVisible default true, splitterRatio default
  0.5, recentFiles cap 5); Fusion + light/dark palette + QSS applied to the
  whole app and re-applied to the preview on toggle; `Ctrl+T` toggle.
- **Preview toggle + status bar** (View → Preview, default ON): hide/show the
  preview pane, editor expands to full width, persisted ratio restored;
  debounced word/char count + live modified indicator in the status bar.
- **Export** (HTML / PDF): both reuse the *same* `RenderedDocument` path the
  preview uses; standalone theme-styled HTML with relative images copied next
  to the file; PDF printed into a `QPdfWriter` (A4 portrait, set DPI). Exports
  never mutate the dirty state.
- **Recent files**: `File → Recent Files` submenu of the last N persisted
  paths, dirty-guarded reopen.
- **CLI + entry point**: `mdit <file>.md` opens an existing file else starts
  untitled; drag-and-drop of a `.md` / `.markdown` file onto the window.

## Test suite (green)

18 CTest / QtTest binaries, all run headless under the Qt **offscreen**
platform and exercising the module classes directly (the GUI is never shown):

`scaffold, smoke, document, opennew, render, previewpane, highlighter,
editor, markdownmodel, livepreview, save, settings, theme, mainwindow,
export_html, export_pdf, entrypoint, github_ready`.

`test_github_ready` guards the repository contract (README / LICENSE /
CHANGELOG / spec / CMakeLists). The pytest shim `tests/test_build.py`
(configure → build → ctest) is the harness entry point (`pytest -q`).

## How to build & run

```sh
# Linux (Ninja)
cmake -S . -B build -DBUILD_TESTS=ON -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure   # or: pytest -q

# run the app
./build/mdit                 # or: ./build/mdit somefile.md
```

Prereqs: a C++20 toolchain, CMake, Ninja, and Qt6 with the **Widgets**,
**Gui**, **PrintSupport**, and **Test** components.

## Repository contract (GitHub-ready)

- `README.md` — purpose, features, build/run, usage/shortcuts, license.
- `LICENSE` — MIT, `Copyright (c) 2026 Pat Wendorf`.
- `CHANGELOG.md` — Keep a Changelog, everything under `## [0.1.0]`.
- `spec.md`, `implementation.md`, `AGENTS.md`, `.gitignore`, `build.sh`,
  `tests/test_build.py`, `RALPH_SUMMARY.md` (this file).

## Result

All subtasks in `implementation.md` are checked `- [x]`; the full suite is
green from a clean configure+build; the repository is committed and
GitHub-ready.
