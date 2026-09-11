# AGENTS.md — learnings for this Ralph run

(Update after each subtask: gotchas, conventions, reusable patterns.)

## Conventions established (keep these)

- **Build:** `cmake -S . -B build -G Ninja -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`. The top-level `build.sh` and `tests/test_build.py` (pytest shim: configure→build→ctest) both wrap this. Harness (`verify.py`) detects `CMakeLists.txt` and runs cmake+ctest directly (kind priority: rust → cmake → python).
- **Qt6 components:** `find_package(Qt6 COMPONENTS Widgets Gui PrintSupport REQUIRED)` at top level. Tests link `Qt6::Test` (+ Widgets/Gui/PrintSupport). PrintSupport is optional at runtime for CUPS but **REQUIRED** for `QPdfWriter` PDF export.
- **C++ standard:** `CMAKE_CXX_STANDARD 20`, `REQUIRED`, `EXTENSIONS OFF` (pure C++20, no GNU exts).
- **Test registration pattern:** in `tests/CMakeLists.txt` use `add_mdit_test(<name>)` — it creates an exe from `<name>.cpp`, links the Qt6 libs, sets AUTOMOC, adds a CTest entry, and forces `QT_QPA_PLATFORM=offscreen` + 120s timeout. **Always use this helper** for new tests; every QtTest binary is headless-safe this way. Add the new `add_mdit_test(...)` call as you create each `tests/test_*.cpp`.
- **Sources globbed:** top-level `file(GLOB MDIT_SOURCES CONFIGURE_DEPENDS src/*.cpp)`. New `src/*.cpp` modules are picked up automatically (no CMake edit needed) once the file exists — but the **test** CMake call must still be added manually.
- **Headless rule:** never open a real window in tests; pure-logic classes (Document, RenderedDocument, MarkdownModel, Settings) need no `QApplication` at all. Widget tests (EditorPane, PreviewPane, MainWindow) need a `QApplication`: `QTEST_MAIN` only creates a **QCoreApplication** (no widgets), so for any widget test write a custom `main` that constructs a `QApplication` before `QTest::qExec(&t, argc, argv)` (the offscreen env from `add_mdit_test` handles the no-display case).

## Decisions / notes per subtask
### 10.2 Doc files + `test_github_ready` (done this iteration)
- **The repo contract is now guarded by CTest:** new `tests/test_github_ready.cpp` (PURE logic → `QTEST_GUILESS_MAIN`, no `QApplication`; registered via `add_mdit_test` + `target_compile_definitions(test_github_ready PRIVATE MDIT_REPO_ROOT="${CMAKE_SOURCE_DIR}")`). 7 slots: root-resolves sanity, README (exists, >500 chars, starts `# mdit`, has `## Features`/`## Build`/`## Run`/`## Usage`/`Prerequisites`/`CMake`/`Qt6`, contains the **exact** spec build command string `cmake -S . -B build -DBUILD_TESTS=ON -G Ninja && cmake --build build`, the CLI-open form `mdit <file>.md`, and `MIT` + `Copyright (c) 2026 Pat Wendorf`), LICENSE (`MIT License` + copyright line), CHANGELOG (`## [0.1.0]` + `Keep a Changelog`), spec.md (present, >500 chars, `# spec.md`), `.gitignore` (entries `build/`, `build-*/`, `.cache/`, `*.user`, `.qmake.stash`, `*.pdf`, `*.html`), CMakeLists.txt (`project(mdit` + `enable_testing(` + `CMAKE_CXX_STANDARD 20` + the exact `find_package(Qt6 COMPONENTS Widgets Gui PrintSupport REQUIRED)` regex). **Any future README/CHANGELOG/LICENSE edit that breaks these exact strings fails the suite — that is the point; if you intentionally change a mandated string, update the test in the same commit.**
- **Repo-root resolution (reusable for any repo-file test):** compile definition `MDIT_REPO_ROOT` = `CMAKE_SOURCE_DIR` (set in `tests/CMakeLists.txt` right after `add_mdit_test`), with an in-test `#ifndef` fallback of `"../../"` (ctest's default working dir is `build/tests`). The test also asserts the root resolves (a `CMakeLists.txt` exists there) so a misconfigured seam fails loudly instead of reading empty files.
- **README finalization:** the long accumulating "Status: ... is now wired" blockquote is GONE — replaced by a full, stable feature list (split-pane/toggle/persisted ratio, highlighter+line numbers, debounced live preview + scroll sync, file I/O + guard + DnD + CLI, find/replace, status bar, theme, export, recent files) + Build (prereqs + exact command), Run (CLI-open semantics), Usage (shortcut table + menu map: File/New/Open/Save/Save As/Export→HTML…/PDF…/Recent Files/Exit, Edit/Find/Replace, View/Preview/Theme, Help/About). Verified against `MainWindow::buildUi()`'s actual menus/shortcuts before writing. If a menu/shortcut changes, update both.
- **CHANGELOG restructured:** ALL feature entries moved from `## [Unreleased]` under `## [0.1.0] - 2026-09-10` (the release this subtask line asked for: "every completed feature has a line under `## [0.1.0]`"); `## [Unreleased]` now carries only a placeholder line. Future completed features: add their bullet under `## [Unreleased]` (Keep-a-Changelog forward flow) — the `[0.1.0]` heading the test asserts will not move.
- **`.gitignore` was already complete** (all seven asserted entries present) — no change.
- **Test gotchas reused:** `QFile` needs `open()` before `readAll()` (no auto-open on this build); `QString::contains(const QByteArray&)` exists (used with `entry.toUtf8()` in a loop); helper functions return `QString` and contain NO `QVERIFY`/`QCOMPARE` (void-slot-only macro rule).
- No spec.md change needed (the subtask line + spec's "GitHub-ready repository contract" section were unambiguous). No new subtasks surfaced — next unchecked line is 10.3 (Final polish: full suite via `test_build.py`, `RALPH_SUMMARY.md`, AGENTS gotchas, commit). NOTE for 10.3: `RALPH_SUMMARY.md` does not exist yet and is NOT asserted by `test_github_ready` (kept in scope for 10.3 per the spec's contract list); create it then.

### 10.1 CLI open + DnD polished (done this iteration)
- **The CLI open is now a pure, testable decision: `src/AppCli.{h,cpp}`** (auto-globbed into `mdit_core`; `main.cpp` is excluded from the lib, so this lives in the lib and is linked by tests). `AppCli::fileToOpen(const QStringList &positionalArgs) → QString` returns the FIRST positional arg that is an **existing regular file with a `.md`/`.markdown` suffix** (case-insensitive), as an **absolute** path; returns **empty** for a missing path, a directory, a non-markdown file, or no args — the caller then leaves the app in the empty untitled state. `main.cpp` calls `AppCli::fileToOpen(parser.positionalArguments())` and, when non-empty, `window.openFile(result)`. **Why a namespace + free function (not a class):** the entry-point decision is one pure function — a class would be over-engineering; it has no `Q_OBJECT`, so no moc needed (a fresh non-`Q_OBJECT` `.cpp` in `mdit_core` is picked up by the `GLOB CONFIGURE_DEPENDS` on the next configure/build; no AUTOMOC-vtable issue like a `Q_OBJECT` module would have).
- **Design decision (documented, not a corner cut): the CLI filters to markdown files**, not just "exists". Rationale: the app "loads only markdown files" (spec Purpose) and the DnD handler already filters to `.md`/`.markdown`, so a CLI arg for an existing `.txt` is a "bad arg" that leaves the app untitled — consistent with DnD. The subtask line's "opens if the path exists" is honored for the markdown case; the spec's "valid `.md` path" / "a bad or missing arg" wording is what the filter implements. spec.md §Input + §5 were updated to record this.
- **`MainWindow::openFile()` now shows the OPENED FULL PATH in the status bar** — `showStatus(tr("Opened %1").arg(info.absoluteFilePath()))` (was `info.fileName()`). This is the "update the window/status to show the opened path" requirement; it applies to CLI, menu-open, DnD, and recent-files (all funnel through `openFile`). No existing test asserted the old "Opened <filename>" text, so nothing else needed changing. The title still carries the file name (unchanged).
- **DnD was already done** (subtask 1.2: `handleDroppedPaths`/`dragEnterEvent`/`dropEvent`, tested by `test_opennew::dropOpensMarkdownOnly`); this subtask only added the shared status-path behavior + tied it into the entry-point test. No DnD code changed.
- **Test = new `tests/test_entrypoint.cpp`** (WIDGET, custom `QApplication` main, offscreen; `add_mdit_test(test_entrypoint)`). 11 slots: the pure `AppCli::fileToOpen` decision (no-args/empty-arg → empty; existing `.md` and `.markdown` → path; `.MD` case-insensitive; missing → empty; existing `.txt` → empty; a directory named `*.md` → empty; first-valid-wins skipping a missing + a non-md path) and the window-level entry point (open existing → document path + content + **status `Opened <path>`** + title contains the filename; a missing arg → untitled/empty/bare-`mdit` title; an existing non-markdown arg → untitled), plus a drop opening a `.md` with the same status path. **Every window is built with an isolated temp-file `QSettings` backing** (`MainWindow(nullptr, &ini)` on a `QSettings(dir+"/test.ini", QSettings::IniFormat)`) because `openFile()` records the file in Recent Files (`addRecentFile` → `sync()`), so a bare `MainWindow` would write the user's real settings.
- **Gotcha (recurring incomplete-type class):** `EditorPane`/`PreviewPane` are only forward-declared in `MainWindow.h`; a test that calls a method on `w.editorPane()` (e.g. `toPlainText()`) needs a direct `#include "EditorPane.h"`. Same family as the `QStatusBar`/`QMenuBar` notes — when you drive a child widget's API from a test, include that widget's own header.
- No new subtasks surfaced — next unchecked line is 10.2 (Doc files + `test_github_ready`).

### 9.3 Recent files (done this iteration)
- **`File → Recent Files` submenu is now real.** New `QMenu *m_recentMenu` created in `buildUi()` right after the Export submenu (before the File-menu separator/Exit). Two new private methods: `addRecentFile(path)` (no-op on empty → `Settings::addRecentFile` + `sync()` + `rebuildRecentMenu()`) and `rebuildRecentMenu()` (deletes all existing entry actions via `qDeleteAll(actions())`, then adds one enabled `QAction` per persisted path in most-recent-first order with the path in the action's `data`; when empty it adds a disabled `"No recent files"` placeholder and disables the whole submenu item). `onOpenRecent()` slot reads `sender()`'s `data()` and calls `openFile(path)` — so the **dirty guard runs first** (no separate guard needed).
- **When files join the list:** `openFile()` calls `addRecentFile(info.absoluteFilePath())` on success (after the status line), and both `saveDocument()` and `saveAsDocument()` call `addRecentFile(...)` after a successful write. So "opened/saved" per spec §11. `rebuildRecentMenu()` is also called once at the end of `buildUi()` to seed the submenu from any persisted list on startup.
- **Reusing existing Settings:** no Settings change was needed — `recentFiles()`/`addRecentFile()`/`recentCap()` (default 5, most-recent-first, dedupe, cap) already existed from subtask 7. The `test_settings` cap coverage (`addRecentFileCappedMostRecentFirst` etc.) was already there and still passes.
- **Test = 6 new slots in `test_mainwindow`** (widget, custom `QApplication` main, offscreen). Helper `seedRecent(ini, dir, n)` writes N `recent%N.md` paths to an isolated backing and returns the stored list. Slots: `recentMenuReflectsPersistedList` (menu lists seeded paths, most-recent-first, enabled), `recentMenuDisabledWhenEmpty` (fresh settings → disabled item + `"No recent files"` placeholder), `openFileAddsToRecentFilesCapped` (opening 6 files caps at 5, drops oldest, and the list reaches disk so a fresh window on the same backing shows 5), `saveAddsToRecentFiles` (Save records the current path), `recentEntryReopensFile` (trigger the entry → the file reopens, content + path verified), and `recentReopenRespectsDirtyGuard` (a `NoDiscardWindow` — a local subclass overriding `askDiscardChoice()` → `Cancel` — with a dirty current doc refuses a guarded re-open of another file; content/path stay intact, still dirty).
- **Gotcha:** `tests/test_mainwindow.cpp::writeTemp()` takes a `const QByteArray&` for the body, but `.arg()` on a `QStringLiteral` yields a `QString` — call `.toUtf8()` on it (raw `const char*` bodies convert implicitly and are fine).
- No spec.md change needed — spec §11 "Recent files — a `File` submenu of the last N (default 5) opened/saved files, persisted via `Settings`; clicking reopens (dirty guard first)" is exactly what is built and tested. No new subtasks surfaced — next unchecked line is 10.1 (CLI open + DnD polished).

### 9.2 Export PDF (done this iteration)
- **`File → Export → PDF...` is now real** (was the placeholder `onExportPdf`). Mirrors the HTML export exactly: new public API on `MainWindow` — `bool exportPdfDocument()`, `QString suggestedPdfName() const` (doc stem + `.pdf`; untitled → `untitled.pdf`), and `virtual QString askExportPdfPath(const QString &suggestedName)` — the **fourth dialog seam** (after `askDiscardChoice()`/`askSaveAsPath()`/`askExportHtmlPath()`); default shows a real modal `QFileDialog::getSaveFileName` with the PDF filter. `setPdfResolutionDpi(int)` (default **96**, clamped to ≥1) + `pdfResolutionDpi()` expose the DPI. Same semantics as HTML: exports the **current editor content** (unsaved edits included), derives the base URL exactly like `updateLivePreview()` (md dir, empty when untitled), never touches the dirty flag, cancel writes nothing, bad path → `Could not export <name>`, success → `Exported <name>`.
- **The render+print lives in a NEW pure helper `RenderedDocument::renderPdf(md, baseUrl, dark, outPath, pageSizeId, dpi) -> bool`** (in `src/RenderedDocument.{h,cpp}`): renders into a local `QTextDocument` via the shared `render()` (so the theme stylesheet + base URL are applied exactly as on screen — "what you preview is what you export"), then `doc.print(&writer)` into a `QPdfWriter`. It returns true **only** when a real PDF was written (it re-opens the file and checks byte 0 == `%PDF-`), which is also the natural failure signal for an unwritable path (the writer never writes the header).
- **Qt6 PDF page config — there is NO `QPageSetup`** (removed in Qt6). `QPdfWriter : QObject, QPagedPaintDevice`; page size/orientation/margins are set with a **`QPageLayout`**: `QPageLayout(QPageSize(pageId), QPageLayout::Portrait, QMarginsF(...), QPageLayout::Millimeter)` then `writer.setPageLayout(layout)`. `QPageSize::setOrientation` does NOT exist — orientation lives on `QPageLayout`. `QPdfWriter::setResolution(int dpi)` sets the DPI. `QPageSize::A4` (etc.) and `QPageLayout::Portrait`/`Millimeter` are all present. The **`QPageLayout` ctor's 4th arg is `Unit` and defaults to `Point`** — pass `QPageLayout::Millimeter` explicitly if you want mm margins.
- **`QPdfWriter(const QString &filename)` opens the file in its ctor and writes the `%PDF-` header immediately; the file is finalized in the dtor.** Scope the writer in a block so it is destroyed (and the PDF closed) BEFORE you read the file back to verify the header. Printing is `doc.print(&writer)` — `QPdfWriter` IS a `QPagedPaintDevice` (that's `QTextDocument::print(QPagedPaintDevice*)`'s exact argument; **no `QPainter` needed**).
- **Offscreen + no CUPS is fine for `QPdfWriter`** — it uses its own PDF paint engine, independent of the CUPS/print backend (the `Could NOT find Cups` configure warning is harmless, as already noted). A bad path makes `QPainter::begin()` return false (a benign `QWARN`), no header is written, and the `%PDF-` check correctly reports failure.
- **Real PDF sanity:** a 2-line markdown doc produced a **~10 KB** PDF containing a `/Page` object and a trailing `%EOF` marker — so "magic header + size>0" was too weak; `test_export_pdf` also asserts `contains("/Page")` and `contains("%EOF")` to catch a degenerate/empty PDF. (The `%EOF` marker is a SINGLE percent — do not write `%%EOF`, that's printf syntax.)
- **Test = new `test_export_pdf`** (widget, custom `QApplication` main, offscreen; `ScriptedPdfWindow` forwards the `(parent, QSettings*backing)` ctor and overrides `askExportPdfPath`; every window built with an isolated temp-file `QSettings` via `MainWindow(nullptr, &ini)`). 8 slots: renderPdf magic-header + `/Page` + `%EOF` + non-degenerate, renderPdf fails on an unwritable path, window export (valid PDF + "Exported" status + still-not-dirty), suggested stem name (saved `notes.pdf` + untitled `untitled.pdf`, driven via the `onExportPdf()` slot), current-editor-content + dirty-preserving, cancel writes nothing, bad path → "Could not export" + state unchanged, and DPI configurable + clamped (0/-50 → 1).
- No spec.md change needed (spec §10 "Export → PDF renders the document to a .pdf file via `QTextDocument::print` / `QPdfWriter`" and "set page size/DPI" is exactly what is now built and tested). No new subtasks surfaced — next unchecked line is 9.3 (Recent files: `File` submenu of the last N persisted paths, reopening with the dirty guard, `test_settings` cap).

### 9.1 Export HTML (done this iteration)
- **`File → Export → HTML...` is now real** (was a placeholder slot). New public API on `MainWindow`: `bool exportHtmlDocument()`, `QString suggestedHtmlName() const` (doc stem + `.html`; untitled → `untitled.html`), and `virtual QString askExportHtmlPath(const QString &suggestedName)` — the **third dialog seam** (after `askDiscardChoice()`/`askSaveAsPath()`); the default shows a real modal `QFileDialog::getSaveFileName` with the HTML filter. **Any future dialog (PDF export, subtask 9.2) should reuse the same virtual-seam pattern.**
- **Export = same renderer as the preview** ("what you preview is what you export"): `m_doc.setText(m_editor->toPlainText())` (unsaved edits included; a no-op for the dirty flag in practice since `onEditorTextChanged` already synced), base URL derived exactly like `updateLivePreview()` (md dir, empty when untitled), `RenderedDocument::render(text, base, m_dark, doc)` into a **local** `QTextDocument`, then `doc.toHtml()`.
- **New pure helper `RenderedDocument::standaloneHtml(title, documentHtml, dark)`** (in `src/RenderedDocument.{h,cpp}`): wraps the toHtml() fragment into a real standalone page — `<!DOCTYPE html>`, `<html>`, UTF-8 charset, the title (HTML-escaped via a local `escapeHtmlAttribute`), `RenderedDocument::stylesheet(dark)` in a `<style>` block, and the **lifted `<body>` content**. The body is lifted between the closing `>` of the opening `<body ...>` tag (Qt emits an ATTRIBUTED `<body link=... vlink=...>` on this build) and the last `</body>`; if no `<body>` tag exists the raw fragment is used as the body (still non-empty + standalone). Keeping this in the pure `RenderedDocument` module (not the widget) is deliberate: the next subtask (PDF export) will render into a local doc the same way, and the pure helper stays GUI-less-testable.
- **Spec §10 image copying is implemented (not skipped):** every `<img src="...">` in the toHtml() fragment (collected with a `QRegularExpression` `<img[^>]*\bsrc=([\"'])([^\"']+)\\1`, quote-capturing, de-duplicated; entities `&amp;`/`&quot;`/`&lt;`/`&gt;`/`&#39;` unescaped first) is, **when relative** (no `:`, no leading `/`/`~`), **copied from the md dir to the HTML's dir preserving the relative path** (`QDir().mkpath` + `QFile::copy`; skipped when it already exists or the source is missing — a broken link degrades to alt text like in the preview). Keeping the same relative path means the src needs NO rewriting in the HTML.
- **Export side effects:** success → `showStatus("Exported <name>")`, return true; cancel (empty path) → nothing written, false; un-writable path → `showStatus("Could not export <name>")`, false. The document's dirty state is **deliberately untouched** (export is a read-only view of the content) — `test_export_html::exportUsesCurrentEditorContentAndKeepsDirty` pins that.
- **Test = new `test_export_html`** (widget, custom `QApplication` main, offscreen; `ScriptedExportWindow` overrides `askExportHtmlPath`; **every window is built with an isolated temp-file `QSettings` backing** via `MainWindow(nullptr, &ini)` — `setDarkTheme` persists, so a bare `MainWindow` in a theme-touching test would hit the user's real settings). 9 slots: standaloneHtml wrap (doctype/title-escaping/light+dark stylesheet/body lift), no-`<body>` fallback, export writes non-empty standalone HTML (doctype + content + `background-color: #ffffff;` + `<style>`) + "Exported" status + still-not-dirty, suggested stem name (saved + untitled), current-editor-content export (unsaved edit wins over on-disk text; .md file itself untouched), cancel writes nothing, bad path fails with "Could not export" + state unchanged, dark-theme export carries `#1e1e1e`, and relative-image copying (top-level + `screenshots/x.png` subdirectory, recreated + byte-identical via `QPixmap::save` 4x4 PNGs).
- **Stripped-Qt gotchas hit (this run):**
  - **`absolute` is a MACRO on this Qt build** — `const bool absolute = ...` is a hard parse error ("expected primary-expression before '||'"; GCC even suggests `std::filesystem::absolute`). Don't use `absolute` (or `relative`) as an identifier in code that includes the file-system headers.
  - **A test subclass that declares its own ctor (or none) HIDES the base ctor:** `ScriptedExportWindow w(nullptr, &ini);` failed with "candidate expects 0 arguments, 2 provided" until I added a forwarding ctor `ScriptedExportWindow(QWidget *parent = nullptr, QSettings *backing = nullptr) : MainWindow(parent, backing) {}`. (test_save's `ScriptedSaveWindow` never hit this because it is only default-constructed.)
  - **No static `QFile::size(const QString&)`** on this build — use `QFileInfo(path).size()`.
- **toHtml() shape confirmed (for the PDF subtask):** the fragment opens the body as `<body link=".." vlink="..">` (attributed), and images come out as `<img ... src="relative.png">` with the relative src preserved — both facts the standaloneHtml lift and the image-copy scanner rely on.
- No spec.md change needed (spec §10 "Export reuses the same RenderedDocument output (HTML via toHtml())… images referenced by relative path are copied… referenced by path relative to the saved HTML" is exactly what is now built and tested). No new subtasks surfaced — next unchecked line is 9.2 (Export PDF via `QTextDocument::print` into a `QPdfWriter`; PrintSupport is already REQUIRED and linked into every test binary).

### 8.2 Status-bar word/char counts + modified indicator (done this iteration)
- **Two PERMANENT status-bar `QLabel`s** (right-aligned, via `statusBar()->addPermanentWidget()`), created in `buildUi()` right after the transient `showMessage("Ready")`: `m_countsLabel` (objectName `StatusCounts`) and `m_modifiedLabel` (objectName `StatusModified`). Permanent widgets are independent of the transient message area, so a `Saved <name>` toast never clobbers the readouts. Getters `statusCountsLabel()` / `statusModifiedLabel()` are public so tests read `->text()` headlessly; `refreshStatusCounts()` / `updateModifiedIndicator()` are public too (a test can force a synchronous refresh to avoid a `qWait`).
- **Counts = debounced; modified = O(1)/live.** `refreshStatusCounts()` reads `m_editor->wordCount()/charCount()` (the MarkdownModel canonical counts) and sets the label to `"%1 word(s), %2 character(s)"` with correct singular/plural. It is called from `onPreviewTimerTimeout()` (the debounce — spec §3 "counts update on the same debounce"), `openFile()` (immediate, no debounce wait on load), `newDocumentNoGuard()` (back to 0/0), and `buildUi()` (initial 0/0). It is DELIBERATELY not called on every keystroke: counting is O(n), which the spec keeps off the per-keystroke path. `updateModifiedIndicator()` sets `Modified`/`Unmodified` from `m_doc.dirty()` and is called from `updateTitle()` (which runs on typing/open/new/save/save-as) — O(1), so it's live.
- **KEY DESIGN DECISION — the debounce is no longer gated on preview visibility.** `schedulePreviewUpdate()` previously `return`ed early when `!isPreviewVisible()`, so a hidden preview meant the timer never fired and the counts would go stale (the status bar is on the WINDOW, not the pane — a user hiding the preview and typing would see stale counts). Now the timer always runs; on timeout it refreshes the (cheap) count unconditionally and calls `updateLivePreview()`, which is STILL visibility-guarded internally (no render while hidden). So a hidden preview still costs zero render time — only the cheap O(n) count runs on the timer. This is the ONLY change to the live-preview timing; `test_livepreview::noRenderWhilePreviewHidden_thenRendersOnReshow` still passes because the render guard is inside `updateLivePreview()`, not at the schedule site.
- **Test (6 new slots in `test_mainwindow`, offscreen, isolated temp `QSettings`):** `statusBarCountsAndModifiedInitially` (fresh → "0 words, 0 characters" + "Unmodified"), `countsRefreshOnDebounce_notPerKeystroke` (right after `setPlainText` the label is STILL "0 words, 0 characters" while the modified indicator is already "Modified" — proving the count is deferred to the debounce, not the keystroke; after `qWait` → "5 words, 23 characters"), `countsUpdateWhilePreviewHidden` (hide preview, type, `qWait` → "3 words, 17 characters" — proves the decoupling), `countsAfterOpenFile_immediate` (openFile → real counts, not stale 0/0), `countsPluralization` (1 word / 1 character / 2 words), `modifiedIndicator_followsDirty` (clean→Unmodified, clean load→Unmodified, edit→Modified immediately, save→Unmodified; NO modal because it uses openFile+saveDocument, never a dirty-guarded New/Open). A local `writeTemp(dir,name,QByteArray)` helper was added (the `bytes` arg is `const QByteArray&` — pass a C string, not a `QString`/`QStringLiteral`, or it's a compile error).
- **`QLabel` is forward-declared in MainWindow.h** (`class QLabel;`) — the header only needs the pointer type; `<QLabel>` is included in MainWindow.cpp. Same pattern as the other forward-declared widget members.
- No new subtasks surfaced. Next unchecked line is section 9 (Export HTML / PDF / Recent files).

### 8.1 Preview toggle + splitter-ratio persistence (done this iteration)
- **What existed vs. what was built:** the `View → Preview` action (checkable, default checked, `Ctrl+Shift+P`) and `togglePreview()` already existed from the 0.2 shell, but nothing was persisted and re-show reset to a hardcoded 550/550. This subtask closed that gap entirely in `MainWindow.{h,cpp}`:
  - **Startup honors persisted state:** `buildUi()` computes initial `setSizes` from `m_settings.splitterRatio()` (default 0.5; the splitter scales the requested sizes to its real width, so ratio 0.8 → 880/220 of the 1100 px window) and, if `!m_settings.previewVisible()`, hides the pane + unchecks the action. **Ordering gotcha (caught by a SIGSEGV):** that hide block MUST run AFTER the menu-bar section of `buildUi()` — `m_previewToggleAct` is created there, and calling `setChecked()` on the still-null pointer crashed `QAction::setChecked` at `+4` (`mov 0x8(%rdi)` on rdi=0).
  - **`togglePreview()`** now also does `m_settings.setPreviewVisible(nowVisible); m_settings.sync();` and, on re-show, calls the new `applyPersistedRatio()` (persisted ratio, default 0.5) INSTEAD of `setSizes({550,550})` — "restore the persisted ratio when re-shown" is now literally true.
  - **`setSplitterRatio(double)`** (new public API): clamps to **[0.05, 0.95]** (the splitter is non-collapsible; 0.0/1.0 would fight the widgets' minimum sizes), persists through Settings + `sync()`, and applies `setSizes` scaled to the splitter's current total width. If the splitter has no width yet (never shown offscreen) it sets `m_pendingSplitterRatio` and the new `resizeEvent()` override re-applies from Settings (the value is already on disk, so `applyPersistedRatio()` suffices — no second member needed).
  - **`persistSplitterRatio()`** (connected to `QSplitter::splitterMoved` — i.e. real handle drags): writes `splitterRatio()` to Settings, but **only while the preview is visible**. **Gotcha:** with the preview hidden, `QSplitter::sizes()` reports a DEGENERATE single-pane list (total = editor width only → ratio ≈ 1.0); persisting that would clobber the real remembered split. So a hidden-pane splitterMoved is a no-op for persistence (the real ratio was already persisted at the last drag/toggle). Same reason `splitterRatio()` is not assertable while hidden.
- **`QSplitter::setSizes()` does NOT emit `splitterMoved` on this build** (Qt emits it from `moveSplitter()`, i.e. handle drags, only) — programmatic `setSizes`/`setSplitterRatio` must persist explicitly, which is why `setSplitterRatio()` writes Settings itself rather than relying on the signal.
- **Test = new `test_mainwindow`** (widget, custom `QApplication` main, offscreen; every window built with an isolated temp-file `QSettings` backing via the `MainWindow(nullptr, &ini)` seam). 8 slots: `previewOnByDefault`, `viewMenuToggleActionHidesAndShows` (finds the View-menu "Preview" action by title, asserts checkable + exact `QKeySequence(CTRL|SHIFT|Key_P)` + trigger flips pane and checked state), `togglePersistsVisibilityForNextStart` (raw `previewVisible` key read from a fresh `QSettings` on the same file + a "restart" window starts hidden with unchecked action), `editorTakesFullWidthWhenPreviewHidden` (`show()`+`processEvents()` first so the offscreen splitter gets a real width; hidden → `editorPane()->width() >= splitter()->width()-10`), `splitterRatioPersistedOnChange` (setSplitterRatio(0.7) → live ratio ≈0.7 AND the raw `splitterRatio` key ≈0.7 on a fresh QSettings = real disk persistence), `reshownPreviewRestoresPersistedRatio` (0.7 survives an off/on cycle — not a 50/50 reset), `startupRestoresPersistedRatioAndVisibility` (seeded 0.8 + hidden via a `Settings` instance; restart → hidden, re-show → ≈0.8), `freshSettingsStartAtFiftyFifty` (defaults: visible + ≈0.5).
- **Offscreen geometry (reuse for 8.2 status work and beyond):** `w.show(); QApplication::processEvents();` DOES give the splitter/panes real non-zero widths under the offscreen platform (confirmed: `splitter()->width() > 0` and pane widths follow the requested sizes within rounding). The benign `QWARN "This plugin does not support propagateSizeHints()"` prints on `show()` — noise, not a failure.
- **Include gotcha:** `tests/test_mainwindow.cpp` needed DIRECT `#include`s of `<QMenuBar>`, `<QSplitter>`, `"EditorPane.h"`, `"PreviewPane.h"` — `MainWindow.h` only forward-declares the panes and this Qt's transitive headers leave `QMenuBar`/`QSplitter` incomplete (the same pattern as the `QStatusBar` note in the Gotchas).
- No spec.md change needed (spec §2 "the splitter handle is draggable and the ratio is persisted … preview ON by default … Ctrl+Shift+P or a View menu checkbox … remembered ratio is restored" is exactly what is now built and tested). No new subtasks surfaced — next unchecked line is 8.2 (word/char count + modified indicator in the status bar; note `EditorPane`/`MarkdownModel` counts + `MarkdownModel` are ready-made inputs, and `onPreviewTimerTimeout` is the debounce hook the spec points at).

### 7.2 Theme + toggle (done this iteration)
- **`Theme` is now the full app theme** (`src/Theme.{h,cpp}`): `apply(app, dark)` = `setStyle("Fusion")` + a light/dark `QPalette` + **`app.setStyleSheet(stylesheet(dark))`** (a new application-level QSS — menus/toolbar/status/editor/preview chrome; distinct from the per-document `RenderedDocument::stylesheet()`). New pure helpers: **`static bool resolveDark(Settings::ThemeMode, bool systemDark)`** (Dark→true, Light→false, Auto→systemDark) and **`static bool isSystemDark(QApplication&)`** (reads `app.styleHints()->colorScheme() == Qt::ColorScheme::Dark`).
- **Theme-mode source of truth is `Settings::ThemeMode` (auto/light/dark); the widgets consume a concrete `bool dark`.** `Theme.h` now `#include "Settings.h"` (light header, no cycle) for the enum.
- **`QStyleHints` is in QtGui here (NOT QtWidgets), and `Qt::ColorScheme` is in QtCore (qnamespace.h):** `enum class ColorScheme { Unknown, Light, Dark }`. Include `<QStyleHints>` in Theme.cpp; `qApp->styleHints()->colorScheme()`.
- **`MainWindow` now owns `Settings m_settings` (by value, non-copyable) initialized in the ctor member-init list:** ctor is `explicit MainWindow(QWidget *parent = nullptr, QSettings *backing = nullptr)` → `: QMainWindow(parent), m_settings(backing)`. In the ctor BODY it resolves `m_dark = Theme::resolveDark(m_settings.theme(), Theme::isSystemDark(*qApp))` BEFORE `buildUi()` so the `View→Theme` action (`m_themeAct->setChecked(m_dark)`) and palette are correct from first paint. `main.cpp` dropped its hardcoded `Theme::apply(app,false)` (the window owns the theme now). **`setDarkTheme(bool)`** persists (`m_settings.setTheme(dark?Dark:Light)` — an explicit toggle never stores auto — + `sync()`), then `Theme::apply` + `m_editor->setHighlighterDark(dark)` + `updateLivePreview()` (re-renders the `RenderedDocument` stylesheet; a hidden preview re-renders on next show/edit, matching the "only render when visible" rule). `onToggleTheme()` = `setDarkTheme(!m_dark)` then `m_themeAct->setChecked(m_dark)`.
- **Test = new `test_theme` (WIDGET, custom `QApplication` main, offscreen), registered via `add_mdit_test(test_theme)`.** Slots: `resolveDarkMapping` (pure), `stylesheetDiffersByMode`, `applySetsFusionStylePaletteAndQss`, `mainwindowResolvesPersistedThemeOnStartup` (backing QSettings pre-seeded dark/light → `darkTheme()` matches), `mainwindowTogglePersistsAndReappliesPreview` (seed light, construct with `MainWindow(nullptr, &ini)`, seed editor text + `updateLivePreview()`, assert `previewPane()->displayedQTextDocument()->defaultStyleSheet() == RenderedDocument::stylesheet(false)`; after `onToggleTheme()` → `stylesheet(true)` + `ini.value("theme")=="dark"` + `settings()->theme()==Dark`; toggle back → light). Persistence is read straight off the SAME backing `QSettings` object the window writes to (it is `m_borrowed`, so the test's local `ini` sees the write). The Settings-level theme round-trip is already covered by `test_settings::themeRoundTrip`.
- **Test gotchas (this build):** (1) `QApplication::instance()` is inherited from `QCoreApplication` and returns `QCoreApplication*` — use the **`qApp`** macro (typed `QApplication*`) instead. (2) `app->style()->objectName()` is **empty** for Fusion here — do NOT assert it equals `"Fusion"`; assert `app->style() != nullptr` and check the palette/QSS instead. (3) `QTextDocument::defaultStyleSheet()` getter exists (paired with `setDefaultStyleSheet`). (4) `MainWindow(w)` will NOT bind a `QSettings*` to the first `QWidget*` param — pass `MainWindow(nullptr, &ini)`. (5) A `QSettings` passed as backing outlives the window (declared before the window) — the window never deletes a borrowed backing.
- No spec.md change (spec §9 "theme (light/dark) menu/toolbar toggle, Settings persist, re-render the preview stylesheet so both panes match, default follow system/auto" is exactly what was built). No new subtasks surfaced — next unchecked line is section 8 (preview toggle + status bar).

### 7.1 Settings wrapper (done this iteration)
- New `src/Settings.{h,cpp}` (auto-globbed into `mdit_core`) — a **typed `QSettings` wrapper**, the persistence layer the rest of the app will read through. `enum class ThemeMode { Auto, Light, Dark }`; accessors `theme()/setTheme()`, `previewVisible()/setPreviewVisible()` (default **true**), `splitterRatio()/setSplitterRatio()` (default **0.5**, clamped to `[0,1]` — out-of-range writes fall back to 0.5), `recentFiles()/setRecentFiles()/addRecentFile()`, `recentCap()/setRecentCap()` (default **5**). `addRecentFile`/`setRecentFiles` are most-recent-first, drop empties + duplicates, and trim to the cap. `sync()` flushes to storage.
- **Backing-pointer design (isolation seam):** `Settings` holds `QSettings *m_owned` (created only when no backing is given = the real `QSettings("mdit","mdit")`), `QSettings *m_borrowed`, and `QSettings *m_s` (the one used). `explicit Settings(QSettings *backing = nullptr)`; the dtor deletes `m_owned` only. **This is the test seam:** tests pass a `QSettings` backed on a temp file so the user's real settings are never read/written. `QSettings` is *movable* (unlike `QTextDocument`), so a helper may `return QSettings(...)` by value.
- **`main.cpp`/`MainWindow` are NOT yet wired to `Settings`** — that is the next subtask (7.2 Theme+toggle, persisted via Settings). Don't wire it here; this subtask is the wrapper + `test_settings` only.
- **`test_settings` is a pure-logic test** (`QTEST_GUILESS_MAIN`, no `QApplication`) — `QSettings`/`QTemporaryDir`/`QFile` are all GUI-free. 13 slots: identity/defaults, theme/preview/splitter round-trips (each writes via one `Settings`, `sync()`, then re-reads through a **fresh** `QSettings` on the same file to prove real disk persistence), splitter clamping, recent-files cap / most-recent-first / dedupe / settable-cap, and `writesReachDisk` (asserts an `.ini` actually landed under the temp dir and contains a written value).
- **STRIPPED-Qt gotchas (record for any future persistence work):**
  - **`QSettings::Scope` has ONLY `UserScope`/`SystemScope` — there is NO `UserConfig`/`GlobalConfig`.** So the portable 4-arg `QSettings(Format, Scope, org, app)` ctor **cannot** isolate to a temp path on this build. Instead use **`QSettings(const QString &fileName, Format format)`** with an explicit temp file path (e.g. `dir + "/mdit/test.ini"`, `QSettings::IniFormat`). That ctor IS present and is the reliable isolation seam.
  - **`QVERIFY`/`QFAIL` inside a non-void helper is a hard compile error** here: the macro expands to a bare `return;` (no value) → "return-statement with no value, in function returning X". Keep `QVERIFY`/`QCOMPARE` in **void slots only**; a `makeTempDir()` helper that returns `QTemporaryDir` must not contain one (just `return QTemporaryDir();`).
  - **`QCOMPARE(x, QStringList{a, b, c})` fails** with "macro 'QCOMPARE' passed 4 arguments, but takes just 2" — the initializer-list commas count as top-level macro args. Pre-store the expected list in a `const QStringList` variable and pass the variable.
  - **`QFile::readAll()` does not auto-open** — `QFile f(path); f.open(QIODevice::ReadOnly); f.readAll();`, else you get a `device not open` warning + empty string.
- No spec.md change needed (the subtask line was unambiguous). No new subtasks surfaced — the next unchecked line is section 7, item 2 (`Theme` + toggle, persisted via `Settings`).

### 6.2 Replace Ctrl+H (done this iteration)
- **The replace half is a foldable second ROW in the existing `FindBar`** — one bar, shared search state (query field, case toggle, count label). `Ctrl+H` / `Edit → Replace` → `MainWindow::onReplace()` → `m_findBar->activateReplace()` (`activate()` = show + focus + find-next-if-query, then unfold the row). Row widgets: `QLineEdit` (`ReplaceQuery`, placeholder "Replace with", min-width 200), `QPushButton` Replace (`ReplaceOne`), Replace All (`ReplaceAll`). Layout switched from `QHBoxLayout` to `QGridLayout` (row 0 = search, row 1 = replace); the fixed 36px height was dropped in favor of `updateBarGeometry()` = `move(0,0)` + `adjustSize()` so the height follows the number of visible rows.
- **`replaceCurrent()` semantics:** "current match" = the editor's selection compared with the bar's case-sensitivity (`selectedText().compare(q, cs) == 0`); if no such selection, `find()` first (returns 0 on total miss). Replacement = `QTextCursor::insertText(replaceText)` at the cursor WITH a selection (replaces it; empty replace text = delete). Then `findNext()` advances (with the existing wrap) so consecutive Replace clicks hit consecutive matches, and returns 1. **`replaceAll()`** = `m_editor->replaceAll(q, replaceText, caseSensitivity())` (the single EditorPane impl) + `updateMatchLabel()`; returns the count.
- **Gotcha caught by tests: the constructor must hide the replace-row widgets EXPLICITLY.** The `setReplaceRowVisible(false)` guard (`if (m_replaceRowVisible == on) return;`) makes the first call a no-op because the member starts `false` while the freshly created widgets are still visible — so the ctor calls `m_replaceEdit/Btn/AllBtn->hide()` directly.
- **Test gotchas (both were my own wrong expectations, implementation was right):** (1) case-insensitive replace-all of `"cat"` in `"cat cat CAT cat"` replaces **4**, not 3 — count ALL occurrences when writing expected values; (2) `QTextCursor::clearSelection()` leaves the cursor at a position that makes `find()` continue past your intended spot — instead, park the bare cursor explicitly (`c.setPosition(n)`) at the spot the scenario needs, and derive expectations from "find NEXT from here".
- **Enter in the replace field = Replace** (`returnPressed` → `replaceCurrent`), mirroring the search field's Enter = Next. `MainWindow` needs no new wiring: both replace paths mutate the editor, so the existing `textChanged` → model-dirty/title + `refreshFindBarCount` connections keep everything consistent. `activateReplace()` focuses the replace field only when a query already exists, else the query field.
- **Tests (8 new slots in `test_editor`):** row hidden-by-default / shown by `activateReplace` (checks both the `replaceRowVisible()` flag AND the actual `isHidden()` of `ReplaceQuery`/`ReplaceOne` children), `replaceAll` result + count (ci: 4→`"dog dog dog dog"`; cs: 1 of `cat CAT Cat`), label refresh after replace-all (→ `"no matches"`, selection cleared; also the `aa`→`a` shrink case), replace-current advance-then-stop (2nd Replace lands on the remaining match, 3rd is a 0 no-op), no-selection → finds from cursor, no-match no-op, empty-query no-op.
- No spec change (spec §8: "replace/replace-all" on the find bar with the standard behavior — implemented as-is). No new subtasks surfaced.

### 6.1 Find bar Ctrl+F (done this iteration)
- **New `src/FindBar.{h,cpp}` module** (auto-globbed into `mdit_core`) — the interactive chrome around `EditorPane`'s existing search helpers. `FindBar(EditorPane *editor, QWidget *parent)` builds a compact `QHBoxLayout`: `QLineEdit` (objectName `FindQuery`, clear-button, min-width 200) + `QPushButton` Next/Previous (`FindNext`/`FindPrevious`) + `QCheckBox` "Match case" (`FindMatchCase`) + `QPushButton` Close (`FindClose`) + `QLabel` count (`FindCount`, stretch 1 so it absorbs the spare width). `setFixedHeight(36)`, `hide()` at the end (first Ctrl+F shows it). Test seams: `setQuery(QString)` (= `lineEdit->setText`, fires `textChanged`→`onQueryChanged`), `setCaseSensitive(bool)` (= checkbox `setChecked`, fires `toggled`), `matchLabel()` (the label text), plus `matchCount()`/`currentQuery()`/`caseSensitivity()`.
- **The bar NEVER re-implements search** — it calls `EditorPane::find()/findPrevious()/matchCount()`. **Highlight = the editor selection** (`EditorPane::find()` already selects + `centerCursor()` the hit; `QPlainTextEdit` paints its selection), so the bar just reports the index. **Wrap** is the bar's job (Qt's `Qt::MatchWrap` is a *dialog* flag, not honored by `QTextDocument::find`): `findNext()` = if `find()` misses from the current selection-end, move the cursor to `Start` and `find()` again; `findPrevious()` = if it misses, move to `End` and `findPrevious()` again. A no-match-all query ends with the selection cleared (`EditorPane::find` clears it on a miss) and the label `"no matches"`.
- **`"i of n"` label math** (`updateMatchLabel()`): empty query → `""`; `matchCount()==0` → `"no matches"`; else if the editor has a selection, count how many query-occurrences start strictly before `selectionStart()` (a non-overlapping `QString::indexOf` loop) +1 → `"%1 of %2"`; else `"%1 matches"`. The 1-based index never overflows past `n` because wrap re-selects a real occurrence.
- **Wiring in `MainWindow`:** `m_findBar = new FindBar(m_editor, this)` (parented to the **top-level window**, not the splitter, so it floats over the editor region; `buildUi()` runs it right after `setCentralWidget`). `onFind()` (already bound to `QKeySequence::Find` = Ctrl+F in the Edit menu from the 0.2 shell — subtask only replaced the placeholder slot) → `showFindBar()` → `m_findBar->activate()` (`show`+`raise`+focus field+`selectAll`, and if a query is present a `findNext()`). A **second** `connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::refreshFindBarCount)` (in ADDITION to the existing `onEditorTextChanged` one) keeps the count current while the user edits with the bar open — `refreshCount()` is count-only, never moves the selection or steals focus.
- **Test (6 new slots in `test_editor`, the subtask line's "`test_editor` match count"):** `findBar_matchCount_countsOccurrences` (3/1/0/empty), `findBar_typingQueryHighlightsAndCounts` (typing selects the match + `"1 of 2"`→`"2 of 2"`), `findBar_nextPrev_wrapAround` (2 occurrences: next→next→wrap to first, prev→wrap to last), `findBar_caseSensitiveToggle_changesCount` (3 ci → 1 cs + `"1 of 1"`), `findBar_noMatch_clearsSelectionAndLabel` (`"no matches"` + selection cleared), `findBar_currentMatchIndexLabel` (`"1 of 3"`→`2`→`3`→wrap `1`). **Determinism:** each slot that asserts the initial match index resets the editor cursor to `Start` first (the AGENTS cursor-reset gotcha — `setPlainText` may leave the cursor mid/after the buffer).
- **`Qt::MatchFlags`/`FindFlags` confirmed on this build:** `Qt::MatchCaseSensitive = 16`, `Qt::MatchRegularExpression = 4`; `QTextDocument::FindCaseSensitively = 0x2`, `FindBackward = 0x1`, `FindWholeWords = 0x4` (defined in `QtGui/qtextdocument.h`, an unscoped `enum FindFlag` — `Q_DECLARE_FLAGS(FindFlags, FindFlag)`). `EditorPane::find(int flags)` takes the `Qt::MatchFlags` and maps only `MatchCaseSensitive`→`FindCaseSensitively`. A bare `int flags` param + `if (flags & Qt::MatchCaseSensitive)` is the safe idiom (avoid a `static_cast` from a 2-arg `QFlag` — this Qt 6.10 has no such operator).
- No spec change (spec §8 was unambiguous: `Ctrl+F` find, small docked/overlay bar, highlight + wrap + next/prev + replace — find half done here, replace is the next line). No new subtasks surfaced.

### 5 Save + Save As (done this iteration)
- `MainWindow::onSave()` — untitled documents (`m_doc.currentFilePath().isEmpty()`) route to `saveAsDocument()`; titled documents go through `saveDocument()`. `onSaveAs()` = `saveAsDocument()` directly. Menu actions already carried the spec keys (`QKeySequence::Save`, `QKeySequence::SaveAs` = Ctrl+S / Ctrl+Shift+S) from the 0.2 shell — subtask 5 only replaced the placeholder slots.
- **`askSaveAsPath(suggestedName)` virtual seam** — the Save As dialog (`QFileDialog::getSaveFileName` with the markdown filter, pre-filled with `suggestedSaveName()`) is isolated behind a virtual, exactly like `askDiscardChoice()`. `ScriptedSaveWindow` in `tests/test_save.cpp` records `lastSuggestedName` + increments `saveAsAsks` and returns `scriptedSavePath` (empty == cancel). **Any future file dialog (export destinations, subtask 9) should get the same virtual-seam treatment** so headless tests never touch a real modal.
- **`saveAsDocument()` side effects, in order:** dialog → sync editor→model → `m_doc.save(path)` (updates `currentFilePath`, clears dirty) → `updateTitle()` → **`updateLivePreview()`** (the base URL for relative images follows the file's directory, so a Save As that moves the file must re-render) → `showStatus("Saved <name>")`. Failures show `Could not save <name>` and leave model state untouched (no path change, still dirty).
- **`suggestedSaveName()`:** current file name when saved, else `m_doc.title() + ".md"` (untitled → `untitled.md`). Spec §6: "defaulting the suggested name/extension from the current document".
- **Title behavior intentionally NOT changed:** `titleFor()` stays "filename + `*` when dirty, bare `mdit` when untitled" — `test_smoke` asserts the bare `mdit` for untitled. The spec's "title reflects untitled" is satisfied loosely (the Document model's `title()` returns the untitled name); if a future run changes this, update `test_smoke::openMissingPathIsUntitled` and `newDocumentClearsPanes` too.
- **New test `test_save` (widget, offscreen)** covers: spec shortcuts present + enabled on the File menu actions (found by matching `QAction::shortcut()`), save-through-document (content on disk, CRLF convention restored, dirty cleared, `*` drops, `Saved <name>` status), untitled→Save As routing (dialog asked exactly once, then a plain Save no longer re-opens the dialog), cancel leaves state byte-for-byte untouched, bad path (`/no/such/dir/...`) fails with the error status, and the suggested-name prefill. Document-level save-as path/dirty was already in `test_document` — the subtask line's "`test_document` save-as path/dirty clearing" was satisfied by existing coverage; the new window-level suite is `test_save`.
- **Two more stripped-Qt discoveries (see Gotchas):** `QFileDialog::toNativeSeparators` does NOT exist (pass the plain name), and `QStatusBar` is only forward-declared transitively — a test that calls `statusBar()->currentMessage()` needs a direct `#include <QStatusBar>`.
- No spec change needed (spec §6 was unambiguous); no new subtasks surfaced — next unchecked line is section 6 (Find bar).

### 4.2 Wire live preview (done this iteration)
- **Debounce mechanism:** `MainWindow` owns a singleShot `QTimer m_previewTimer` (interval = `m_debounceMs`, default `PreviewDebouncer::defaultIntervalMs()` = 150 ms) + a `QElapsedTimer m_editClock`. `onEditorTextChanged()` (skipped under `m_updating`) → `schedulePreviewUpdate()`: **returns early when `!isPreviewVisible()`** (the spec: never render a hidden preview), else `m_editClock.restart()` + `m_previewTimer->start()` (restarted by every keystroke = classic debounce). On `QTimer::timeout` → `onPreviewTimerTimeout()` runs `PreviewDebouncer::evaluate(elapsed, interval)` and renders when `renderNeeded`. (With a restart-on-each-change singleShot, the timeout always fires ≥ interval later, so `renderNeeded` is always true — the policy is there for testability/faithfulness, the timer IS the debounce.)
- **`updateLivePreview()`** (public, testable) = the single render entry point: no-op if hidden; derives the **base URL** as `QUrl::fromLocalFile(QFileInfo(currentFilePath).absolutePath() + "/")` (empty `QUrl` when untitled — absolute `file://`/`data:` refs still work); `m_preview->setBaseUrl(base)` + `m_preview->setRendered(m_doc.text(), base, m_dark)`. `setPreviewDebounceMs(ms)` clamps to ≥0 and updates the live timer (tests shorten it so `QTest::qWait` stays snappy).
- **openFile() now renders immediately** (calls `updateLivePreview()` after the guarded `setPlainText`, replacing the old `setHtml("")` placeholder) — no debounce wait on load. **togglePreview() re-renders on re-show** (`if (nowVisible) updateLivePreview()`) so a hidden-then-shown preview isn't stale (typing didn't re-render it while hidden).
- **Scroll sync:** `EditorPane` gained `scrollRatio()`/`setScrollRatio()` mirroring `PreviewPane` (same `(value-min)/(max-min)` math, clamp, pin-to-top on no overflow). `MainWindow` connects `editor->verticalScrollBar()->valueChanged → onEditorScrolled()` and `preview->scrollRatioChanged → onPreviewScrolled(ratio)`. Each sets the OTHER pane's ratio to its own, wrapped in `m_syncingScroll` (set true → drive → false). **Why the guard works synchronously:** `QScrollBar::setValue` emits `valueChanged` synchronously, so driving preview→(its own `scrollRatioChanged`)→`onPreviewScrolled` happens *while `m_syncingScroll` is still true* → the echo is ignored. No recursion, no loop.
- **`QElapsedTimer` is a by-value member** → it needs a **complete type** in `MainWindow.h` (a forward declaration + by-value field = "incomplete type" compile error). `#include <QElapsedTimer>` in the header; `QTimer` stays forward-declared (it's a pointer member). Remember this for any by-value `QElapsedTimer`/`QRandomGenerator`-style member.
- **Test (test_livepreview, widget test):** uses `w.show()` OFFSCREEN to give the panes real geometry — needed so the vertical scrollbars get a real range for the scroll-sync + guard tests (an unshown QPlainTextEdit has no reliable scroll range). For the preview, still force `displayedQTextDocument()->setTextWidth(300)` + `resize(320,200)` (the test_previewpane trick) so its range is well-defined. **Feedback-guard test:** wrap the editor's `verticalScrollBar()` in a `QSignalSpy(&QScrollBar::valueChanged)`, call `editorPane()->setScrollRatio(0.6)`, assert the spy fires **exactly once** (a broken guard would echo preview→editor and emit a 2nd time) and that `previewPane()->scrollRatio()` ≈ 0.6.
- **No spec change** (the subtask line was unambiguous: debounce, render-only-when-visible, base URL from path, best-effort guarded scroll sync). No new subtasks surfaced — the next unchecked line is section 5 (Save / Save As).

### 4.1 MarkdownModel (done this iteration)
- New `src/MarkdownModel.{h,cpp}` (auto-globbed into `mdit_core`) + `tests/test_markdownmodel.cpp` (pure logic → `QTEST_GUILESS_MAIN`, no QApplication) via `add_mdit_test(test_markdownmodel)`. Both classes the subtask names live in this one module per the spec's architecture line:
  - **`MarkdownModel::headings(text) → QVector<MarkdownHeading{level,text}>`**: ATX only (no setext, no blockquote headings — documented in the header). Rules: `^ {0,3}(#{1,6})[ \t]+(.*)$` (≤3 leading spaces = CommonMark; 4+ = indented code, excluded; `#noSpace` and `#######` excluded); optional closing sequence (`space, #+, only trailing whitespace`) stripped, text trimmed; **empty headings skipped** (`#`, `#   ` — they carry no outline text); inline markdown kept verbatim (`## Hello **x**` → `Hello **x**`).
  - **Fence tracking:** toggled by `^ {0,3}(`{3,}|~{3,}).*$` — **CommonMark tildes included** (my first test used a `~~~` fence and the backtick-only regex inherited from the highlighter failed it). **Known cosmetic gap (left as-is, out of this subtask's scope):** `MarkdownHighlighter` still tracks **backtick fences only**, so `~~~`-fenced code is not visually code-highlighted even though the renderer and the heading extractor treat it as code. If a future run touches the highlighter, extend its fence regex the same way (and its tests).
  - **`MarkdownModel::wordCount/charCount` are now the CANONICAL counts** — `EditorPane::wordCount()/charCount()` were changed to delegate to them (identical behavior, `test_editor` unchanged and green). Never re-implement a count loop in a widget: call the model.
  - **`PreviewDebouncer`** — pure policy, no timer inside (GUI-free is the point): `static Decision evaluate(int elapsedMs, int intervalMs)` → `{renderNeeded, waitMs}`; negative inputs clamped to 0; interval 0 = "render on every change". Instance form carries an `intervalMs` (default `defaultIntervalMs()` = **150 ms**, middle of the spec's ~120–200 ms window) for the convenience `evaluate(elapsedMs)`. **Seam for 4.2:** MainWindow should own a `QElapsedTimer` restarted on each `editorTextChanged()` plus a single `QTimer` (singleShot-style) whose timeout reads the elapsed time through this policy and renders — only if the preview is visible.
- No spec.md change needed (the subtask line was unambiguous); no new subtasks surfaced (4.2 "wire live preview" is already the next unchecked line).

### 3.2 EditorPane (done this iteration)
- `src/EditorPane.{h,cpp}` is now a real `QPlainTextEdit` subclass (was a skeleton). New `tests/test_editor.cpp` (WIDGET test, custom `QApplication` main) via `add_mdit_test(test_editor)`.
- **Line-number margin = a nested friend `LineNumberArea : QWidget`** defined *inside* `EditorPane` in the header (Qt "Code Editor" pattern). It has no signals/slots → no `Q_OBJECT` → no own moc; `sizeHint()` and `paintEvent()` just delegate back to the owning `EditorPane`. `friend class LineNumberArea;` is declared inside `EditorPane` so it can call the protected `lineNumberAreaPaintEvent()`. Keep it a nested friend — an anonymous-namespace class in the .cpp couldn't be friended.
- **Repaint triggers wired in the ctor:** `QPlainTextEdit::blockCountChanged(int)` → `updateLineNumberAreaWidth`; `verticalScrollBar()->valueChanged(int)` → `updateLineNumberArea(QRect(),0)`; `QTextDocument::contentsChanged()` → repaint; `QTextDocument::documentLayoutChanged()` → width+repaint (this is the **font-change** hook — a font swap changes the layout, and there is NO `QFontChangeEvent`/`fontChange()` virtual on this Qt build, so this is the only font hook that exists); `resizeEvent` → reposition the margin; `QPlainTextEdit::textChanged()` → emit the concrete `editorTextChanged()` passthrough.
- **Gutter geometry without `blockBoundingGeometry` (missing here):** the editor is `NoWrap`, so block `N` sits at `y = N * QFontMetricsF(font()).lineSpacing()`. Paint each number at `top = blockNumber()*lineH - verticalScrollBar()->value()`, iterating `document()->firstBlock()`→`next()`, with an **early break** once `top > clip.bottom()+lineH` (blocks are ordered, so no later one is visible). This matches the reference example's iterate-from-top cost; fine for v1. `lineNumberAreaWidth()` = `3 + digits*horizontalAdvance('9')`.
- **Testability seams (GUI-free):** `lineNumbers()` returns the exact `"1".."N"` strings (== block count) and `lineNumberAreaWidth()` returns the digit-scaled width, so `test_editor` asserts the block count + sizing without driving a paint event. `lineNumberAreaWidget()` returns the real `LineNumberArea*` (a child of the pane).
- **`firstBlock()` is on the DOCUMENT, not the editor:** it's `document()->firstBlock()` (a bare `firstBlock()` in a `QPlainTextEdit` subclass does not compile).
- **Find/replace helpers are the SUBSTANTIVE half of subtask 6** — the Find bar UI (subtask 6) should call these, not re-implement search: `find(text, Qt::MatchFlags)` / `findPrevious(...)` (search from the selection end/start, select the hit, `centerCursor()`, **deselect via `clearSelection()` on a miss**), `matchCount(text, cs)` (manual non-overlapping `QString::indexOf` loop — cheaper and exact than repeated `document()->find`), `replaceAll(text, with, cs)` (counts then `setPlainText(toPlainText().replace(...))`, cursor to start). Default `cs` is `Qt::CaseInsensitive`.
- **`Qt::MatchFlags` on this build:** `MatchCaseSensitive=16`, `MatchRegularExpression=4`, `MatchWrap=32` — **NO `Qt::MatchRegex` (use `Qt::MatchRegularExpression`), NO `Qt::MatchWholeWords`.** `QTextDocument::FindFlag` = `FindBackward=1, FindCaseSensitively=2, FindWholeWords=4` — **NO `FindByRegularExpression`** (regex is engaged simply by passing a `QRegularExpression` to `find()`).
- **`QRegularExpression::fromWildcard(pattern, cs, ...)`** — 2nd arg is **`Qt::CaseSensitivity`** (NOT the `QChar escape` of stock Qt); pass `Qt::CaseSensitive`/`CaseInsensitive` from the flags.
- **`QTextDocumentFragment` is an INCOMPLETE type here** → `cursor.selection().isEmpty()` will not compile. Use `QTextCursor::hasSelection()` / `selectedText()` instead.
- **Cursor reset gotcha (hit a real bug):** a default-constructed `QTextCursor c;` is null and `c.movePosition(...)` on it is a no-op, so `setTextCursor(c)` does NOT move the cursor — after `setPlainText()` the cursor may sit at the buffer end, making a forward `find()` start past the last match and miss. Always reset via `QTextCursor c = editor.textCursor(); c.movePosition(QTextCursor::Start); editor.setTextCursor(c);` (use the pane's own cursor).
- No new subtasks surfaced — the Find bar / Replace UI are already listed (section 6) and will consume these helpers.

### 3.1 MarkdownHighlighter (done this iteration)
- New `src/MarkdownHighlighter.{h,cpp}` + `tests/test_highlighter.cpp` (`add_mdit_test(test_highlighter)`). All required rule families: headings h1–h6 (bold, distinct per-level theme color, muted hashes), strong `**…**`/`__…__`, emphasis `*…*`/`_…_`, inline code `` `…` ``, fenced code (``` with per-line state), links+images (underline), blockquotes, HR (`---`/`***`/`___`), list markers (`-`/`*`/`+`/`1.`). Theme-aware map built in `buildRules()` from the `m_dark` flag; `setDark(bool)` rebuilds + `rehighlight()` — that's the seam the subtask-7 theme toggle will call.
- **MAJOR environment discovery: this Qt build's `QTextDocument` has NO per-character format storage.** `QSyntaxHighlighter::highlightBlock()` IS called on doc changes (signal wiring works) and `setFormat()` executes without error, but the result is silently discarded: `QTextCursor::charFormat()`, `document()->toHtml()`, even the highlighter's own protected `format(pos)` all report the default format (verified with three standalone probes). Also `QTextCursor::mergeCharFormat()` only mutates the cursor's local copy. **Consequence: any char-level formatting is untestable through the document on this machine.**
- **Pattern used (keep it for any future highlighting/formatting work):** make the rule engine a pure, queryable function and let `highlightBlock()` be a thin applier. `MarkdownHighlighter::formatsForLine(line, inFence) const → LineFormats{ QList<RangeFormat{start,count,QTextCharFormat}> ranges; bool inFenceAfter; }`. `highlightBlock()` = call it with `previousBlockState()==1`, `setFormat()` each range in order (later ranges override earlier overlaps), `setCurrentBlockState(res.inFenceAfter)`. Tests assert on `formatsForLine()` — deterministic, GUI-less (`QTEST_GUILESS_MAIN`, not even `QTextDocument` needed for most slots).
- **Stripped `QSyntaxHighlighter` API names:** `setPreviousBlockState()` does NOT exist here — it is **`setCurrentBlockState(int)`** (same semantics). `previousBlockState()` exists.
- **Stripped `QTextCharFormat` API:** no `setTextDecoration()`/`QTextCharFormat::Underline` enum — use `setFontUnderline(bool)` (maps to `TextUnderlineStyle`) and read `fontUnderline()`. `fontWeight()` returns `int` and `QFont::Weight` is the classic int enum (Bold=700, Normal=400) — no `QFont::Weight` class.
- **`QList::push_back` is gone** (Qt 6.10 new-container QList): use `append()`. Brace-init doesn't bind to `push_back`'s parameter types — with `append(Rule{...})` the explicit type name is required anyway.
- **AUTOMOC missed the brand-new header** on the incremental build (compiler-scan optimization marked the new .cpp "unscanned"), leaving `vtable for MarkdownHighlighter` undefined at link even after re-configure. **Fix that actually worked: `rm -rf build/mdit_core_autogen` then build** (forces the moc scan + regenerates `mocs_compilation.cpp`). Remember this if a future module's vtable/`staticMetaObject` is missing from `libmdit_core.a`.
- **`QTextBlock` is only forward-declared in this Qt** (no qtextblock.h, no complete type) — you cannot call any method on a `QTextBlock`; don't write code that does (`findBlock()` etc. unusable).
- **Default `QTextCharFormat` quirk:** `foreground().color()` is a VALID black (not invalid) on this build, so "unformatted" cannot be detected via color validity. Test with `format == QTextCharFormat()` (QTextFormat has `operator==`) or via specific attributes.
- Regex notes: emphasis `*` rule uses look-arounds `(?<!\*)\*(?!\*)[^*]+\*(?!\*)` so it never matches inside `**strong**`; underscore rule is `(?<![_\w])_(?!_)[^_]+_(?!\w)` (the lookahead after the opening `_` must be `(?!_)`, NOT `(?!\w)` — that breaks `_word_`). HR regex `^(?:(?:\s*-){3,}|\s*\*{3,}|\s*_{3,})\s*$`; list marker `^\s*(?:[-*+]|\d+\.\s)` formats only the marker. Headings require `#\s` (CommonMark) — `#noSpace` intentionally unformatted.
- No new unchecked subtasks surfaced; the remaining section-3 line (`EditorPane` line-number margin + counts + hosting this highlighter) is the next run.

### 2.2 PreviewPane widget (done this iteration)
- `src/PreviewPane.{h,cpp}` is now a real widget, not a skeleton: a read-only `QTextBrowser` hosting the rendered `QTextDocument`. API per the subtask line:
  - `setRendered(md, baseUrl, dark)` — calls `RenderedDocument::render(md, baseUrl, dark, *document())`, i.e. renders **into the browser's own document** (never a by-value `QTextDocument`). This is what makes "what you preview is what you export" true: export (subtask 9) reads `displayedQTextDocument()` = `QTextBrowser::document()`.
  - `displayedQTextDocument()` returns `document()` (the live, non-null browser doc) for export.
  - `setBaseUrl(url)` / `baseUrl()` — set/get `document()->baseUrl()`; points at the markdown file's directory so relative images resolve.
  - `scrollRatio()` → `(value-min)/(max-min)` in [0,1], 0 when no overflow; `setScrollRatio(ratio)` clamps to [0,1] and sets `value = min + round(ratio*range)`, pins to top when range≤0. A `scrollRatioChanged(double)` signal is emitted on `verticalScrollBar()->valueChanged` — **this is the seam subtask 4 hooks editor↔preview sync to**; don't re-invent it.
- **Build gotcha (Qt enum scope):** `QTextBrowser` inherits `QTextEdit`, and the line-wrap enum is `QTextEdit::WidgetWidth` — **NOT** `QPlainTextEdit::WidgetWidth` (that class isn't even in scope in `PreviewPane.cpp`; it only builds if you `#include <QPlainTextEdit>`). Use `QTextEdit::WidgetWidth` for the wrap mode.
- **`test_previewpane` is a WIDGET test** → custom `main` builds a `QApplication` (not `QTEST_MAIN`/`QGuiApplication`, since it's a QWidget). Registered via `add_mdit_test(test_previewpane)` (offscreen env).
- **Testing scroll offscreen (reusable pattern):** an unshown widget has a 0×0 viewport, so the scrollbar range is 0 and `scrollRatio()` is trivially 0. To make the scroll API testable, (a) `pane.resize(400,200)`, (b) render ~200 wrapped paragraphs, (c) **`pane.displayedQTextDocument()->setTextWidth(380)`** to force a definite layout width so the document height (and thus `verticalScrollBar()->maximum()`) becomes well-defined, then assert `maximum()>minimum()` before exercising top/middle/bottom round-trips and clamping. The `scrollRatioChanged` test uses `QSignalSpy` on the signal, then `sb->setValue(...)` and checks `spy.last().at(0).toDouble()` ≈ `scrollRatio()`. **`QCOMPARE` takes exactly 2 args — for a float tolerance use `QVERIFY(qAbs(a-b) < eps)`.**
- No spec change needed — the PreviewPane line was unambiguous and implemented exactly as written. `MainWindow` still calls `m_preview->setHtml(QString())` on open (left as-is); subtask 4 (live preview wiring) is where `setRendered()` gets driven from `textChanged` with the debounce + `setBaseUrl(currentFilePath's dir)`.

### 2.1 RenderedDocument pure renderer (done this iteration)
- New `src/RenderedDocument.{h,cpp}` (auto-globbed into `mdit_core`) + `tests/test_render.cpp` registered via `add_mdit_test(test_render)`. The module is GUI-free; `render()` renders markdown into a `QTextDocument` using `QTextDocument::setMarkdown(md, features())`, sets the base URL **before** `setMarkdown` (so relative images resolve), and applies a per-document light/dark stylesheet via `setDefaultStyleSheet` (a per-instance Q_PROPERTY on this build — verified two docs can hold different sheets).
- **API shape deviation (important, recorded in spec.md):** the implementation.md line says `static QTextDocument render(...)` **by value**, but that is **not compilable** on this toolchain. `QTextDocument` is `Q_DISABLE_COPY` with **no move ctor**, and **GCC 15 hard-errors** on `return <named local QTextDocument>;` (NRVO is not guaranteed for named locals, so the fallback copy is required and is deleted) — confirmed at both `-O0` and `-O2`. The only thing that compiles by value is a *plain prvalue* `return QTextDocument();`, which can't be configured. So `render()` is `static void render(const QString& md, const QUrl& baseUrl, bool dark, QTextDocument &out)` — fills `out` in place; caller owns lifetime. **Rule for all later modules: NEVER return a `QTextDocument` by value; fill an out-parameter (or take/return a pointer).** The preview (subtask 2.2) should render into its own `QTextBrowser::document()`; export (subtask 9) renders into a local doc.
- **"Full feature flag set" on this Qt (6.10.2):** `QTextDocument::MarkdownFeature` exposes ONLY `MarkdownNoHTML`, `MarkdownDialectCommonMark` (=0), and `MarkdownDialectGitHub` (a bitset enabling tables/autolink/strikethrough/task-list/etc.). There are **no per-feature enums** to OR together (unlike newer Qt). `features()` returns `MarkdownDialectGitHub` = the maximum set. Don't go looking for `MarkdownTable`/`MarkdownAutolink` here — they don't exist on this build.
- **This is a STRIPPED/custom Qt build — the usual QTextDocument text-traversal APIs are missing.** There is **no `qtextblock.h`**, and `QTextBlock`/`QTextFragment` are **not iterable** as in stock Qt (`begin()`/`firstFragment()`/`nextFragment()`/`isNull()` don't exist as expected), `QTextDocument::resourceUrls()` is **absent**, and `QTextImageFormat` uses `name()` (QString), **not** `image()`/`imageUrl()` (QUrl). So to test rendering, do NOT walk blocks/fragments. Instead: assert on **`toHtml()` / `toPlainText()`**, and resolve images via **`doc.resource(QTextDocument::ImageResource, baseUrl.resolved(relPath)).value<QPixmap>()`** (a QGuiApplication is required for QPixmap — offscreen is fine). `toHtml()` on this build inlines styles rather than using semantic tags: bold=`font-weight:700`, emphasis=`font-style:italic`, inline code=`font-family:'monospace'`, fenced code=`<pre>`, blockquote=indented `<p>` (`margin-left:40px`, no `<blockquote>` tag), headings=`<h1>`..`<h6>`, tables=`<table>`, links=`<a href>`, task list=`<li class="checked">`/`<li class="unchecked">`. test_render asserts each with an `||` fallback to the stock-Qt tag form so it also passes on a stock Qt.
- **test_render is a GUI test** (custom `main` builds a `QGuiApplication`, not QCoreApplication) because of the QPixmap image-resolution check; the offscreen env from `add_mdit_test` keeps it headless. The other 10 slots are pure and would run under QTEST_GUILESS_MAIN, but one QGuiApplication covers all of them.

### 1.2 Wire Open/New through Document + dirty guard + DnD (done this iteration)
- `MainWindow` now owns a `Document m_doc` by value (exposed via `document()`). `openFile()` calls `confirmDiscard()` first, then `m_doc.load(path)`; on load it pushes `m_doc.text()` into the editor under an `m_updating` guard flag so the programmatic `setPlainText` does NOT re-mark dirty. `newDocument()` = guard + `newDocumentNoGuard()` (clears path, advances `nextUntitled()`, clears editor/preview, resets dirty). The old ad-hoc `m_currentPath`/manual `QFile` read in `MainWindow` is gone — the `Document` model is the single source of truth.
- **Dirty tracking:** `MainWindow` connects `m_editor` `QPlainTextEdit::textChanged` → `onEditorTextChanged()`, which (when not `m_updating`) does `m_doc.setText(editor->toPlainText())` + `updateTitle()`. `Document::setText` marks dirty only when the text actually changed, and `updateTitle()` now renders `titleFor(m_doc.dirty())` so the title carries `*` live. The `m_updating` flag is the key to breaking the "programmatic set → textChanged → re-mark dirty" loop.
- **`confirmDiscard()` design (testable + headless-safe):** returns true if `!m_doc.dirty()`; otherwise calls **virtual** `askDiscardChoice()` which by default shows a real modal `QMessageBox` (Save/Discard/Cancel). The Save branch calls `saveDocument()` (syncs editor→doc, `m_doc.save()`; returns false for untitled since Save As is subtask 5) and only proceeds if the save succeeded. Tests override `askDiscardChoice()` in a `ScriptedWindow` subclass to script Save/Discard/Cancel — no modal ever blocks the offscreen run. **This virtual-seam pattern is the reusable way to make any "ask the user" decision testable headlessly.**
- **DnD:** `setAcceptDrops(true)` in the ctor; `dragEnterEvent` accepts `hasUrls()`; `dropEvent` collects local-file URLs and delegates to public `handleDroppedPaths(QStringList)`, which opens the FIRST local `.md`/`.markdown` path (suffix match, case-insensitive; `.markdown` included) and ignores everything else (single-file focus). Tests drive `handleDroppedPaths()` directly (synthesizing a `QDropEvent` offscreen is fiddly).
- **Test:** new `tests/test_opennew.cpp` (widget test, custom `QApplication` main) via `add_mdit_test(test_opennew)`. Covers: open-through-document (path recorded, dirty cleared, CRLF normalized, title), typing marks dirty + `*` title, guard passes when clean (no ask), guard honors Discard/Cancel, a Cancel'd guard blocks BOTH open and new (state unchanged), and drop opens only `.md`/`.markdown` (first valid wins).
- **`test_smoke::newDocumentClearsPanes` had to change:** previously it did `editorPane()->setPlainText(...)` then `newDocument()`. Now typing marks the doc dirty, so `newDocument()` would hit the guard → a real modal `QMessageBox::exec()` would HANG the offscreen test. Fixed to load a real (clean) file first, then `newDocument()` (clean → no prompt). **Gotcha: any test that puts text in the editor and then calls a guarded op (New/Open/close) must either keep the doc clean or use a scripted window — otherwise the modal hangs the suite.**
- **MOC gotcha (new):** a `private slots:` section placed AFTER an existing `public slots:` section in the same class made `moc` fail with "Not a signal or slot declaration" at the `private slots:` line. Fix: put the extra slot into the existing `public slots:` block instead of opening a second slot access section. (Also: a member used by a declaration must be declared above it — `enum class DiscardChoice` had to sit above `askDiscardChoice()`'s declaration in the class.)

### 1.1 Document core (done this iteration)
- New `src/Document.{h,cpp}` — the first "module" file. It is auto-globbed into `mdit_core` (no CMake edit for the lib) but the `add_mdit_test(test_document)` line was added by hand to `tests/CMakeLists.txt`. `test_document` is a **pure-logic** test → `QTEST_GUILESS_MAIN` (no QApplication needed), registered like every other test.
- **Qt6 gotchas hit (baked into Document.cpp, remember for later modules):**
  - `QTextCodec` is **NOT** in the default Qt6 install (it moved to a separate `Qt6::TextCodec` module we don't link). Do NOT `#include <QTextCodec>` or call `QTextStream::setEncoding(Qt::UTF8)` / `setCodec(...)`. **`QTextStream` defaults to UTF-8 in Qt6** — just read/write with a plain `QTextStream`.
  - `QString::replace` has **no `(QString, QChar)` and no `QLatin1String`+`QChar` mixed overloads**. The usable ones are `(QChar, QChar)`, `(const QString&, const QString&)`, `(QChar, const QString&)`. Use `QString` for multi-char needles/replacements, e.g. `text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"))`.
  - Open files **without `QIODevice::Text`** so Qt does no platform line-ending translation; `Document` does its own CRLF→LF normalize on load (and re-applies the remembered ending on save). This makes the CRLF round-trip test exact.
- **Design choice — line-ending memory:** `load()` sniffs the raw bytes for a `\r\n` run and stores `m_lineEnding` (`"\n"` default, `"\r\n"` if detected). `save()` re-expands LF→CRLF only when that was the source convention. In-memory text is always LF. This satisfies "keep detected line ending, but by default `\n`".
- **`save(path)` semantics:** empty `path` → save to `currentFilePath()` (fails if that is empty). A non-empty `path` (Save As) updates `currentFilePath` and clears dirty on success. `load()` of a missing path returns false and leaves prior state untouched (test asserts this).
- **`title()`** = `QFileInfo(path).fileName()` when saved, else the untitled name. No `*` marker here — that's a title-bar concern (subtask 5). `nextUntitled()` returns "untitled" then "untitled 1", "untitled 2", ... and advances an internal counter.
- MainWindow still uses its own ad-hoc `openFile()` (pre-`Document`); subtask **1.2** wires Open/New through this model + a `confirmDiscard()` guard. Don't delete that ad-hoc path yet — 1.2 replaces it.

### 0.3 test_build.py pytest shim (done this iteration)
- `tests/test_build.py` already existed from 0.1 (three pytest functions: `test_configure` → `test_build` → `test_ctest`); this iteration **verified it end-to-end and negative-tested failure propagation** (`mv CMakeLists.txt aside` → `pytest -q` reports `test_configure FAILED` / `test_build FAILED`, exit code non-zero; restored → 3 passed). It is the harness entry point: `pytest -q` from the repo root collects exactly the 3 shim tests (no other python tests in the repo).
- The shim reuses the repo `build/` dir (no wipe) — configure is ~0.1s and ninja is incremental, so a full `pytest -q` run is <1s when nothing changed. It prefers Ninja, falls back to `Unix Makefiles` if ninja is missing. Do **not** add a clean-build step (would make every harness iteration pay the full Qt compile cost).
- Subtask 1.2 (next) will be the first new module (`Document`); remember: new `src/*.cpp` is auto-globbed into `mdit_core`, but the `add_mdit_test(test_document)` line must be added to `tests/CMakeLists.txt` by hand.



### 0.2 src/ skeleton + MainWindow (done this iteration)
- Created the window shell per spec: `MainWindow.{h,cpp}` (QMainWindow holding a `QSplitter` with `EditorPane` left + `PreviewPane` right at 50/50, a File/Edit/View/Help menu bar with the exact spec shortcuts, a toolbar, a status bar), plus minimal-but-real `EditorPane.{h,cpp}` (QPlainTextEdit), `PreviewPane.{h,cpp}` (QTextBrowser), and `Theme.{h,cpp}` (Fusion + light/dark palette). `main.cpp` now creates the QApplication, applies `Theme::apply(app, dark)`, shows the `MainWindow`, and opens an optional `argv[1]` path if it exists.
- **`mdit_core` static library (important refactor):** the top-level CMake now globs `src/*.cpp`, excludes `main.cpp`, and builds them into `add_library(mdit_core STATIC ...)` with `target_include_directories(... PUBLIC src)`. The `mdit` executable is just `main.cpp` + `target_link_libraries(mdit PRIVATE mdit_core)`. **`add_mdit_test()` in `tests/CMakeLists.txt` links `mdit_core`** so the CTest binaries exercise the real module code (that's how `test_smoke` constructs a `MainWindow`). Keep this pattern — every later module added to `src/` is auto-globbed into `mdit_core` and becomes testable with no extra link edit.
- **Skeleton-scope decision (deliberate, not a cut corner):** this subtask is the "shell". Menu actions whose *behavior* is a later subtask (Save/Save As = 5, Find/Replace = 6, Export = 9) are present as real `QAction`s with the **correct spec shortcuts/structure** but wired to `showStatus("... implemented in subtask N")` placeholder slots. The shell-correct behavior IS fully implemented now: `openFile()` (streaming read + `\r\n`→`\n` normalize + title update + returns bool), `newDocument()`, `togglePreview()`, `setDarkTheme()`. Subtasks 1/2/3/5/6/9 flesh these out in place — don't delete the action plumbing.
- **`isPreviewVisible()` uses `!m_preview->isHidden()`**, NOT `isVisible()`. Rationale: `QWidget::isVisible()` is false while the top-level window hasn't been shown (always the case in headless tests), which broke the "preview ON by default" check. `isHidden()` reports the explicit show/hide toggle state independent of window visibility — that's the right semantic for the preview-toggle feature. `togglePreview()` computes its next state from `isHidden()` the same way.
- **50/50 check is robust under offscreen:** `MainWindow` ctor calls `resize(1100,750)` + `splitter->setSizes({550,550})`; `splitterRatio()` guards to 0.5 when the size total is 0 (offscreen may not assign pixels until after `show()`), so `test_smoke::splitterIsFiftyFifty` asserts `|ratio-0.5| < 0.05` and can't flake.
- **Menu titles carry the `&` mnemonic** (`menuBar()->findChildren<QMenu*>()` → `QMenu::title()` returns `&File`). Strip with `m->title().remove('&')` before comparing in tests.
- **Theme default is light in `main.cpp`** for the skeleton. "Applies *persisted* Theme" is satisfied structurally (main calls `Theme::apply` with a resolved dark flag); the flag is a hardcoded `false` until subtask 7 wires `Settings` (auto/light/dark). Don't treat the light default as final.
- Tests: `test_smoke` (widget test — custom `main` builds a `QApplication`, since `QTEST_MAIN` is core-only) covers: constructs+layout (editor left / preview right, 2 children), 50/50, menu/toolbar/status bar + all 4 menus, preview default-ON + toggle, `openFile` load + CRLF normalization + title, missing-path → untitled, `newDocument` clears. `test_scaffold` kept.

### 0.1 Scaffold + CMake (done this iteration)
- Built the full build system + repo contract. `project(mdit VERSION 0.1.0 LANGUAGES CXX)`, C++20, Qt6 Widgets/Gui/PrintSupport, `enable_testing()`, `BUILD_TESTS` option (default ON), `qt_add_executable(mdit WIN32 MACOSX_BUNDLE ...)`, `target_include_directories(mdit PRIVATE src)`, `tests/` subdir linking `Qt6::Test`, and a complete `.gitignore`.
- **`src/main.cpp` is intentionally a MINIMAL buildable stub** (QApplication + `--version`). `qt_add_executable` requires ≥1 source, so a stub was needed to make the scaffold green. **Subtask 0.2 replaces/extends it** (theme wiring, MainWindow, optional `argv[1]` open). Don't treat the stub's behavior as final.
- **`tests/test_scaffold.cpp` is a minimal placeholder** proving the Qt6/QtTest toolchain configures/builds/runs offscreen. **Subtask 0.2 adds the real `test_smoke`.** test_scaffold may be kept or folded later; don't let its triviality imply the app is built.
- **GitHub-ready docs + harness created now (standing contract, not deferred):** `README.md`, `LICENSE` (MIT, `Copyright (c) 2026 Pat Wendorf`), `CHANGELOG.md` (Keep-a-Changelog, `## [0.1.0]`), `.gitignore`, `build.sh`, and `tests/test_build.py` (the pytest configure→build→ctest shim). Keep README feature list + CHANGELOG in sync as features land. `test_github_ready` (subtask 10) will assert these doc files — they already exist, so 10.2/10.3 mostly finalize wording + add the `test_github_ready` C++ test.
- **CUPS warning on configure is harmless** (`Could NOT find Cups`) — it's an optional PrintSupport dep; `QPdfWriter` still works.

## Environment (verified 2026-09-10, behoserver)
- cmake 4.2.3, ninja 1.13.2, g++ (GCC 15.2.0), Qt6 with Core/Gui/Widgets/PrintSupport/Test all present under `/usr/lib/x86_64-linux-gnu/cmake/Qt6*`. Python 3.14.4, pytest 9.1.1 (at `~/.local/bin/pytest`). No external markdown lib / QtWebEngine — pure Qt only.

## Gotchas
- **`absolute` (and likely `relative`) is a MACRO on this Qt build** — it is not a valid identifier: `const bool absolute = ...` fails to parse. Rename any such local (e.g. `isAbsolute`). Stripped-file-API notes: no static `QFile::size(path)` (use `QFileInfo(path).size()`).
- **Derived test windows must forward the ctor:** any `Scripted*Window : public MainWindow` that wants to be built with `(parent, QSettings*backing)` needs an explicit forwarding constructor — an implicit/derived ctor hides the base one.
- **Isolating `QSettings` in tests (this Qt build):** `QSettings::Scope` has ONLY `UserScope`/`SystemScope` — **no `UserConfig`/`GlobalConfig`**, so the 4-arg `QSettings(Format, Scope, org, app)` ctor can't point at a temp path. Use **`QSettings(fileName, QSettings::IniFormat)`** with an explicit temp file path (inside a `QTemporaryDir`) as the isolation seam. Also: `QVERIFY`/`QCOMPARE` are void-slot-only (a non-void helper with one is a hard "return-statement with no value" error), `QCOMPARE(x, QStringList{a,b,c})` mis-counts macro args (store the list in a variable first), and `QFile::readAll()` needs `open()` first. `Settings` wraps a backing `QSettings*` for exactly this — see the 7.1 notes.
- **Stripped `QFileDialog` / incomplete `QStatusBar`:** `QFileDialog::toNativeSeparators` does not exist on this Qt build — pass the file name straight to `getSaveFileName`. And `QStatusBar` is only forward-declared transitively (via `QMainWindow` headers): code that calls `statusBar()->currentMessage()` etc. needs a direct `#include <QStatusBar>`.
- **No char-format storage in QTextDocument (this Qt build).** `QSyntaxHighlighter::setFormat()` is an unobservable no-op; `QTextCursor::charFormat()`/`toHtml()` never reflect highlights. Expose formatting decisions as a pure function (see `MarkdownHighlighter::formatsForLine`) and test that. See the 3.1 notes.
- **Never return a `QTextDocument` by value.** It is `Q_DISABLE_COPY` and has no move ctor; GCC 15 hard-errors on returning a configured named local (non-guaranteed NRVO → deleted copy). Use an out-parameter (`render(..., QTextDocument &out)`) or a pointer. See the 2.1 notes.
- **This Qt is stripped:** no `QTextBlock`/`QTextFragment` iteration, no `resourceUrls()`, `QTextImageFormat::name()` instead of `image()`. Test rendering via `toHtml()`/`toPlainText()` and `doc.resource(...)`, not fragment walking.
- **Which QtTest macro:** non-widget (pure-logic) tests → `QTEST_GUILESS_MAIN` (no `QApplication`, lightest). Widget tests → you must create a `QApplication` yourself (custom `main`, or `QTEST_MAIN` does NOT suffice since it is core-only). The `QT_QPA_PLATFORM=offscreen` env set by `add_mdit_test` is what makes widget tests run without a display.
- The CMake `GLOB ... CONFIGURE_DEPENDS` only re-globs on **configure** (or first build when files change); a brand-new `src/*.cpp` added in the same session is fine because the build re-checks globbed dirs. If a newly added source is ever missed, re-run configure.
- **QTextDocument geometry/find API is half-present (this Qt build).** Present + working: `firstBlock()`, `lastBlock()`, `block.next()/blockNumber()/isVisible()/length()/isValid()`, `find(QString|QRegularExpression, from|cursor, FindFlags)`, signals `contentsChanged()`, `blockCountChanged(int)`, `documentLayoutChanged()`. **MISSING:** `block(int)`, `blockBoundingGeometry()`, `cursorForPosition()`, `documentSizeChanged()` (only `documentLayoutChanged()`), `isVerticalScrollBarPresent()`, `QFontChangeEvent`/`fontChange()`. For a NoWrap editor, compute a block's y as `blockNumber()*QFontMetricsF::lineSpacing()` instead of `blockBoundingGeometry`. `setViewportMargins`/`contentOffset` are protected (fine from within a `QPlainTextEdit` subclass). See the 3.2 notes.
- **`QTextDocumentFragment` is incomplete** → don't call `cursor.selection().isEmpty()`; use `QTextCursor::hasSelection()`/`selectedText()`. And `Qt::MatchRegex`/`Qt::MatchWholeWords` don't exist (use `Qt::MatchRegularExpression`; there is no whole-word match flag), `QTextDocument::FindByRegularExpression` doesn't exist (regex is triggered by passing a `QRegularExpression` to `find()`), and `QRegularExpression::fromWildcard` takes `Qt::CaseSensitivity` as its 2nd arg. See the 3.2 notes.

## Final polish (last iteration) — loop complete
- The single remaining unchecked subtask was "Final polish". Ran the FULL suite
  green: `rm -rf build && pytest -q` (the `tests/test_build.py` shim: configure →
  build → ctest) rebuilt from scratch and passed **all 18 CTest/QtTest binaries**
  in ~21 s. Confirmed the offscreen platform runs every widget test with no
  display.
- Wrote `RALPH_SUMMARY.md` — the standing "the loop is done" summary (what was
  built, per-feature checklist, test list, build/run commands, repo contract).
  This file is what a human / future run looks at to confirm the project is
  finished and GitHub-ready.
- Added a CHANGELOG **Unreleased** entry for the final polish (kept the
  `## [0.1.0]` heading and "Keep a Changelog" line intact, so `test_github_ready`
  still passes — it only asserts those two strings, not the Unreleased body).
- **No new subtasks surfaced.** Every feature in spec.md / implementation.md is
  implemented, tested, and green. The repository is committed.
- Gotcha worth remembering for a future repo: the `## [Unreleased]` body of the
  CHANGELOG is NOT guarded by any test, so it is safe to use as the "in-progress
  since last release" area; the guarded invariants are only the release heading
  and the "Keep a Changelog" reference.

## Post-loop polish (2026-09-11, handoff copy on beholaptop)
The loop finished `done`; the project was copied from the synced ralph deliverable
(`~/Personal/ralph/qt6-markdown-editor-mditor`) to `~/Personal/mdit` — this copy is
the canonical iteration point from here on. Work done in this phase:
- **Renamed the whole project `mditor` -> `mdit`** (150 occurrences / 31 files:
  directory, CMake `project()`, `mdit_core` lib, `add_mdit_test()` helper,
  CTest names, `MDIT_*` defines, binary `build/mdit`, README/CHANGELOG/LICENSE/
  spec/AGENTS/prompt/deps.yaml). Rebuilt + reran the suite green afterwards.
- **Undo / redo while editing** (`Ctrl+Z`, `Ctrl+Shift+Z`, `Ctrl+Y`): Edit >
  Undo / Redo actions owned by the **editor pane** with
  `Qt::WidgetWithChildrenShortcut` — that scopes the keys to the document editor
  so the find bar's fields keep their own native undo (a window-level action
  would have stolen `Ctrl+Z` from them). Enabled state follows
  `undoAvailable`/`redoAvailable`.
- **Gotcha found + fixed: `EditorPane::replaceAll()` used `setPlainText()`**,
  which clears the undo stack — so a Replace All could never be undone. It now
  rewrites through one `QTextCursor` edit block (`beginEditBlock()`/`endEditBlock()`),
  which makes the whole sweep a single undo step. Same for the "cursor at the
  document start" contract (kept).
- **The dirty flag is now undo-aware.** `EditorPane::markClean()`
  (`QTextDocument::setModified(false)`) is called after every programmatic load
  (open/new) and every successful save; `onEditorTextChanged()` then mirrors the
  document's modified flag back to `Document::setDirty(false)` when an undo lands
  exactly on the checkpoint. QTextDocument tracks this itself: undoing *past* a
  save re-marks it modified, so the dirty flag stays honest in both directions.
- **Qt facts verified by probe on this box (Qt 6.10.2, offscreen):** a bare
  `QPlainTextEdit` already handles `Ctrl+Z`/`Ctrl+Shift+Z` in its own
  `keyPressEvent` (so undo "worked" before this phase too — it was just
  undiscoverable, had no redo entry, and Replace All broke it). The syntax
  highlighter does **not** pollute the undo stack (a probe shows one undo step
  per `QTextCursor::insertText` edit). Consecutive `insertPlainText()` calls may
  be **merged** into one undo step by Qt, so tests that need a known step count
  must measure it (undo to empty, count, redo) rather than assume — that mistake
  cost a red test run in `test_undo`.
- Tests: `tests/test_undo.cpp` (12 cases) added to `tests/CMakeLists.txt` via
  `add_mdit_test(test_undo)` -> 19 CTest binaries, all green.

## Post-loop polish II (2026-09-11) — find popup, theme button, app icon
- **Find/Replace is a popup window now.** `FindBar` derives from `QDialog` (was a
  `QWidget` overlay glued to the window's top-left, which sat under the menu bar).
  It is owned by the main window and `centerOnParent()`s itself (host =
  `parentWidget()->window()`, falling back to the primary screen) on every
  `showEvent` and whenever it grows. The title bar names the mode.
- **Gotcha: `adjustSize()` will not SHRINK a visible top-level window.** The
  popup kept a tall default (a blank second row in find-only mode) until
  `updateBarGeometry()`/`showEvent()` were changed to `resize(sizeHint())`
  explicitly. Assertions on the popup's size (`test_findwindow`) caught it.
- **Focus after closing:** assert with `QWidget::focusWidget()` on the host
  window, not `QApplication::focusWidget()` — offscreen windows are never
  "active", so the app-level focus widget is null even though the hand-off to the
  editor worked.
- **Toolbar right-justification:** `tb->addWidget(spacer)` with a `QWidget` whose
  horizontal size policy is `Expanding`, before the action; the test asserts both
  that the action is last and that the action before it owns an Expanding widget.
- **Emoji buttons work here** (Noto Color Emoji is installed): `☀️` (U+2600 +
  VS16) and `🌙` (U+1F319) render in colour in a `QToolButton`. The toolbar twin
  of the `View → Theme` action deliberately has NO shortcut — `Ctrl+T` stays on
  the menu action so the pair can never be an ambiguous shortcut.
- **Icon:** `tools/make_icon.py` (Pillow, generation-time only) draws the mark —
  white disc, radius = 0.4872 * size to mirror `~/Personal/Pengy/pengy.png`
  (measured: disc radius 573/1176, colour #151515 for the penguin black) — with a
  `#` made of four rounded bars. Per-size drawing, not scaling: the 16 px size is
  pixel-snapped with square ends (sub-pixel antialiased bars turn to grey mush
  at that scale), 24-32 px use a lighter stroke with wider counters, ≥ 48 px the
  true weight. 16/24/32/48/64/128/256/512 PNGs + `assets/mdit.svg` are committed.
- **Static-library qrc gotcha:** a `.qrc` compiled into a STATIC lib
  (`mdit_core`) is registered by an object file nothing references, so the linker
  drops it and `:/icons/...` silently stops resolving. Fixed with a file-scope
  `Q_INIT_RESOURCE(mdit)` helper in `AppIcons.cpp` (the macro cannot live inside a
  namespace). `test_icon` fails loudly if the resource is missing.
- **Resampling gotcha:** LANCZOS downscaling rings — it left a faint halo 1-2 px
  OUTSIDE the disc edge (alpha 2/255), which both looked wrong on a dark desktop
  and failed a strict transparency assertion. `Image.reduce()` (box filter) with
  8x supersampling is ring-free; the committed PNGs are bit-identical in shape
  but with a clean edge.
- Tests: `test_findwindow` (6 cases) + `test_icon` (6 cases) added, `test_theme`
  gained 2 → **21 CTest binaries**, all green.

## Post-loop polish III (2026-09-11) — one chrome row + the dock icon
- **The toolbar is gone.** It duplicated the menus (New/Open/Save/Preview) in a
  second row under the menu bar, which read as "two layers of menus". The app now
  has exactly one chrome row: the File/Edit/View/Help menu bar (every action is
  still in the menus) + the status bar. The light/dark sun/moon toggle moved to
  the **menu bar's right-hand corner** via
  `menuBar()->setCornerWidget(button, Qt::TopRightCorner)` with an auto-raised
  `QToolButton` whose `setDefaultAction()` shares the existing theme QAction
  (so the emoji text/tooltip/trigger stay in one place). `test_smoke` asserts
  `findChildren<QToolBar*>().isEmpty()`; `test_theme` asserts the corner widget.
- **Dock/taskbar icon (Wayland!):** the box runs `XDG_SESSION_TYPE=wayland`
  (ubuntu:GNOME). A Wayland compositor **ignores window icons** — the dock shows
  the `Icon=` of the `.desktop` entry whose *basename* matches the window's app
  id. Three things were missing: (1) no `.desktop` anywhere and no install, so
  the running window had a transient generic entry; (2) the app id needs pinning
  -> `QGuiApplication::setDesktopFileName("mdit")` in `main()`; (3) an actual
  icon in an icon theme dir -> `cmake --install build --prefix ~/.local` writes
  `~/.local/bin/mdit`, `~/.local/share/icons/hicolor/{32..512}/apps/mdit.png`
  and `~/.local/share/applications/mdit.desktop` (no sudo needed; `~/.local/bin`
  is already on the PATH via ~/.zshrc).
- **Verification recipe without a live session** (useful again later):
  `update-desktop-database ~/.local/share/applications` +
  `gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor`, then
  `gio mime text/markdown` (lists `mdit.desktop` as the handler / default) and a tiny
  python3-gi GTK4 probe: `Gtk.IconTheme.new().set_theme_name("hicolor")` +
  `lookup_icon("mdit", None, 48, 1, Gtk.TextDirection.NONE, Gtk.IconFlags(0))`
  -> prints the resolved PNG path. (`QIcon::fromTheme()` is NOT a valid check
  outside a desktop session: without a platform theme its search path is just
  `:/icons`.)
  Remember: the user must RELAUNCH the app (the old process keeps its transient
  entry) and can right-click the dock icon -> "Pin to Dash" to keep it.
- `StartupWMClass=mdit` in the .desktop covers X11 too; note the Pengy recipe on
  this box used a absolute `Icon=/home/beholder/Personal/Pengy/pengy.png` — the
  theme-name lookup works here as well (verified), which stays portable.

## Post-loop polish IV (2026-09-11) — exit guard + the About box
- **Unsaved-changes guard on close.** `MainWindow::closeEvent()` now runs
  `confirmDiscard()` (Save / Discard / Cancel); Cancel calls `event->ignore()` so
  the window stays open. Because `File > Exit` was wired to `QWidget::close()`
  and a session-manager quit also sends a close event, all three paths are
  covered by that one override — no extra work needed.
- **Related bug found while testing it:** the guard's *Save* answer called
  `saveDocument()`, which returns false for an untitled document (no path) — so
  choosing "Save" on a new document refused the close and saved nothing.
  `confirmDiscard()` now routes through `saveAsDocument()` when
  `currentFilePath().isEmpty()`, exactly like `Ctrl+S`/`onSave()`.
- **About box** (`src/AboutDialog.{h,cpp}` + `src/AppInfo.{h,cpp}`): a real
  dialog (mark + name + version + tagline + ©/MIT + the Catbee link with
  `openExternalLinks()`), created once by MainWindow, re-shown, window-modal and
  centered over the window via the same `centerOnParent()` trick as FindBar.
  `AppInfo` is the single source of the strings, and the **version comes from
  CMake** — `target_compile_definitions(mdit_core PUBLIC
  MDIT_VERSION="${PROJECT_VERSION}")` fed to `AppInfo::version()`, so there is no
  second version literal to drift (main() now sets
  `applicationVersion(AppInfo::version())` too).
- **Dark-mode gotcha (worth remembering):** `palette(placeholder-text)` in a QSS
  looks fine in light mode but the dark palette never sets `PlaceholderText`, so
  the "muted" small print rendered nearly invisible. Use the window-text color at
  an alpha instead (`rgba(r, g, b, 65%)`), and re-derive it in
  `changeEvent()` on `PaletteChange`/`StyleChange`/`ApplicationPaletteChange`.
  Same class of bug: Qt's default link color is a dark navy — on dark mode a
  rich-text `<a>` needs an explicit color (we use `QPalette::Highlight`).
- Tests: `test_closeguard` (6 cases) + `test_about` (5 cases) -> **23 CTest
  binaries**, all green. Both About themes verified by grabbing the dialog
  offscreen (`QWidget::grab`) and looking at it.

## Post-loop polish V (2026-09-11) — the theming system (stolen from PengyCPP)
- **What was stolen:** ~/Personal/PengyCPP/themehelper.h — a `Theme` map of named
  colour tokens generated from `mode` x `accent`, a full `appStyleSheet(theme, scale)`,
  and a *direct* UI-scale multiplier (`uiScaleFactor`/`scaledSize`/`scaledFont`/
  `scaledSystemFont`) applied to fonts + explicit widget metrics. PengyCPP's config
  keys are `theme_mode` (system|light|dark), `theme_accent`, `ui_scale` (int %, with
  75/100/110/125/135/150/175/200 offered in its settings dialog).
- **mdit's version:** `Theme::make(dark, accent)` -> `QMap<QString,QString>` of 36
  tokens; **Default accent reproduces the old neutral colours byte-for-byte** (so
  the pre-existing tests/expectations survived), other accents are *computed* by
  blending the neutral surfaces toward the accent (no 16 hand-written tables, unlike
  PengyCPP's literal maps) — `Theme::blend(a, b, t)`. `primary` is the raw accent in
  BOTH modes (PengyCPP parity); only `primary_hover` shifts by mode.
- **Gotchas hit:**
  - A member field named `accentTitle` collided with a static `accentTitle(...)` —
    renamed the static to `accentTitleFor()`.
  - `Settings::ThemeMode::System` reads better than `Auto` in new code: added it as a
    second enumerator *inside* the enum (`System = Auto`) so the spelling works and
    the old `Auto` keeps compiling.
  - The QSS is built by substituting `@{token}` names (NOT positional `.arg()` — a
    27-placeholder sheet is unmaintainable and QString::arg has no 10-arg overload).
    `test_theme` asserts no `@{` and no bare `%` survives: any new token that is
    forgotten in the substitution now fails loudly instead of rendering as `@{x}`.
  - **`QAction` can appear in several menus; a `QMenu` can only be a submenu in one
    place.** The View submenus and the corner button's flat popup therefore share the
    *actions* (one source of truth), not the submenus.
  - Qt's default link navy is unreadable on dark → the preview/About links take
    `QPalette::Highlight`/`doc_link` from the theme. Same family of bug as the
    earlier `palette(placeholder-text)` one.
- Preview/export/highlighter all take the `Theme` now (the old `bool dark` overloads
  remain as `Theme::make(dark)` sugar so the older tests keep working unchanged).

## Post-loop polish VI (2026-09-11) — CI/CD + deployables (stolen from PengyCPP)
Stole PengyCPP's `.github/workflows/{ci,release}.yml`, `build_deb.sh`,
`appimage/build.sh`, `build_macos.sh`, `build_windows.bat` and `check_release.sh`
and adapted them to mdit. The reusable lessons:
- **dpkg versions must start with a digit** — the tag (`v0.2.0`) is NOT a valid
  `Version:` field, so the deb script strips the leading `v`
  (`VERSION="${VERSION#v}"`). This exact thing broke PengyCPP's release once
  (BotTalk 8ef691b9).
- **Parsing the project version:** PengyCPP's `sed -n 's/^project([^ ]* VERSION
  \(...\)/\1/p'` assumes a SINGLE-LINE `project()`. mdit's spans several lines,
  so use `grep -A5 -m1 '^project(' CMakeLists.txt | tr '\n' ' ' | sed -n
  's/.*VERSION[[:space:]]\+\([0-9][^ )]*\).*/\1/p'` (handles both layouts).
  Keeping `project(VERSION ...)` as the single source of truth is what lets both
  `AppInfo::version()` (compile-time `MDIT_VERSION`) and the packaging scripts
  agree.
- **Build the deb payload with `cmake --install --prefix <staging>/usr`** rather
  than copying files by hand: the binary, the hicolor icons and the `.desktop`
  file then come from the same rules a source install uses, so the package can
  never drift. Add `DEBIAN/control`, `usr/share/doc/<pkg>/{copyright,
  changelog.Debian.gz}` (Debian policy: gzipped changelog) and a `postinst` that
  runs `update-desktop-database` + `gtk-update-icon-cache` (+
  `xdg-desktop-menu forceupdate`) so the dock entry and its icon appear with no
  logout. Qt6 `Depends` need the Ubuntu t64 alternations
  (`libqt6core6t64 | libqt6core6`, …).
- **linuxdeploy validates that the icon's file name matches its pixel size**, so
  hand it `assets/icons/mdit-256.png` named `mdit.png` at the AppDir root.
- **Bundle the Qt Wayland platform plugin** (`libqwayland*.so` +
  `libQt6WaylandClient.so.6*` + the wayland shell/graphics/decoration plugin
  dirs) and then *assert* it is in the AppDir: linuxdeploy-plugin-qt ships xcb
  only, which means XWayland (blurry on HiDPI) or a hard failure on Wayland-only
  compositors.
- **Verifying icon resolution without root:** extract the deb
  (`dpkg-deb -x pkg.deb /tmp/root`) and run the GTK lookup with
  `XDG_DATA_HOME=/tmp/no-home XDG_DATA_DIRS=/tmp/root/usr/share:/usr/share` —
  XDG_DATA_HOME defaults to ~/.local/share and would otherwise shadow the
  packaged icon (it did on the first run!). Check registration with
  `grep text/markdown /tmp/root/usr/share/applications/mimeinfo.cache` or
  `gio mime text/markdown` — NOT `xdg-mime query default`, whose generic
  fallback returns the first cache entry regardless of the real default (and
  GLib ignores a `.desktop` whose `Exec` binary is not on PATH).
- AppImages need FUSE: `APPIMAGE_EXTRACT_AND_RUN=1 ./linuxdeploy-x86_64.AppImage`
  works on a box without libfuse2 (both for building and for running the result).

## Post-loop fix (2026-09-11) — tests were writing the user's real settings
- **Symptom the user reported:** the chosen theme/accent/UI scale "didn't save" —
  mdit came back looking reset, with a *red* highlight. **Cause:** the
  persistence code was fine; `MainWindow`'s default constructor uses the REAL
  `QSettings("mdit","mdit")` (→ `~/.config/mdit/mdit.conf`), so any test that
  built a `MainWindow` wrote the user's config. `test_theme`'s
  `changingTheThemeRepaintsPreviewAndHighlighter()` (a `MainWindow w;` with no
  backing) left `theme=dark`, `themeAccent=red`; the close-guard/save tests
  filled `recentFiles` with `/tmp/test_*` paths. Running `ctest` after the user
  changed a setting silently reverted it.
- **Helper:** `tests/testmain.h` → `isolateUserSettings()` redirects the Qt
  settings path table (`QSettings::setPath(NativeFormat/IniFormat, UserScope,
  tmpdir)`) into a process-lifetime `QTemporaryDir`, and an **auto-invoked
  file-scope static** runs it at load time so no test can forget it (this also
  covers `QTEST_GUILESS_MAIN` binaries, which build their own QCoreApplication
  inside QtTest's main and have no place to insert a call).
- **Diagnostic that proved it:** `QSettings("mdit","mdit").fileName()` +
  `allKeys()` in a throwaway probe, then diffing `~/.config/mdit/mdit.conf`
  before/after a full `ctest` run (now: unchanged ✓).
- **Gotcha:** `QSettings::fileName(format, scope)` is NOT a thing — there is no
  static path getter; construct a throwaway `QSettings` and read its
  `fileName()` (it honours `setPath`). First guard-test attempt failed because of
  this, which is exactly why the guard test is worth having.
- **Also:** the shipped default theme mode is now **Light** (was Auto/System),
  with Default accent + 100% scale, per the user's request.
- **Cleanup done for the user:** removed the polluted `theme`/`themeAccent`/
  `uiScale`/`recentFiles` keys from their `~/.config/mdit/mdit.conf` (backup at
  /tmp/mdit_conf_backup_before_cleanup.conf) so the next launch is light/Default/100%.

## Publishing (2026-09-11) — github.com/patw/mdit + first release
`gh repo create mdit --public --source=. --remote=origin --push` (logged in as
**patw**, SSH; all their repos are public, descriptions are one-line technical
summaries), topics + homepage via `gh repo edit`, then `git tag v0.1.0 &&
git push origin v0.1.0` to fire the release workflow. Both workflow runs and the
published artifacts were verified by downloading them back.

**The CI/CD lessons — every one of these only shows up OFF the dev box:**
1. **Qt version floor:** `QStyleHints::colorScheme()`/`Qt::ColorScheme` are Qt
   6.5+; Ubuntu 24.04 ships **6.4.2**. Guard with `QT_VERSION_CHECK` and fall
   back to the palette's window colour.
2. **MSVC is stricter than GCC/Clang:** implicit `qsizetype`→`int` narrowing in
   brace init is an *error* (C2397), and `M_PI` does not exist without
   `_USE_MATH_DEFINES`. Cast explicitly / carry your own `kPi`.
3. **macOS:** `install(TARGETS ...)` needs a **BUNDLE DESTINATION** for a
   `MACOSX_BUNDLE` target or *configure* fails (before any build).
4. **Standard key sequences are not portable:** on a bare CI runner
   `QKeySequence::Quit` comes out **empty**, so a test that finds an action by
   shortcut matches an arbitrary shortcut-less action. Actions now carry stable
   `setObjectName()`s (`action.save`, `action.exit`, `menu.file`, …) and tests
   look them up by name. Also: `QIODevice::Text` translates `\n`→`\r\n` on
   Windows, which mangled a CRLF test fixture (write fixtures in binary mode).
5. **The 2-argument `QSettings(organization, application)` constructor always
   uses NativeFormat** — a plist on macOS, the registry on Windows — and neither
   honours `QSettings::setPath`. Redirecting the path table therefore only ever
   isolated *Linux* tests. Fix: `Settings` honours a `MDIT_SETTINGS_INI` env var
   (set by `tests/testmain.h`) and then uses that explicit ini FILE, so the
   seam is portable.
6. **A headless runner has no display:** any invocation of the GUI binary needs
   `QT_QPA_PLATFORM=offscreen` — that includes the packaging scripts' smoke
   tests (`mdit --version` core-dumped inside `build_deb.sh` and failed the
   release) and never pipe into `head` (SIGPIPE).
7. **linuxdeploy-plugin-qt** prunes plugins it did not add itself; bundle extra
   ones via `EXTRA_PLATFORM_PLUGINS`, computed from a **glob** — the wayland
   plugin is `libqwayland.so` on Qt 6.10 but `libqwayland-{egl,generic}.so` on
   Ubuntu 24.04's Qt 6.4, and naming a missing file makes the plugin exit 1.
   Adding `libqoffscreen.so` also makes the published AppImage runnable
   headlessly, which is exactly how CI smoke-tests it now.
8. **Artifact naming:** keep the tag's `v` out of the file names (the `.deb`'s
   `Version:` field must start with a digit anyway) so the release listing is
   consistent.
