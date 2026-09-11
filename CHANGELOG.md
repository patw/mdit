# Changelog

All notable changes to **mdit** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.1] - 2026-09-11

### Performance
- **Snappier editing on large documents** (`src/MainWindow.{h,cpp}`,
  `src/Document.{h,cpp}`, `src/FindBar.*`) — mdit no longer copies the full
  editor buffer into its file model on every keystroke. A single deferred
  snapshot now supplies document synchronization, word/character counts, and
  the live preview after the typing debounce. The closed Find dialog also no
  longer scans the document after every edit; it refreshes only while visible
  with an active query. These changes reduce main-thread work during normal
  typing while preserving immediate dirty-state feedback and the existing live
  preview behavior.


### Fixed
- **Packaging smoke tests on a headless machine** (`build_deb.sh`,
  `build_appimage.sh`, `build_macos.sh`, `check_release.sh`) — `mdit --version`
  aborts without a display (the binary builds a QApplication), which failed the
  first release run; they now run with `QT_QPA_PLATFORM=offscreen`.
- **The AppImage now bundles the offscreen platform plugin** and is *smoke-tested
  as built* (`APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=offscreen ./mdit-x86_64.AppImage --version`),
  so the published AppImage also runs on headless machines/in containers.
  `EXTRA_PLATFORM_PLUGINS` is computed from what exists, because the wayland
  plugin is one file on Qt 6.10 and split into `-egl`/`-generic` on Ubuntu 24.04
  (naming a missing one makes linuxdeploy-plugin-qt fail).
- **Consistent release asset names** — the Windows zip drops the tag's leading
  `v`, matching the `.deb`, `.dmg` and `.AppImage` names.

### Added
- **A screenshot in the README** (`assets/screenshots/mdit-split-view.png`,
  1840x1040) — mdit editing its own `README.md`, showing the split view
  (syntax-highlighted source left, live-rendered preview right) with the light
  theme and the sun toggle in the menu bar corner. Authoritative source: a real
  GNOME screenshot, only stripped of metadata (`convert -strip`).

### Fixed
- **A test run could overwrite the user's real settings**
  (`tests/testmain.h`, every widget test's `main()`) — `MainWindow`'s default
  constructor uses the real `QSettings` (`~/.config/mdit/mdit.conf`), so the
  theme/close-guard/recent-files suites were writing into it. The theme suite
  left `theme=dark`, `themeAccent=red` and `uiScale=110` behind, which made mdit
  start up "reset" to a theme nobody had chosen (the red highlight) and filled
  Recent Files with `/tmp/test_*` paths. Tests now isolate the process's settings
  location into a temp dir at **load time** (`isolateUserSettings()` +
  an auto-invoked static, so a test cannot forget it and `QTEST_GUILESS_MAIN`
  binaries are covered too), and `test_settings` asserts the default settings
  path is *not* the user's config — that guard is what caught the first, wrong
  attempt at the fix.

### Changed
- **The shipped default theme mode is now Light** (was "System"/auto), with the
  Default accent and a 100% UI scale — a fresh install looks identical on every
  desktop, and following the system colour scheme is an explicit choice.

### Added
- **CI/CD + deployables** (`.github/workflows/{ci,release}.yml`, `build_deb.sh`,
  `build_appimage.sh`, `build_macos.sh`, `build_windows.bat`, `check_release.sh`)
  — adapted from PengyCPP. `ci.yml` builds and runs the full CTest suite on
  **Linux / macOS / Windows** for every push; `release.yml` (on a `v*` tag)
  builds a Linux **`.deb`** + **AppImage**, a macOS **`.dmg`** and a Windows
  **`.zip`** and attaches them to the GitHub Release.
  The `.deb` payload comes from mdit's own `cmake --install` rules (binary, the
  hicolor icons 32…512, and `mdit.desktop` with `Icon=mdit` +
  `StartupWMClass=mdit`), plus `DEBIAN/control` (Qt6 runtime deps with the
  pre-/post-`time_t64` alternations), `copyright`, a gzipped Debian changelog and
  a `postinst` running `update-desktop-database` + `gtk-update-icon-cache`, so
  the entry and its icon show up for every user (dock included) with no logout.
  The AppImage bundles the Qt **Wayland** platform plugin and verifies it made it
  in (xcb-only bundles fall back to XWayland — blurry on HiDPI — or fail outright
  on Wayland-only compositors). `check_release.sh` is the pre-flight: version
  consistency (optionally against the tag), icon sizes, packaging inputs, the
  docs contract, a clean Release build with the suite green.
  Verified locally, not just in CI: the `.deb` was built and unpacked into a temp
  root — `desktop-file-validate` passes, the entry is registered as a
  `text/markdown` handler (`gio mime`), and GTK's
  own icon lookup (what the GNOME dock uses) resolves `mdit` to the packaged
  `usr/share/icons/hicolor/<size>/apps/mdit.png`; the AppImage was built with
  linuxdeploy and run (`mdit 0.1.0`, `--help`), with the wayland + xcb plugins
  and all icon sizes present in its squashfs.
- **Theming system: modes x accents x UI scale** (`src/Theme.{h,cpp}`,
  `src/Settings.{h,cpp}`, `src/MarkdownHighlighter.*`, `src/RenderedDocument.*`,
  `src/PreviewPane.*`, `src/EditorPane.*`, `src/MainWindow.*`,
  `tests/test_theme.cpp`, `tests/test_settings.cpp`) — the palette is no longer
  hardcoded light/dark. Adapted from PengyCPP's `themehelper.h`:
  `Theme::make(dark, accent)` generates a complete **named-token** palette
  (36 tokens: surfaces, text, interaction colours and the preview/export
  `doc_*` set) from **3 modes** (System / Light / Dark) x **8 accents**
  (Default + Blue/Teal/Green/Orange/Red/Pink/Purple) = 16 palettes. The
  `Default` accent reproduces the previous neutral colours byte-for-byte, so the
  app looks identical unless an accent is chosen; every other accent blends the
  surfaces toward the colour and sets `primary`/`selection`/`link`.
  Everything consumes the tokens: the `QPalette` (`buildPalette`), the
  application QSS (`appStyleSheet`, with a test asserting no unsubstituted
  `@{token}` placeholder survives), the **editor's markdown highlighter**
  (`applyTheme()` — headings h1/h2 and links take the accent, h3–h6 blend
  toward the text colour) and the **preview + HTML/PDF export stylesheet**
  (`RenderedDocument::stylesheet(theme)`), so nothing can drift.
  A separate **UI scale** multiplier (PengyCPP's model: 75–200%,
  `scaleFactor`/`scaled`/`scaledFont` — *on top of* Qt's DPI handling) scales
  the application font, the editor's monospace font and the widget metrics in
  the stylesheet. Mode, accent and scale all persist (`themeAccent`, `uiScale`)
  and are re-applied on startup.
  UI: `View > Theme ▸ / Accent ▸ / UI Scale ▸` (radio groups) plus the menu-bar
  corner button, whose arrow opens the same actions as one flat menu
  (`QAction`s can live in several menus; a `QMenu` cannot) — the sun/moon,
  `Ctrl+T` toggle behaviour and its persistence are unchanged.
  Suite: `test_theme` rewritten (20 cases: token completeness/legibility for all
  16 palettes, Default-accent parity, accent tinting, scale maths, stylesheet
  substitution, palette mapping, the menus/corner popup, preview + highlighter
  following the theme) and `test_settings` gained accent/scale round-trip +
  clamping cases.
- **Unsaved-changes guard when closing** (`src/MainWindow.{h,cpp}`,
  `tests/test_closeguard.cpp`) — the title-bar **X**, `File → Exit` / `Ctrl+Q`
  and a session-manager quit now run the same Save / Discard / Cancel guard as
  New/Open (`closeEvent()` → `confirmDiscard()`); **Cancel leaves the window
  open** and Save writes before closing. Chasing it surfaced a related bug: the
  guard's *Save* answer called `saveDocument()` directly, which is a no-op for an
  untitled document — it now routes through Save As (exactly like `Ctrl+S`) and
  only closes once the file actually landed.
- **A real About box** (`src/AboutDialog.{h,cpp}`, `src/AppInfo.{h,cpp}`,
  `tests/test_about.cpp`) — `Help → About mdit` shows a small themed dialog with
  the brand mark, **mdit** + its version (now a single source of truth from
  CMake's `project(VERSION ...)` via `MDIT_VERSION`, no second literal), the
  tagline *"The ultimate lightweight Markdown tool"*, *"© 2026 Pat Wendorf —
  MIT License"* and *"mdit is a Catbee project — catbee.ca"* as a working link
  (`openExternalLinks()`). It is window-modal, re-centers over the main window,
  built once and re-shown, and re-derives its muted/link colors on theme change
  (Qt's default link navy and an unset `palette(placeholder-text)` were both
  unreadable in dark mode).
- **Find/Replace is now its own popup window** (`src/FindBar.{h,cpp}`,
  `tests/test_findwindow.cpp`) — the overlay bar that sat under the menu bar is
  gone. `FindBar` is a non-modal `QDialog` centered over the main window on
  every show and re-centered when it grows for the replace row; the title bar
  names the mode (*Find* / *Find and Replace*); Esc (or the title-bar X) closes
  it and returns the focus to the editor. Two real bugs surfaced while making it
  a window: the platform handed it a taller default (a blank second row in
  find-only mode — `adjustSize()` will not SHRINK a visible top-level window, so
  it now resizes to the layout hint explicitly), and re-showing did not
  re-center.
- **Right-aligned sun/moon theme toggle** (`src/MainWindow.{h,cpp}`,
  `tests/test_theme.cpp`) — the light/dark toggle moved to the far right of the
  toolbar (an expanding spacer pushes it there) as an icon-only emoji button:
  `☀️` while light, `🌙` while dark, with a tooltip naming the state and the
  click action. It follows the **persisted** theme on startup and writes the
  explicit light/dark choice back through `Settings`, so the last choice is
  remembered; `View → Theme` (`Ctrl+T`) stays as the menu twin (the button
  deliberately has no shortcut, so the pair is never an ambiguous shortcut).
- **Application icon** (`tools/make_icon.py`, `assets/icons/*.png`,
  `assets/mdit.svg`, `resources/mdit.qrc`, `src/AppIcons.{h,cpp}`,
  `tests/test_icon.cpp`) — a Pengy-style mark: solid **white disc, transparent
  outside**, with a bold **`#`** (the markdown heading element) in the mascot's
  near-black instead of the penguin. Eight sizes (16…512) are rendered natively
  (the 16 px one is pixel-snapped and square-ended so it stays legible; ≥ 48 px
  use the soft rounded ends), embedded via `qrc` and set on the application and
  the window. `cmake --install` adds them to the hicolor icon theme and installs
  `packaging/mdit.desktop`. The suite is now **21 CTest binaries** (was 19).
- **Undo / redo while editing** (`src/MainWindow.{h,cpp}`,
  `src/EditorPane.{h,cpp}`, `tests/test_undo.cpp`) — `Ctrl+Z` / `Ctrl+Shift+Z`
  (plus `Ctrl+Y`) now have **Edit → Undo / Redo** menu entries whose enabled
  state follows the editor's undo stack (scoped to the editor, so the find
  bar's own fields keep their native undo). Three real gaps closed at the same
  time: **Replace All is now a single undo step** (it went through
  `setPlainText()`, which wiped the whole undo stack, so a Replace All could
  not be undone at all); the dirty flag became **undo-aware** (`markClean()`
  after every load/new/save establishes the checkpoint, so undoing back to the
  saved content drops the `*` from the title and flips the status-bar
  indicator to "Unmodified", while undoing *past* a save stays dirty); and a
  freshly opened document starts a clean history (no undoable trip back into
  the previous document). Covered by the new 12-case CTest binary
  `test_undo` (19 binaries total, all green).
- **Final polish** — `RALPH_SUMMARY.md` (the standing "the loop is done"
  summary: what was built, feature checklist, test suite, build/run, and the
  repository contract), verified the full 18-binary CTest/QtTest suite green
  from a clean configure+build via `tests/test_build.py` (`pytest -q`), and
  committed the GitHub-ready repository.


### Changed
- **One row of chrome: the toolbar is gone** (`src/MainWindow.{h,cpp}`,
  `tests/test_smoke.cpp`, `tests/test_theme.cpp`) — the QToolBar duplicated the
  menus (New/Open/Save/Preview) in a second row under the menu bar, so it was
  removed; every one of those actions is still reachable from File/View. The
  light/dark toggle moved to the **menu bar's right-hand corner**
  (`QMenuBar::setCornerWidget`, an auto-raised `QToolButton` sharing the theme
  action), which keeps it right-aligned on the single chrome row.

### Fixed
- **The dock/taskbar icon** (`src/main.cpp`, `packaging/mdit.desktop`) — the app
  was never installed, so there was no `.desktop` entry for GNOME/Ubuntu Dock to
  take an icon from (and on Wayland a compositor ignores window icons entirely).
  `main()` now sets `QGuiApplication::setDesktopFileName("mdit")` so the window's
  app id matches `mdit.desktop`, the entry gained `StartupWMClass=mdit`, and
  `cmake --install --prefix ~/.local` puts `mdit` on PATH plus the hicolor icons
  and the desktop entry in place (verified: GTK's icon lookup resolves `mdit` to
  `~/.local/share/icons/hicolor/48x48/apps/mdit.png`, and `mdit.desktop` is
  registered as a `text/markdown` handler). Registering a handler is all the
  install does — it never sets a *default*; the desktop decides. On GNOME/GLib
  with no prior user choice that lands on mdit (the only exact-type handler,
  beating the `text/plain` fallback Text Editor wins by); KDE leaves it to File
  Associations.

## [0.1.0] - 2026-09-10

## [0.1.0] - 2026-09-10

### Added
- **GitHub-ready repository guard** (`tests/test_github_ready.cpp`,
  `README.md`, `CHANGELOG.md`): a new CTest suite that fails the build if the
  repository contract drifts — `README.md` exists with the required sections
  (title, Features, Build + exact build command, Run incl. the CLI-open form,
  Usage/shortcuts, MIT + copyright), `LICENSE` contains the **MIT License**
  text and `Copyright (c) 2026 Pat Wendorf`, `CHANGELOG.md` carries the
  `## [0.1.0]` heading, `spec.md` is present and non-empty, `.gitignore`
  covers the build/export artifacts, and `CMakeLists.txt` declares
  `project(mdit ...)` + `enable_testing()`. Every feature above now lives
  under the `## [0.1.0]` release heading (moved from Unreleased).
- **CLI open + entry-point status path** (`src/AppCli.{h,cpp}`, `src/main.cpp`,
  `src/MainWindow.{h,cpp}`): the single-file command-line open is now a **pure,
  GUI-free decision** — `AppCli::fileToOpen(positionalArgs)` returns the first
  argument that is an existing `.md` / `.markdown` file (as an absolute path),
  and an empty result otherwise (a missing path, a directory, a non-markdown
  file, or no arguments), so `mdit <file>.md` opens the file when it exists and
  a missing/bad/absent argument leaves the app in the empty **untitled** state
  (mdit loads only markdown files, mirroring the drag-and-drop filter). `main()`
  calls it and hands a non-empty result to `MainWindow::openFile()`. `openFile()`
  now shows the **opened full path** in the status bar (`Opened <path>`, not just
  the file name) so the entry point — CLI or drag-and-drop — makes the loaded
  file unambiguous; the title carries the file name. Drag-and-drop of a
  `.md` / `.markdown` (wired earlier) opens the same way and shares the status.
  Exercised headlessly (offscreen `QApplication`, isolated temp `QSettings`) by
  the new `test_entrypoint` suite: the pure CLI decision (existing `.md`/`.markdown`,
  case-insensitive suffix, first-valid-wins, missing / directory / non-markdown /
  no-args → untitled), the entry-point open showing the path in the status +
  title, a missing/bad CLI argument leaving the window untitled, and a drop
  opening with the same status path.
- **Recent files** (`File → Recent Files` submenu, `src/MainWindow.{h,cpp}`,
  `src/Settings.{h,cpp}`): the File menu now carries a **Recent Files** submenu
  listing the last N (default 5) opened/saved markdown paths, most-recent-first,
  persisted through `Settings` (`recentFiles`/`addRecentFile`, capped +
  de-duplicated). Every successful **open** and **save / save-as** records the
  file; `MainWindow::rebuildRecentMenu()` rebuilds the submenu so each entry is
  one enabled action (the path stored in the action's `data`). Clicking an
  entry reopens its file through `openFile()` — **dirty guard first** — so a
  pending change on the current document is not silently discarded. With no
  entries the submenu item is disabled and shows a non-interactive "No recent
  files" placeholder. Exercised headlessly (offscreen `QApplication`, isolated
  temp `QSettings`) by six new slots in `test_mainwindow`: the submenu mirrors
  the persisted list (order + enabled), the empty/disabled state, open adding +
  the cap at 5 (and reaching disk across a "restart"), save adding to the list,
  an entry re-opening its file, and the dirty-guard refusing a guarded re-open
  (scripted `Cancel`).
- **Export PDF** (`File → Export → PDF...`, `src/MainWindow.{h,cpp}`,
  `src/RenderedDocument.{h,cpp}`): the current editor content (unsaved edits
  included) is rendered through the **same shared `RenderedDocument` path the
  preview and HTML export use** ("what you preview is what you export") and
  printed into a `QPdfWriter` via the new pure
  `RenderedDocument::renderPdf(md, baseUrl, dark, outPath, pageSizeId, dpi)` —
  which sets the page **size** (A4) + **orientation** (portrait) + **margins**
  and **resolution (DPI)** on the writer, then `QTextDocument::print()`.
  `renderPdf()` returns true only when a real PDF was written (it verifies the
  `%PDF-` magic header at byte 0), so an un-writable destination reports
  failure. `MainWindow::exportPdfDocument()` asks for the destination through
  the scriptable `askExportPdfPath()` seam (same pattern as
  `askExportHtmlPath()`), pre-filling the document's stem (`notes.md` →
  `notes.pdf`, untitled → `untitled.pdf`), and reports `Exported <name>` /
  `Could not export <name>` in the status bar. The render resolution is
  configurable via `setPdfResolutionDpi()` (default 96, clamped to ≥ 1).
  Export never changes the document's dirty state; a canceled dialog writes
  nothing. Exercised headlessly (offscreen `QApplication`, isolated temp
  `QSettings`) by the new `test_export_pdf` suite: a real PDF (magic header +
  `/Page` object + `%EOF` trailer, non-degenerate), failure on an unwritable
  path, the window-level export (valid PDF + status + not-dirty), the
  suggested stem name (saved + untitled), current-editor-content +
  dirty-preserving export, cancel-writes-nothing, bad-path failure, and the
  configurable/clamped DPI.
- **Export HTML** (`File → Export → HTML...`, `src/MainWindow.{h,cpp}`,
  `src/RenderedDocument.{h,cpp}`): the current editor content (unsaved edits
  included) is rendered through the **same shared `RenderedDocument` path the
  preview uses** ("what you preview is what you export"), wrapped by the new
  `RenderedDocument::standaloneHtml()` into a real **standalone page**
  (`<!DOCTYPE html>`, UTF-8 charset, the document title, the theme stylesheet
  in `<style>`, the lifted `<body>` content), and written to the `.html` file
  chosen in the Save dialog — pre-filled with the document's stem
  (`notes.md` → `notes.html`, untitled → `untitled.html`). **Relative image
  references are copied next to the saved HTML** (same relative path,
  subdirectories recreated) so the exported file keeps working wherever it is
  saved; scheme/absolute refs (`file://`, `http://`, `data:`) are left as-is.
  Export never changes the document's dirty state; a canceled dialog writes
  nothing and an un-writable path reports `Could not export <name>`. The
  dialog is scriptable through the virtual `askExportHtmlPath()` seam (same
  pattern as `askSaveAsPath()`). Exercised headlessly (offscreen `QApplication`,
  isolated temp `QSettings`) by the new `test_export_html` suite: standalone
  page assembly (doctype/title escaping/theme/fallback without a `<body>` tag),
  non-empty exported HTML with the rendered content + stylesheet, the suggested
  stem name, current-editor-content + dirty-preserving export, cancel,
  bad-path failure, dark-stylesheet export, and relative-image copying (top
  level + subdirectory, byte-identical).
- **Status-bar word/char counts + modified indicator** (`src/MainWindow.{h,cpp}`):
  the status bar now carries two permanent readouts (right-aligned, independent
  of the transient message area). A **word/char count** label (e.g. `5 words, 23
  characters`, with correct singular/plural) is refreshed **on the live-preview
  debounce** (`onPreviewTimerTimeout`) and immediately on **open** and **new**
  so it never shows a stale `0/0` after loading a file; the O(n) count is
  deliberately kept off the per-keystroke path. The debounce timer is no longer
  gated on preview visibility — it now drives the (cheap) count regardless,
  while the (expensive) render stays guarded inside `updateLivePreview()` (a
  no-op while the pane is hidden), so the counts stay current even with the
  preview off and a hidden preview still costs no render time. A **modified/
  unmodified indicator** (O(1) — just the Document dirty flag) is refreshed via
  `updateTitle()` on every dirty-state change (typing, open, new, save, save-as)
  so it flips live. `refreshStatusCounts()`/`updateModifiedIndicator()` and the
  two label getters are exposed for headless tests. Exercised by six new slots in
  `test_mainwindow` (initial 0/0 + Unmodified; debounce-not-per-keystroke; counts
  refresh while the preview is hidden; immediate counts on open; singular/plural
  formatting; and the indicator tracking clean→edit→save).
- **Preview toggle + splitter-ratio persistence** (`src/MainWindow.{h,cpp}`):
  the `View → Preview` menu/toolbar action (`Ctrl+Shift+P`, checkable, **ON by
  default**) now fully hides/shows the preview pane — while hidden the editor
  takes the **full width** — and **persists its state** through `Settings`
  (`previewVisible`, default `true`) so a hidden preview survives a restart
  (with the action's checked state following). The **splitter ratio is
  persisted on change**: dragging the handle (`QSplitter::splitterMoved`) or
  calling the new `MainWindow::setSplitterRatio()` (clamped to 0.05–0.95 so the
  non-collapsible panes never fully collapse) writes `splitterRatio` to the
  backing `QSettings`; on **startup** the initial split honors the persisted
  ratio (default `0.5` = the 50/50 split) and re-showing a hidden preview
  **restores the persisted ratio** instead of resetting to 50/50. Exercised
  headlessly (offscreen `QApplication`, isolated temp `QSettings`) by the new
  `test_mainwindow` suite covering the spec defaults, the View-menu toggle
  action (key + checkable + trigger), visibility persistence across "restarts",
  full-width-when-hidden, ratio persistence on change (to disk), restore-on-
  re-show, and startup restoration of a persisted ratio + hidden preview.
- **Light / Dark theme + toggle** (`src/Theme.{h,cpp}`, `src/MainWindow.{h,cpp}`):
  the theme is now a full application feature. `Theme::apply(app, dark)` sets the
  **Fusion** style, a light/dark `QPalette`, and an application-level **QSS
  stylesheet** so every widget (menus, toolbar, status bar, editor, preview
  chrome) matches. The three persisted **modes** (`auto`/`light`/`dark`) resolve
  to a concrete light/dark through the pure `Theme::resolveDark()` — `auto`
  follows the platform's `Qt::ColorScheme` (`Theme::isSystemDark()`).
  `MainWindow` owns a typed `Settings` (a test-supplied backing `QSettings` keeps
  offscreen runs off a user's real settings): it **resolves the persisted theme
  on startup** (so the `View → Theme` action state is correct from first paint)
  and the **View → Theme** menu/toolbar action (`Ctrl+T`) toggles light/dark,
  **persists** the explicit choice through `Settings`, re-applies the Fusion
  palette + QSS, re-themes the editor `MarkdownHighlighter`, and **re-renders the
  preview** so the `RenderedDocument` stylesheet matches the new theme.
  `main.cpp` no longer hardcodes the theme (the window owns it). Exercised
  headlessly (offscreen `QApplication`, isolated temp `QSettings`) by the new
  `test_theme` suite covering the `resolveDark()` mapping, the light/dark
  `stylesheet()`, `apply()`'s Fusion+palette+QSS, startup theme resolution, and
  the toggle's persistence + preview re-application.
- **`Settings` wrapper** (`src/Settings.{h,cpp}`): a typed `QSettings`
  layer (org/app `mdit`) the rest of the app persists through, with typed
  accessors and the spec defaults — **theme** (`auto`/`light`/`dark`, default
  `auto`), **`previewVisible`** (default `true`), **`splitterRatio`** (default
  `0.5` = the 50/50 split, clamped to `[0,1]`), and a **`recentFiles`** list
  (default empty, **capped at 5**) that is most-recent-first, de-duplicated,
  and grown via `addRecentFile()`. The constructor takes an optional backing
  `QSettings*` so tests can point it at an isolated temp dir and never touch a
  user's real settings. Exercised headlessly (no `QApplication`) by the new
  `test_settings` suite covering the identity/defaults, typed round-trips
  through a fresh `QSettings` (real disk persistence), splitter clamping, the
  recent-files cap / order / dedupe, a settable cap, and the on-disk `.ini`.
- **Replace (Ctrl+H)** in the `FindBar` (`src/FindBar.{h,cpp}`): the Ctrl+F
  find bar gains a foldable **replace row** (replace field, **Replace**,
  **Replace All**) that `Ctrl+H` / `Edit → Replace` unfolds — the same bar as
  find, so search state (query, case-sensitivity, count label) is shared.
  **Replace** rewrites the *current match* (finding the next one first when
  none is selected) with the replace text, then advances to the following
  occurrence (wrapping like Next); **Replace All** delegates to the single
  `EditorPane::replaceAll()` implementation with the bar's case-sensitivity.
  Enter in the replace field = Replace; the match-count label refreshes after
  either operation. Both paths emit through the editor's `textChanged`, so
  dirty/title/status stay correct with no extra wiring. Exercised by eight new
  slots in `test_editor` covering the row's show/fold, replace-all result +
  count (incl. case sensitivity and the label refresh), replace-current
  advance/stop semantics, no-match and empty-query no-ops.
- **Find bar (Ctrl+F)** in `MainWindow` + the new `FindBar` widget
  (`src/FindBar.{h,cpp}`): a small overlay bar over the editor (search field,
  Next/Previous buttons, a "Match case" toggle, a match-count label, and a
  Close button; Enter = next, Shift+Enter = previous, Esc = close) that drives
  the `EditorPane`'s existing search helpers — it never re-implements search.
  Typing a query highlights the match at/after the cursor (the editor's
  selection) and reports `"<i> of <n>"`; Next/Previous step through matches
  **wrapping** at the ends (past the last → first, before the first → last); a
  case-insensitive match count is shown (and, with "Match case" on, a
  case-sensitive one); a query with no matches clears the selection and shows
  "no matches". `Edit → Find` / `Ctrl+F` shows the bar; while it is open the
  editor's `textChanged` keeps the count current (count-only, no selection
  move). Exercised headlessly (offscreen `QApplication`) by six new slots in
  `test_editor` covering match count, typing→highlight + "i of n", next/prev
  wrap, the case-sensitivity toggle, no-match clearing, and the "i of n" index.
- **Save / Save As** in `MainWindow` (`src/MainWindow.{h,cpp}`): `Ctrl+S`
  writes the editor content to the document's current file path through the
  `Document` model (UTF-8, remembered line ending), clears dirty, drops the
  `*` from the title, and reports `Saved <name>` in the status bar; an
  untitled document's Save routes straight to Save As. `Ctrl+Shift+S` opens
  the (markdown-filtered) Save dialog, pre-filled with the current file name
  or `untitled.md`; a chosen path is written via `Document::save(path)`
  (updating `currentFilePath` and clearing dirty) and the preview re-renders
  so its base URL follows the new directory. Cancel leaves every bit of state
  untouched; an un-writable path shows `Could not save <name>` without
  touching the model. The dialog answer is scriptable through the virtual
  `askSaveAsPath()` seam (same pattern as `askDiscardChoice()`). The
  Document-level save-as behavior (path update + dirty clearing) is covered
  by `test_document`; the window-level wiring is exercised by the new
  `test_save` suite (spec shortcuts, save-through-document incl. CRLF
  round-trip, untitled→Save As routing, cancel, bad-path failure, suggested
  name).
- **Live preview wiring** in `MainWindow` (`src/MainWindow.{h,cpp}`): typing in
  the editor now schedules a **debounced** re-render of the preview (a singleShot
  `QTimer` restarted on each change, its elapsed time run through the pure
  `PreviewDebouncer` policy, default 150 ms) — and it renders **only while the
  preview pane is visible**, so a hidden preview never burns time. The base URL
  is derived from the document's current file path so **relative images resolve**
  against the file's directory; `openFile()` renders immediately (no debounce
  wait), and re-showing a hidden preview re-renders the current content. The
  editor and preview now **sync their vertical scroll ratio** (best-effort, with
  a feedback guard so driving one pane doesn't echo back into it).
  `EditorPane` gained `scrollRatio()`/`setScrollRatio()` to mirror `PreviewPane`.
  Exercised headlessly (offscreen `QApplication`) by the new `test_livepreview`
  suite covering the debounced render, the immediate render on open, base-URL
  image resolution, the no-while-hidden + re-render-on-reshow behavior, the
  editor→preview scroll sync, and the feedback guard.
- `MarkdownModel` (`src/MarkdownModel.{h,cpp}`): a pure, GUI-free markdown
  text-logic module — ATX **heading extraction** (`#`/`##`/... → level + text,
  closing `#` sequences stripped, `#noSpace`/`#######`/empty headings
  excluded, lines inside fenced code blocks ignored, ≤3 leading spaces
  honored) for an outline and content tests; the canonical **word/char
  counts** (`EditorPane` now delegates to these so the status-bar counts and
  the model can never diverge); and the **`PreviewDebouncer` policy** — a pure
  "render now, or wait N ms" decision from the time elapsed since the last
  edit (default interval 150 ms, the middle of the spec's ~120–200 ms
  window). Exercised headlessly (no `QApplication` needed) by the new
  `test_markdownmodel` suite covering heading levels/text, non-headings,
  fenced-code exclusion, indentation rules, counts, and the debounce
  decision/clamping.
- `EditorPane` (`src/EditorPane.{h,cpp}`): the left-hand source editor — a
  `QPlainTextEdit` subclass that adds a **line-number margin** (a dedicated
  `LineNumberArea` widget that repaints on vertical scroll, font/layout change,
  and resize; sized from the widest line number and exposing the exact
  `"1".."N"` block count via `lineNumbers()`), hosts the `MarkdownHighlighter`
  (installed on its document, re-themed with `setHighlighterDark()`), and
  reports live **word/char counts** (`wordCount()`/`charCount()`). It also
  provides the **find/replace helpers** the Find/Replace UI will build on:
  `find()`/`findPrevious()` (select + scroll to the next match, deselect on a
  miss, case-sensitive/regex via `Qt::MatchFlags`), `matchCount()`, and
  `replaceAll()`. `editorTextChanged()` re-broadcasts the base `textChanged` as
  a stable passthrough for the subtask-4 debounced preview. Exercised
  headlessly (offscreen `QApplication`) by the new `test_editor` suite covering
  counts, the line-number block count + digit-width sizing + the margin widget,
  a `textChanged` emission, the highlighter's heading/emphasis formatting ranges
  and `setHighlighterDark` re-theming, and the find/replace helpers.
- `MarkdownHighlighter` (`src/MarkdownHighlighter.{h,cpp}`): a pure-
  `QTextCharFormat` `QSyntaxHighlighter` for the editor pane covering headings
  h1–h6 (bold, distinct per-level theme color, muted hashes), strong/emphasis
  (`**…**` bold, `*…*`/`_…_` italic — with look-around guards so emphasis never
  leaks into strong), inline code, fenced code blocks (``` … ```, tracked with
  per-block state so `#`/`**` inside code are not highlighted), links and
  images (underline), blockquotes, horizontal rules, and list markers. The
  whole color map is theme-aware (light/dark) and `setDark()` re-themes and
  repaints in place for the subtask-7 theme toggle. Exercised headlessly (no
  `QApplication` needed) by the new `test_highlighter` suite, which reads the
  char formats back via `QTextCursor::charFormat()`.
- `PreviewPane` widget (`src/PreviewPane.{h,cpp}`): the right-hand live preview
  — a read-only `QTextBrowser` hosting the rendered `QTextDocument`.
  `setRendered(md, baseUrl, dark)` renders through the shared `RenderedDocument`
  into the browser's own document (the same path export reuses),
  `displayedQTextDocument()` hands back that live document for HTML/PDF export,
  `setBaseUrl()` points the document's base URL at the markdown file's directory
  so **relative images resolve**, and `scrollRatio()`/`setScrollRatio()` expose
  the vertical scroll position as a 0..1 fraction (with a `scrollRatioChanged`
  signal for the editor↔preview sync in a later subtask). Exercised headlessly
  (offscreen `QApplication`) by the new `test_previewpane` suite covering
  rendering into the live document, base-URL image resolution, and
  scroll-ratio round-trips (top/middle/bottom + clamping).
- `RenderedDocument` (`src/RenderedDocument.{h,cpp}`): a pure, GUI-free
  markdown→`QTextDocument` renderer shared by the live preview and HTML/PDF
  export so "what you preview is what you export". `render(md, baseUrl, dark)`
  imports via `QTextDocument::setMarkdown()` with the fullest feature flag set
  this Qt build exposes (the GFM dialect — tables, autolinks, strikethrough,
  task lists, and the CommonMark core), sets the base URL so **relative images
  resolve** against the markdown file's directory, and applies a per-document
  light/dark stylesheet. Exercised headlessly (offscreen `QGuiApplication`) by
  the new `test_render` suite covering headings, bold/emphasis, inline + fenced
  code, a GFM table, a link, a blockquote, a task list, base-URL image
  resolution, and the theme stylesheet.
- Open / New wired through the `Document` model in `MainWindow`
  (`src/MainWindow.{h,cpp}`): `openFile()` loads via `Document::load()` (path
  recorded, dirty cleared, CRLF→LF normalized, title updated) and `newDocument()`
  resets to an untitled document. Both are protected by a new dirty-guard helper
  `confirmDiscard()` (returns true when clean, or when the user chooses
  Save/Discard via a scriptable `askDiscardChoice()`; false on Cancel). Typing in
  the editor marks the document dirty through the model and refreshes the title
  (`*` while dirty). Command-line and menu Open keep their standard `Ctrl+N` /
  `Ctrl+O` shortcuts.
- **Drag-and-drop** open: dropping a local `.md` / `.markdown` file onto the
  window opens it (dirty-guarded); other files are ignored. Driven by
  `MainWindow::handleDroppedPaths()`, exercised headlessly by the new
  `test_opennew` suite (document round-trip, dirty transitions, guard
  save/discard/cancel, drop filtering).
- `Document` model (`src/Document.{h,cpp}`): a pure, GUI-free document model
  owning the text, `currentFilePath`, and the dirty flag. `load()` streams a
  UTF-8 read and normalizes `\r\n`/CR to `\n` (remembering the source line
  ending); `save()` writes UTF-8 (no BOM), restoring the remembered line ending
  (default `\n`) and clearing dirty on success; `title()` derives the base file
  name or the untitled name, and `nextUntitled()` yields `untitled`,
  `untitled 1`, .... Exercised headlessly by the new `test_document` suite.
- `tests/test_build.py` pytest shim verified end-to-end: `pytest -q` configures
  (`-DBUILD_TESTS=ON`), builds, and runs `ctest --output-on-failure`, failing on
  any configure, build, or test error. This is the entry point the ralph
  harness invokes.

- Project scaffold: CMake 3.22 build (C++20, Ninja) targeting the `mdit`
  Qt6 executable with `WIN32`/`MACOSX_BUNDLE` flags.
- `find_package(Qt6 COMPONENTS Widgets Gui PrintSupport REQUIRED)` — pure Qt6,
  no QtWebEngine / no external markdown library.
- Repository layout with `src/` (sources + include dir) and a `tests/`
  subdirectory holding C++ QtTest binaries that link `Qt6::Test`.
- `enable_testing()` + a `BUILD_TESTS` option (default ON) driving the CTest
  suite; tests run under the Qt **offscreen** platform plugin (headless-safe).
- Initial QtTest binary (`test_scaffold`) verifying the configure/build/ctest
  pipeline, plus `tests/test_build.py` (pytest shim) that configures, builds,
  and runs `ctest --output-on-failure`.
- GitHub-ready docs: `README.md`, `LICENSE` (MIT, Pat Wendorf 2026),
  `CHANGELOG.md`, `spec.md`, and a complete `.gitignore`.
- `src/` window shell: `main.cpp` (creates the `QApplication`, applies the
  theme, shows `MainWindow`, honors an optional `argv[1]` `.md` open), and a
  `MainWindow` (a `QSplitter` with `EditorPane` left + `PreviewPane` right at
  50/50, a menu bar, a toolbar, a status bar, an optional file open, and a
  preview show/hide toggle). App sources are compiled into a `mdit_core`
  static library that the CTest binaries link directly.
- `test_smoke` QtTest verifying the shell builds and behaves headlessly
  (splitter layout / 50-50, menu·toolbar·status bar, preview toggle,
  `openFile()` load + `\r\n`→`\n` normalization, untitled reset).
