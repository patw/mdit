# spec.md — mdit

A **speciality markdown editor** written in **C++20 / Qt6 (QtWidgets)**. It loads
**only markdown files** and presents a **50/50 split-pane view** by default:
the editor on the left, a **live-rendered preview** on the right. It is built to
be **lightning fast and responsive** — a single-purpose editor, not a kitchen
sink. It drops into `~/Personal/mdit`.

The point: open a `.md` (or `.markdown`) file, edit instantly with a live
preview, save, and export when needed. Few moving parts, no network, minimal
dependencies (pure Qt6 — **no QtWebEngine, no external markdown library**).

## Purpose
A fast, distraction-free markdown authoring tool. Load, edit, preview live, save,
and optionally export to HTML/PDF. Keyboard-first, tiny footprint, instant.

## Supported I/O
### Input (must open)
- **`.md` and `.markdown`** files only (the open dialogs filter on these; other
  extensions are not offered).
- Loaded via **streaming** if the file is large (`QFile` read into a `QString`),
  normalized to `\n` line endings for consistent behavior.
- Command-line argument: `mdit <path>.md` opens the file if it exists (and is a
  `.md` / `.markdown` file); a bad (missing, non-file, or non-markdown) or absent
  arg leaves the app in the empty (untitled) state.

### Output
- **Save** (`Ctrl+S`) writes back to the current file path (UTF-8).
- **Save As** (`Ctrl+Shift+S`) writes to a chosen path, defaulting the suggested
  name/extension from the current document.
- **Export → HTML** writes the rendered preview to a standalone `.html` file.
- **Export → PDF** renders the document to a `.pdf` file via `QTextDocument::print`
  / `QPdfWriter`.

## Key non-functional requirements
- **Lightning fast & responsive:** typing must never lag. The editor is a
  `QPlainTextEdit` (fast for large plain text). The **live preview is debounced**
  (e.g. ~120–200 ms after the last keystroke) and re-rendered on the GUI thread
  only when the document actually changed, and only when the preview pane is
  visible — so a huge file or hidden preview never burns time. No per-keystroke
  blocking work.
- **Instant launch:** no heavyweight engines, no static build step beyond the
  compile — a cold start shows an empty split view immediately.
- **Single-file focus:** one editor, one document. No multi-tab requirement
  (multi-tab is a non-goal). A dirty-document guard prompts before New/Open/Close
  discard unsaved changes.

## Architecture (modules)
```
src/
├── main.cpp                 QApplication, theme wiring, single-file CLI open, shows MainWindow
├── MainWindow.{h,cpp}       QMainWindow: menu bar + status bar (NO toolbar),
│                            QSplitter(editor|preview),
│                            dirty tracking, title bar, open/save/save-as/new/export/theme/preview-toggle,
│                            close-dirty guard
├── Document.{h,cpp}         non-QWidget document model: loads/saves text (UTF-8, \n normalize),
│                            holds currentFilePath, dirty flag, title; NEWline/CRLF handling
│                            (does NOT own the editor widget)
├── EditorPane.{h,cpp}       QPlainTextEdit subclass + line-number area + markdown syntax
│                            highlighter; textChanged signal, find/replace, line-numbers margin,
│                            word/char count (also a lightweight Folding/block API for tests)
├── MarkdownHighlighter.{h,cpp} QSyntaxHighlighter for markdown (headings, emphasis, bold, inline
│                            code, code fences, links, blockquotes, hr, lists) — purely cosmetic
├── PreviewPane.{h,cpp}      QTextBrowser/doc that renders markdown→HTML via QTextDocument::setMarkdown
│                            (full feature flags: Tables, code blocks, links, images), resolves relative
│                            image paths against the document's base URL, exposes the rendered QTextDocument
│                            for export + the scroll-ratio sync
├── RenderedDocument.{h,cpp} pure, GUI-independent markdown→QTextDocument renderer (uses QTextDocument
│                            which is non-widget/fast), so export (HTML/PDF) and preview share one path;
│                            builds the base URL for image resolution, injects a light stylesheet
├── AppIcons.{h,cpp}        the application/window icon (the Pengy-style white disc + '#'
│                            mark), compiled in from assets/icons via resources/mdit.qrc
├── AppInfo.{h,cpp}         the identity strings (name, version from CMake, tagline,
│                            ©/MIT line, 'mdit is a Catbee project' + https://catbee.ca)
├── AboutDialog.{h,cpp}      Help > About: a small themed box (mark, name+version, tagline,
│                            ©/MIT line, a clickable Catbee link), window-modal, re-centered
├── Theme.{h,cpp}             the theming system: named-token palettes generated from
│                            mode (System/Light/Dark) x accent (Default+7 colours),
│                            the QPalette + application QSS builders, the UI-scale
│                            helpers (scaleFactor/scaled/scaledFont) and apply()
│                            (Fusion + palette + scaled app font + QSS)
│                            + a persisted setting key
├── Settings.{h,cpp}          QSettings wrapper (org/app): theme (auto/light/dark), previewVisible,
│                            splitter ratios, recent-files list; load/save typed accessors
└── MarkdownModel.{h,cpp}     pure logic: heading extraction (for an optional outline + content test),
                             word/char counting, preview debounce policy (pure, testable)
```
Plus:
```
tests/            (see Verification) — C++ QtTest binaries run by CTest + a pytest build shim
resources/        optional app icon + stylesheet (light.qss / dark.qss)
README.md, spec.md, implementation.md, prompt.md, AGENTS.md, .gitignore
CMakeLists.txt
```
> **Separation of concern for testability:** the preview renderer is split so the
> **pure** `RenderedDocument` (markdown→QTextDocument) lives apart from the
> widget-only `PreviewPane`. Tests exercise `RenderedDocument` for content and the
> `EditorPane`/`Document` for editing/file behavior **without opening a real window**
> where possible (`-platform offscreen` when a `QApplication` is needed).

## Detailed behavior
1. **Start** — `main.cpp` applies the persisted theme (defaulting to system /
   auto), creates `MainWindow`, and if a valid `.md` path was passed as argv[1]
   (and exists) opens it. The window shows an empty split view (preview pane
   shows an empty render) and the title reflects `untitled`.
2. **Split view** — a `QSplitter` with 50/50 initial sizes; **editor on the left,
   preview on the right**; the splitter handle is draggable and the ratio is
   persisted. The **preview is ON by default** and can be hidden via a
   menu action (`Ctrl+Shift+P` or a View menu checkbox). When hidden, the
   editor takes the full width; when shown again, the remembered ratio is
   restored.
3. **Editing** — the editor is a `QPlainTextEdit` with a **markdown syntax
   highlighter** and a **line-number margin** that tracks the vertical
   scrollbar. Typing marks the document dirty and schedules a **debounced** live
   preview update (only when the preview is visible). **Word/char counts** in the
   status bar update on the same debounce; a **modified/unmodified indicator**
   also lives in the status bar and flips live (O(1)) on every dirty-state
   change. The count readout stays current even while the preview is hidden —
   only the expensive *render* is visibility-gated, not the cheap O(n) count, so
   the debounce timer runs regardless of preview visibility.
   **Undo / redo** (`Ctrl+Z` / `Ctrl+Shift+Z`, plus `Ctrl+Y`; `Edit → Undo` /
   `Redo`, enabled only while the corresponding stack is non-empty) is a
   first-class editing behavior: every edit is a stepping stone, **Replace All
   is one single undo step**, a programmatic load (open / new) starts a fresh
   history, and the **dirty flag is undo-aware** — undoing back to the loaded or
   most recently saved content clears dirty (the `*` leaves the title and the
   status bar reads "Unmodified"), while undoing *past* the last save leaves it
   dirty. The undo keys are shortcut-scoped to the editor pane so the find
   bar's search/replace fields keep their own native undo.
4. **Live preview** — on a debounced change, `RenderedDocument` re-renders the
   editor text to a `QTextDocument` (via `QTextDocument::setMarkdown` with the
   full feature set), assigns the document's source directory as the **base URL**
   so **relative image paths resolve**, sets the theme-aware stylesheet, and the
   `PreviewPane` shows it. Scrolling the preview **syncs the scroll ratio** to the
   editor (and vice-versa) so the reader stays roughly aligned with the line being
   edited (a best-effort sync, not perfect).
5. **Open** — `File → Open` (`Ctrl+O`), the **command line** (`mdit <path>.md`),
   and **drag-and-drop** of a `.md`/`.markdown`
   file onto the window all share one path. The `Document` loads the file (streaming, `\n`
   normalization), sets `currentFilePath`, resets dirty to false, and the editor +
   preview fill from it; on success the status bar shows the opened path
   (`Opened <path>`). Protected by the dirty-document guard. The command line
   loads only `.md`/`.markdown` (the pure `AppCli::fileToOpen()` decision); any
   other argument is a "bad arg" that leaves the app untitled.
6. **Save / Save As** — Save (`Ctrl+S`) to the current path (or prompts Save As if
   untitled); Save As (`Ctrl+Shift+S`) to a chosen path. Writes **UTF-8**, chooses
   line endings from the loaded content (`\n` or `\r\n`) or defaults to `\n` for a
   new document. Clears dirty, updates the title.
7. **New** — `File → New` (`Ctrl+N`) resets to a blank untitled document (dirty
   guard first), resets the preview to empty.
8. **Find / Replace** — `Ctrl+F` (find), `Ctrl+H` (find+replace) over the editor
   text, presented as a **small non-modal popup WINDOW centered over the app**
   (a `QDialog` with its own title bar naming the mode — "Find" /
   "Find and Replace" — not a bar glued under the menu bar), with the standard
   highlight + match-wrap behavior, next/prev, match-case, and
   replace/replace-all (Replace All = one undo step). It re-centers every time
   it is shown and whenever it grows; Esc (or the close button / the title-bar
   X) hides it and returns the focus to the editor.
9. **Theme (modes x accents x UI scale)** — a **token-based** palette generator
   (adapted from PengyCPP's `themehelper.h`): `Theme::make(dark, accent)` builds a
   complete named-token palette (`bg`/`fg`/`panel`/`primary`/`doc_link`/…) from a
   **mode** (**System** / Light / Dark — System follows the platform colour
   scheme) x an **accent** (**Default** + Blue/Teal/Green/Orange/Red/Pink/Purple),
   giving 16 palettes. The `Default` accent reproduces the original neutral
   colours exactly; the others tint the surfaces toward the accent and set
   `primary`/`selection`/`link`. **Everything** reads those tokens: the
   `QPalette`, the application QSS, the editor's markdown highlighter (headings +
   links follow the accent) and the preview/export document stylesheet — so the
   chrome, the editor and the rendered document can never drift apart.
   A separate, direct **UI scale** multiplier (75–200%, `Theme::scaleFactor`
   /`scaled`/`scaledFont`) is applied to the application font and the explicit
   widget metrics, on top of Qt's own DPI handling.
   The app has **one row of chrome** — the File/Edit/View/Help menu bar, with no
   toolbar (a toolbar only duplicated the menus) — and the quick light/dark
   toggle lives in the **menu bar's right-hand corner** as an icon-only button
   showing a **sun while light** and a **moon while dark** (tooltip naming the
   state, the accent and the click action). `View → Theme ▸ / Accent ▸ /
   UI Scale ▸` are radio groups, `Ctrl+T` flips light/dark, and the corner
   button's arrow opens the same actions as one flat menu. Mode, accent and scale
   are all **persisted** and re-applied on startup; the shipped defaults are
   **Light + Default accent + 100% UI scale** (`System` is an explicit choice),
   and tests must never write the user's config (see `tests/testmain.h`).
10. **Export** — `File → Export → HTML/PDF`. Export reuses the **same**
    `RenderedDocument` output (HTML via `toHtml()`, PDF via `QTextDocument::print`
    into a `QPdfWriter`), so what you preview is what you export. A file dialog
    picks the destination; images referenced by relative path are copied/embedded
    (HTML references them by path relative to the saved HTML; PDF embeds local
    image resources that Qt can resolve from the document base URL).
11. **Recent files** — a `File` submenu of the last N (default 5) opened/saved
    files, persisted via `Settings`; clicking reopens (dirty guard first).
12. **App icon** — the window/application icon mirrors the Pengy mascot icon:
    a **solid white disc with a fully transparent outside** (the same radius
    ratio, 0.4872 x size) carrying a bold **`#`** — the markdown ATX heading
    element — instead of the penguin. It is generated deterministically by
    `tools/make_icon.py` (one natively rendered PNG per size, 16…512, plus a
    scalable `assets/mdit.svg`), compiled into the binary through
    `resources/mdit.qrc` (`:/icons/mdit-<size>.png`, so no loose runtime files),
    set on the `QApplication` and on the window, and installed into the hicolor
    icon theme with `packaging/mdit.desktop` by `cmake --install`. On Wayland the
    compositor takes the dock/taskbar icon from that `.desktop` entry rather than
    from the window, so `main()` also sets
    `QGuiApplication::setDesktopFileName("mdit")` (the entry's basename) and the
    entry carries `StartupWMClass=mdit` for X11.
13. **Undo / redo** — see §3 (Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y).
14. **Exit / close guard** — closing the window (the title-bar **X**, `File → Exit` /
    `Ctrl+Q`, or the desktop session manager asking to quit) must never silently
    drop unsaved changes: `MainWindow::closeEvent()` runs the *same* guard as
    New/Open (`confirmDiscard()` → Save / Discard / Cancel). **Cancel ignores
    the close and the window stays open**; Save writes first (routing an untitled
    document through Save As, exactly like `Ctrl+S`) and only then closes.
15. **About** — `Help → About mdit` shows a small, themed dialog (not a plain
    message box): the brand mark, the name with the version (from the build's
    `project(VERSION ...)`), the tagline *"The ultimate lightweight Markdown
    tool"*, the `© 2026 Pat Wendorf — MIT License` line, and **mdit is a Catbee
    project — catbee.ca** as a real link that opens in the browser. It is
    window-modal, re-centers over the main window on every show, is created once
    and re-shown, and re-derives its muted/link colors when the theme changes.

## Markdown coverage
- Rendering uses **Qt's built-in CommonMark/GFM importer** via
  `QTextDocument::setMarkdown(md, featureFlags)` with the **full feature flag set
  enabled** (headings h1–h6, emphasis/strong, inline code + fenced code blocks,
  links, images, ordered/unordered + task lists, blockquotes, tables (GFM),
  horizontal rules, escaped characters, line breaks, and reference-style links).
- **Images** (inline `![](path)`, reference-style, local relative/absolute and
  `file://`) render by setting the base URL to the markdown file's directory, so
  relative paths `image.png` and `./screenshots/x.png` resolve; broken image
  links degrade gracefully to the alt text.

> **Highlighter testability (discovered constraint).** This Qt build's
> `QTextDocument` has **no observable per-character format storage**:
> `QSyntaxHighlighter::setFormat()` executes but `QTextCursor::charFormat()`
> and `toHtml()` never reflect it (verified by probe). `MarkdownHighlighter`
> therefore exposes its rule engine as a pure function —
> `formatsForLine(line, inFence) → {ranges(start,count,QTextCharFormat),
> inFenceAfter}` — and `highlightBlock()` applies exactly those ranges with
> `setFormat()`. Tests assert on `formatsForLine()`; the widget still gets real
> highlighting wherever the platform honors `setFormat()`.

> **Renderer API shape (discovered constraint).** `QTextDocument` is
> non-copyable **and** non-movable (`Q_DISABLE_COPY`, no move ctor), and GCC 15
> refuses to return a *configured* local `QTextDocument` by value (NRVO is not
> guaranteed for named locals, and the copy/move needed as a fallback are
> deleted — this is a hard compile error at any `-O` level). `RenderedDocument`
> therefore renders **into a caller-supplied document** rather than returning
> one by value: `static void render(const QString &md, const QUrl &baseUrl,
> bool dark, QTextDocument &out)`. The caller owns the document's lifetime. The
> "full feature flag set" resolves to `QTextDocument::MarkdownDialectGitHub` on
> this Qt build (only the two dialect presets — CommonMark = 0 and the GFM
> bitset — are exposed; there are no per-feature enums to OR together).

## Verification (quadruple method — spec⇄docs⇄code⇄tests)
- **Docs:** this `spec.md`, plus `README.md` (build/run/package/usage) and
  `AGENTS.md`.
- **Code:** matches the module responsibilities above; `src/` clean, documented,
  no QtWebEngine / no external markdown dependency.
- **Tests:** C++ **QtTest** suites driven by **CTest** (`cmake --build && ctest`).
  These are the real objective function for the loop. Prefer headless `-platform
  offscreen`; test the logic classes directly (no real window).
- **Build shim:** `tests/test_build.py` configures+builds the project with CMake
  (`-DBUILD_TESTS=ON`) and runs `ctest`, failing if the build or any test fails.

### Required C++ test cases (subset; agent may add)
- `test_document`: load/save round-trip preserving UTF-8 text; `\n` normalization
  from CRLF input; dirty-flag transitions; title derivation; save-as updates path
  and clears dirty.
- `test_render`: `RenderedDocument` turns markdown into a `QTextDocument` whose
  plain text/HTML contains the expected rendered content — headings, bold/emphasis,
  inline code, fenced code, a GFM table, a link, a blockquote, a task list, and an
  **image** (assert the image resource / base-URL resolution; use a data or
  file-URL image so it resolves without a window).
- `test_editor`: `EditorPane` count (words/chars) matches known input; line-number
  area block count matches the document; a `textChanged` emission occurs on edit;
  find/replace produces the expected result; the syntax highlighter formats the
  expected heading/emphasis ranges (format check on a block's `QSyntaxHighlighter`).
- `test_markdownmodel`: heading extraction returns expected levels/text; the
  debounce policy (pure) returns whether a render is needed / how long to wait.
- `test_undo`: `EditorPane` canUndo()/canRedo() track the undo stack; the
  **Ctrl+Z** key undoes a real edit (Ctrl+Shift+Z / Ctrl+Y redo it, exactly one
  step per press); a programmatic load starts a fresh history; `replaceAll()` is
  one undo step and a no-match replace records none; the Edit menu offers Undo /
  Redo with the standard keys and follows the stack; and the dirty flag returns
  to clean when an undo lands back on the loaded/saved content (dirty again when
  it undoes past a save).
- `test_settings`: theme/preview/splitter/recent-files persist and reload through
  an isolated (temp) `QSettings` file (no clobbering the user's real settings).
- `test_mainwindow` (optional, offscreen): splitter starts ~50/50, preview visible
  by default, toggling preview hides/shows it, dirty guard flags when attempting
  to discard unsaved changes.
- `test_github_ready` (**required**): asserts the repository is GitHub-ready —
- `test_findwindow`: the find/replace popup is a **top-level `QDialog`** owned by
  the main window (not an overlay child), shown NON-modally by Ctrl+F/Ctrl+H and
  **centered on the main window** (re-centered on every show and when it grows
  for the replace row); the title names the mode; Esc closes it and returns the
  focus to the editor; the search/replace behavior still drives the editor.
- `test_icon`: `AppIcons::windowIcon()` carries every generated size (16…512)
  and the resource decodes; the pixels match the design — transparent corners
  and outside a ring at 0.494 x size, an opaque **white disc** (~73% of the
  canvas), a **dark `#`** centred inside it whose middle row/column cross exactly
  two bars (the two verticals / two horizontals) with white counters between
  them; the tiny 16 px mark is still legible; the window carries the same icon.
- `test_closeguard`: a clean window closes with no prompt; a dirty one asks and
  **Cancel keeps it open** (buffer untouched); Discard closes without writing;
  Save writes and then closes; Save on an untitled document routes to Save As,
  and a cancelled Save As leaves the window open; `File → Exit` takes the same
  path.
- `test_about`: the About box shows the app mark, the name/version/tagline and
  the `© … MIT` line (all from `AppInfo`), the project line is a rich-text link
  to `https://catbee.ca` with `openExternalLinks()` on, Close hides it, and
  MainWindow shows one reusable, window-modal, centered dialog.
  `README.md`, `LICENSE` (contains "MIT License" + the copyright line),
  `CHANGELOG.md` (contains a `## [0.1.0]` heading), `spec.md`, and that
  `CMakeLists.txt` declares `project(... )` with a valid name + `enable_testing()`.
  This is the loop's guardrail that a missing doc file fails the suite.

## GitHub-ready repository contract (HARD requirement)
Every deliverable the loop produces MUST be a complete, publishable GitHub
repository. These files are **authored / project-owned** (not regenerated by the
loop) and are asserted on by `test_github_ready`, so a missing or empty file fails
the build/verify:
- **`README.md`** — title, one-line purpose, a screenshot of the UI
  (`assets/screenshots/`), a feature list, a Build section
  (prereqs: CMake ≥ 3.22, Ninja, Qt6 Widgets/Gui/PrintSupport), exact build
  commands (`cmake -S . -B build -DBUILD_TESTS=ON -G Ninja && cmake --build
  build`), a Run section (with the CLI-open form `mdit <file>.md`), a Usage
  section covering the shortcuts and menus, and a brief "Build from source" note.
- **`LICENSE`** — the MIT License text with `Copyright (c) 2026 Pat Wendorf`.
- **`CHANGELOG.md`** — Keep-a-Changelog style, starting with the `## [0.1.0]` /
  `## Unreleased` headings and entry lines; the agent appends an `### Added`
  bullet for each completed feature/bugfix.
- **`spec.md`** — this document (the project's SPEC).
- **`RALPH_SUMMARY.md`** — a short project summary (see ralph's contract).
- **`.gitignore`** — ignores `build/`, `build-*/`, `.cache`, `*.user`, `.qmake.stash`,
  IDE/CMake cruft, and export artifacts (`*.pdf`, `*.html`).
- The project holds a **full test suite** (CTest + the pytest build shim) that is
  all green as the definition of done.
The agent must keep these in sync as it works: every completed feature adds a
CHANGELOG entry and stays reflected in the README's feature list.

## Conventions
- **C++20**, CMake ≥ 3.22, Ninja preferred, Qt6 (Widgets, Gui, PrintSupport; **no
  Multimedia, no WebEngine**).
- `find_package(Qt6 COMPONENTS Widgets Gui PrintSupport REQUIRED)` (PrintSupport
  for PDF export; `QF` for images). No third-party deps.
- `build/` (git-ignored). Tests link `Qt6::Test`.
- Keep tests headless-friendly: pure logic classes take no window; use
  `-platform offscreen` for any QtTest needing `QApplication`/`QWidget`.
- The loop's pytest shim runs the CMake build + `ctest`; keep `test_build.py`
  in sync.

## Suggested-but-deferred (documented, not implemented)
- Multi-tab / multi-document, an outline/heading sidebar, a dedicated GFM
  extension set (footnotes, strikethrough toggle, task-list checkboxes that
  write back), a dark-preview-style contrast control, markdown lint,
  autosave/backup, single-instance enforcement. These are non-goals for the
  first spec unless the agent refines the spec (per the loop's directive) to add
  a genuinely scoped one.

## Non-goals
- No QtWebEngine / no external markdown library — this is a **pure Qt** app.
- No multi-tab, no project files, no live server/preview over HTTP.
- No network/online features, no accounts, no cloud sync.
- No single-instance enforcement, no autosave backups (v1).
- No fancy WYSIWYG editing (this is source + rendered-preview, not a
  contenteditable editor).

## Packaging
- A top-level `build.sh` that configures (`-DBUILD_TESTS=ON` where relevant) and
  builds.
- `cmake --install` (optional, Unix-friendly) installs the binary, the icon
  (`share/icons/hicolor/<size>x<size>/apps/mdit.png`) and
  `packaging/mdit.desktop` (`share/applications`) so a launcher shows mdit with
  its mark.
- **CI** (`.github/workflows/ci.yml`): Linux / macOS / Windows - install Qt6,
  configure with `-DBUILD_TESTS=ON`, build, `ctest --output-on-failure`, and
  smoke-test `mdit --version` + `--help` on Linux.
- **Release** (`.github/workflows/release.yml`): on a `v*` tag, Linux builds the
  `.deb` + AppImage, macOS the `.dmg`, Windows the `.zip`, each attached to the
  GitHub Release (`softprops/action-gh-release`).
- **`build_deb.sh`** - the payload comes from mdit's own `cmake --install` rules
  (binary + hicolor icons + `.desktop`), so the package cannot drift from a
  source install; it adds `DEBIAN/control` (Qt6 runtime deps with the
  pre-/post-time_t64 alternations), `copyright`, a gzipped Debian changelog and a
  `postinst` that refreshes the desktop database + icon cache. The version is
  read from `CMakeLists.txt` and a leading `v` is stripped (dpkg requires a
  digit-first version).
- **`build_appimage.sh`** - linuxdeploy + linuxdeploy-plugin-qt over an AppDir
  assembled by `cmake --install`; it bundles the Qt **Wayland** platform plugin
  and its runtime libs and then *verifies* they are present (an xcb-only bundle
  falls back to XWayland - blurry on HiDPI - or fails to start on Wayland-only
  compositors).
- **`build_macos.sh`** - `mdit.app` (CMake `MACOSX_BUNDLE`) + a generated
  `.icns` (sips + iconutil from the committed PNGs) + `macdeployqt` + `hdiutil`.
- **`build_windows.bat`** - CMake + `windeployqt` + `Compress-Archive`.
- **`check_release.sh`** - pre-flight: version consistency (optionally against a
  tag), icon sizes (256x256 for linuxdeploy), the packaging inputs, the docs
  contract, a clean Release build with the full suite green, and a clean tree.
- `.gitignore` covering `build/`, IDE/CMake cruft, `*.pdf`, `*.html` exports and
  the release artifacts (`*.deb`, `*.AppImage`, `*.dmg`, `*.zip`, `.deb_staging/`,
  `appimage/tools/`, `squashfs-root/`).
