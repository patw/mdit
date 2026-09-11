# implementation.md — mdit (ordered task list, one per iteration)

> Work EXACTLY ONE unchecked `- [ ]` per run. Each item is a small, unambiguous
> unit. The project is **GitHub-ready**: README.md, LICENSE (MIT, Pat Wendorf
> 2026), CHANGELOG.md (Keep a Changelog), spec.md, .gitignore and a full CTest
> suite — asserted by `test_github_ready`. Keep the doc files in sync as you go.

## 0. Repository foundation
- [x] Scaffold + CMake: `CMakeLists.txt` with `project(mdit VERSION 0.1.0 LANGUAGES CXX)`, C++20, `find_package(Qt6 COMPONENTS Widgets Gui PrintSupport REQUIRED)`, `enable_testing()`, a `BUILD_TESTS` option, `qt_add_executable(mdit ...)` targeting WIN32/MACOSX_BUNDLE, and a `tests/` subdir with its own `CMakeLists.txt` linking `Qt6::Test`. `aux_source`/`target_include_directories` sorted into `src/`. Set up `.gitignore` (build/, build-*/, .cache, *.user, *.pdf, *.html, *.o, CMake cruft).
- [x] `src/` skeleton: `main.cpp` (creates `QApplication`, applies persisted Theme, shows `MainWindow`, handles an optional `argv[1]` open), and an empty `MainWindow` (a `QSplitter` holding an `EditorPane` left + a `PreviewPane` right, initial 50/50, a menu bar, a toolbar, a status bar). Compiles headless with `-platform offscreen`. `test_smoke` QtTest binary added to CTest.
- [x] `test_build.py` pytest shim: configures the build (`-DBUILD_TESTS=ON`), builds, and runs `ctest --output-on-failure`; fails on any build/test failure. (This is what the ralph harness invokes.)

## 1. Document model (pure, no widget)
- [x] `Document` core: members `currentFilePath`, `dirty`, `text()/setText()`, `load(path)` (streaming UTF-8 read, normalize `\r\n`→`\n`, set path, dirty=false), `save(path)` (UTF-8 write, keep detected line ending, but by default `\n`; on success clear dirty), `title()` (filename or next-untitled), `nextUntitled()`. No Qt GUI. `test_document` covers round-trip, CRLF normalization, dirty transitions, save-as path+clears-dirty.
- [x] Wire Open/New into MainWindow through `Document` (menu actions, Cmd/Std shortcuts, DnD). Dirty-guard helper `confirmDiscard()` returns true if clean or user agrees.

## 2. Rendering (pure `RenderedDocument`)
- [x] `RenderedDocument` (pure): `static QTextDocument render(const QString& md, const QUrl& baseUrl, bool dark)`. Uses `QTextDocument::setMarkdown(md, flags)` with the FULL flag set (Tables, CommonMark, and Qt's available feature enums), sets `setBaseUrl(baseUrl)` so relative images resolve, and applies a theme stylesheet (light/dark). `test_render` asserts rendered plain text / HTML for headings, bold, emphasis, inline code, fenced code, a GFM table, a link, a blockquote, a task list, and an **image** (base URL resolution using a `file://` or data image).
- [x] `PreviewPane` widget: a `QTextBrowser`/view hosting the rendered `QTextDocument`; sets the base URL to the markdown file's directory; exposes `setRendered(...)`, `displayedQTextDocument()` for export, and a `scrollRatio()`/`setScrollRatio()` API for sync. Headless-safe (offscreen).

## 3. Editor
- [x] `MarkdownHighlighter`: a `QSyntaxHighlighter` subclass formatting headings h1–h6, emphasis/strong, inline code, code fences, links, blockquotes, horizontal rules, and list markers with a theme-aware color map. Pure formatting via `QTextCharFormat`.
- [x] `EditorPane`: a `QPlainTextEdit` subclass adding a line-number margin (widget that repaints on vertical-scroll/font/resize) and hosting the `MarkdownHighlighter`. Exposes `wordCount()/charCount()`, find/replace helpers, and `textChanged` passthrough. `test_editor` covers counts, line-number block count, a `textChanged` emission, and a highlighter formatting range.

## 4. Live preview + model logic
- [x] `MarkdownModel` (pure): heading extraction (level+text, from `#`/`##`...), word/char counting, and a `PreviewDebouncer`/policy (returns render-needed + delay from elapsed time). No Qt GUI. `test_markdownmodel` asserts heading levels/text, counts, and the debounce decision.
- [x] Wire live preview: on `textChanged`, schedule a debounced (≈120–200 ms) update that calls `RenderedDocument` **only if the preview is visible**; set the base URL from `currentFilePath`. Add scroll-ratio sync editor↔preview (best-effort, guard against feedback loops). Headless-safe.

## 5. Save / dirty / title
- [x] Save + Save As (Ctrl+S / Ctrl+Shift+S): write via `Document`, prompt Save As when untitled, update title bar to filename + `*` when dirty, show a status-bar message. `test_document` save-as path/dirty clearing.

## 6. Find / Replace
- [x] Find bar (Ctrl+F): small docked/overlay widget over the editor; highlights current match, next/prev, wrap, match count. Uses `QPlainTextEdit` selection/search APIs. `test_editor` match count.
- [x] Replace (Ctrl+H): adds replace/replace-all controls operating on the current match; `test_editor` asserts replace-all result.

## 7. Theme & settings
- [x] `Settings` wrapper: `QSettings("mdit","mdit")` (or org/app) with typed accessors for theme (auto/light/dark), previewVisible (default true), splitterRatio (default 0.5), recentFiles (cap default 5). Constructor takes an override for an isolated temp path (for tests). `test_settings` uses an isolated temp `QSettings` and asserts round-trip + cap.
- [x] `Theme` + toggle: static `apply(QApplication&, bool dark)` sets Fusion style + light/dark palette + a QSS stylesheet; re-applies the `RenderedDocument` stylesheet on toggle. Menu/toolbar action, persisted via `Settings`. `test_settings` persistence (no real window).

## 8. Preview toggle + status bar
- [x] Preview toggle action (View menu, default ON): hide/show the `PreviewPane` (editor expands to full width when hidden), restore the persisted ratio when re-shown. Splitter ratio persisted on change. `test_mainwindow` (offscreen) verifies default ON, toggle, ratio.
- [x] Word/char count in the status bar, updated on the debounce; a "modified" indicator.

## 9. Export + recent files
- [x] Export HTML: write `RenderedDocument.toHtml()` to the chosen `.html` file (default name from the doc stem); base URL/paths preserved. Test asserts non-empty HTML (e.g. contains `<!DOCTYPE` or `<html`).
- [x] Export PDF: render `RenderedDocument` via `QTextDocument::print` into a `QPdfWriter` (set page size/DPI); save `.pdf`. Test asserts a `%PDF` magic header.
- [x] Recent files: a `File` submenu of the last N persisted paths; reopening with the dirty guard. `test_settings` cap.

## 10. CLI + finalize GitHub-ready
- [x] CLI open + DnD polished: `mdit <file>.md` opens if the path exists (else empty/untitled); drag-and-drop of a `.md`/`.markdown` file onto the window; update the window/status to show the opened path. `test_document`/entry-point check.
- [x] Doc files: write the full `README.md` (title, purpose, features, Build prereqs+commands, Run + CLI-open, Usage/shortcuts table, license note) and update `CHANGELOG.md` so every completed feature has a line under `## [0.1.0]`. Add `test_github_ready` (asserts README, LICENSE contains "MIT License" + copyright, CHANGELOG `## [0.1.0]`, spec.md, CMake `project()`). Ensure `.gitignore` complete.
- [x] Final polish: run the FULL suite green via `test_build.py`; update `RALPH_SUMMARY.md`; append any learned gotchas to `AGENTS.md`; `git add -A && git commit` the finished GitHub-ready repository.

## 11. Post-loop polish (added after the loop finished, on the handoff copy)
- [x] **Undo / redo while editing** (`Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y`): Edit → Undo / Redo actions (shortcut-scoped to the editor) whose enabled state follows the editor's undo stack. `EditorPane::replaceAll()` is now ONE undo step (it previously went through `setPlainText()`, which cleared the stack). The dirty flag is undo-aware (`markClean()` after open / new / save), so undoing back to the saved content drops the `*`. `test_undo` (12 cases) added; README / CHANGELOG / spec.md kept in sync.

## 12. Post-loop polish II (find popup, theme button, app icon)
- [x] Find/Replace as its own popup: `FindBar` is now a non-modal `QDialog` centered over the main window on every show (title bar names the mode, Esc/close returns focus to the editor, re-centers when the replace row appears). `test_findwindow` added.
- [x] Theme toggle: moved to the far right of the toolbar as an icon-only sun/moon emoji button (`☀️` light / `🌙` dark) with a state-naming tooltip, following and persisting the last theme; `View → Theme` (`Ctrl+T`) kept as the menu twin. Covered in `test_theme`.
- [x] Application icon: Pengy-mirroring white disc (transparent outside) with a `#` mark, generated by `tools/make_icon.py` (16…512 PNG + `assets/mdit.svg`), compiled in via `resources/mdit.qrc` (`Q_INIT_RESOURCE` for the static lib) and set on the app + window; install rules + `packaging/mdit.desktop`. `test_icon` pins the geometry/pixels.
- [x] Single row of chrome: the `QToolBar` was removed (it duplicated the menus) — the menu bar is the only chrome row, with the theme toggle in its right-hand corner (`QMenuBar::setCornerWidget`). `test_smoke`/`test_theme` updated.
- [x] Dock/taskbar icon fixed: `QGuiApplication::setDesktopFileName("mdit")` (Wayland app id <-> `mdit.desktop`), `StartupWMClass=mdit`, user-level install (`cmake --install build --prefix ~/.local` -> `~/.local/bin/mdit` + hicolor icons + desktop entry). Verified GTK resolves `mdit` to our PNG and `xdg-mime query default text/markdown` reports `mdit.desktop`.

## 14. Post-loop polish V (theming system: modes x accents x UI scale)
- [x] Token-based theming stolen from PengyCPP (`themehelper.h`): `Theme::make(mode, accent)` -> 36 named tokens; 3 modes (System/Light/Dark) x 8 accents (Default+7) = 16 palettes; `Default` keeps the original neutral look byte-for-byte, other accents tint the surfaces and drive primary/selection/link. `appStyleSheet()` substitutes `@{token}` names and scales its metrics.
- [x] UI scale (75-200%, PengyCPP's multiplier model) applied to the application font, the editor's monospace font and the QSS metrics; persisted (`uiScale`) with clamping (50..300 on read and write).
- [x] Everything consumes the tokens: QPalette, app QSS, the editor highlighter (`MarkdownHighlighter::applyTheme` — accent-tinted headings/links) and `RenderedDocument::stylesheet(theme)` for the preview + HTML/PDF exports.
- [x] UI: `View > Theme / Accent / UI Scale` radio menus + the menu-bar corner button's flat popup sharing the same QActions (the sun/moon toggle and `Ctrl+T` unchanged); mode/accent/scale re-applied and ticked on startup.
- [x] Tests: `test_theme` rewritten (20 cases) + accent/scale cases in `test_settings`; 23 binaries green. Verified visually in light/dark/accents and at 150% scale.

## 15. Post-loop polish VI (GitHub CI/CD + deployables, stolen from PengyCPP)
- [x] `.github/workflows/ci.yml`: Linux/macOS/Windows build + `ctest` on every push/PR (+ `mdit --version/--help` smoke on Linux).
- [x] `.github/workflows/release.yml`: on `v*` tags -> Linux `.deb` + AppImage, macOS `.dmg`, Windows `.zip`, uploaded via softprops/action-gh-release.
- [x] `build_deb.sh`: payload from `cmake --install` (so the package can't drift), Qt6 `Depends` with the pre-/post-t64 alternations, copyright + gzipped changelog + `postinst` (update-desktop-database, gtk-update-icon-cache, xdg-desktop-menu forceupdate), version parsed from CMakeLists with the leading `v` stripped.
- [x] `build_appimage.sh`: AppDir from `cmake --install`, linuxdeploy + Qt plugin, Wayland platform plugin + runtime libs bundled and VERIFIED after packaging.
- [x] `build_macos.sh` (.app + generated .icns + macdeployqt + hdiutil dmg), `build_windows.bat` (windeployqt + zip).
- [x] `check_release.sh` pre-flight (version/icon/docs/packaging inputs/clean build+suite/tree) and `.gitignore` for the artifacts.
- [x] Verified locally: built + unpacked the `.deb` (GTK icon lookup resolves the packaged hicolor PNGs; the entry is registered for text/markdown) and built + ran the AppImage (29 MB, wayland+xcb).

## 16. Post-loop fix (settings isolation + shipped defaults)
- [x] Tests can no longer touch the user's config: `tests/testmain.h` redirects the Qt settings path into a temp dir at load time (`isolateUserSettings()` + an auto-invoked static), applied to every widget test, with a `test_settings` guard asserting the default settings path is not `~/.config/mdit/mdit.conf`. (The theme suite had been writing `theme=dark`/`themeAccent=red`/`uiScale=110` there, which looked like the app "resetting" to a red theme.)
- [x] Shipped defaults are **Light + Default accent + 100% UI scale** (System/auto is opt-in).
