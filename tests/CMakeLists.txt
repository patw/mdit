# mdit test suite — C++ QtTest binaries driven by CTest.
#
# Every test binary is registered with CTest and forced to the offscreen Qt
# platform plugin so the suite is headless-safe (no display needed). New test
# files added by later subtasks (test_document, test_render, test_editor,
# test_markdownmodel, test_settings, test_mainwindow, test_github_ready) follow
# the same pattern via add_mdit_test().
find_package(Qt6 COMPONENTS Test REQUIRED)

function(add_mdit_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        mdit_core
        Qt6::Test
        Qt6::Widgets
        Qt6::Gui
        Qt6::PrintSupport
    )
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120
    )
endfunction()

# Subtask 0.1 scaffold check: the Qt6/QtTest toolchain configures, compiles,
# links, and runs a trivial QtTest under the offscreen platform plugin.
add_mdit_test(test_scaffold)

# Subtask 0.2: the window shell builds and behaves headlessly (splitter layout,
# ONE chrome row — the menu bar (with the theme toggle in its corner) + status
# bar, no toolbar — preview toggle, optional file open).
add_mdit_test(test_smoke)

# Subtask 1.1: the pure Document model — UTF-8 round-trip, CRLF normalization,
# dirty transitions, save-as path + dirty clearing, title/untitled naming.
add_mdit_test(test_document)

# Subtask 1.2: Open/New wired through the Document model + dirty guard
# (confirmDiscard) + drag-and-drop open of .md / .markdown files.
add_mdit_test(test_opennew)

# Subtask 2.1: the pure RenderedDocument markdown renderer — full feature set,
# headings/emphasis/code/table/link/blockquote/task-list markup, base-URL image
# resolution, and the light/dark theme stylesheet. (Uses a QGuiApplication
# offscreen for the QPixmap-based image-resolution check.)
add_mdit_test(test_render)

# Subtask 2.2: the widget-side PreviewPane — setRendered renders through the
# shared renderer into the pane's live document, setBaseUrl enables relative
# image resolution, and scrollRatio()/setScrollRatio() round-trip the vertical
# scroll position (with scrollRatioChanged for the subtask-4 sync).
add_mdit_test(test_previewpane)

# Subtask 3 (highlighter): the markdown QSyntaxHighlighter — headings h1–h6,
# strong/emphasis, inline code, fenced code blocks (per-block state, no inline
# rules leak into code), links/images, blockquotes, horizontal rules, list
# markers, and the light/dark theme-aware color map.
add_mdit_test(test_highlighter)

# Subtask 3 (editor pane): the EditorPane — line-number margin (block count +
# digit-width sizing, a real left-margin widget), the hosted MarkdownHighlighter
# (formats a heading/emphasis range + setHighlighterDark re-themes), word/char
# counts, the textChanged passthrough, and the find/replace helpers.
add_mdit_test(test_editor)

# Subtask 4.1: the pure MarkdownModel text logic — ATX heading extraction
# (level+text, closing-hash stripping, fenced-code exclusion, indentation
# rules), the canonical word/char counts, and the PreviewDebouncer policy
# (render-now vs wait-N-ms from the elapsed time). Pure logic: no
# QApplication needed (QTEST_GUILESS_MAIN).
add_mdit_test(test_markdownmodel)

# Subtask 4.2: the live-preview wiring in MainWindow — the debounced render on
# editor change (only while the preview is visible), the immediate render on
# open, the base-URL-from-document-path image resolution, the re-render on
# re-show, the editor->preview scroll-ratio sync, and the feedback guard.
# (Widget test; uses an offscreen show() to give the panes real scroll ranges.)
add_mdit_test(test_livepreview)

# Subtask 5: Save (Ctrl+S) + Save As (Ctrl+Shift+S) wired through the Document
# model in MainWindow — spec shortcuts on the menu actions, save writes UTF-8
# (remembered CRLF honored), title '*' drops on success, status-bar messages,
# untitled Save routes to Save As, cancel leaves state untouched, bad path
# fails loudly, and the dialog's suggested name comes from the current document.
# The Save As dialog is scripted via the virtual askSaveAsPath() seam (widget
# test, offscreen QApplication).
add_mdit_test(test_save)

# Subtask 7 (first item): the typed Settings (QSettings wrapper) — theme
# (auto/light/dark), previewVisible, splitterRatio, and a capped recent-files
# list, all with the spec defaults and round-tripping through an isolated
# temp-dir QSettings (real disk persistence). Pure logic: no QApplication.
add_mdit_test(test_settings)

# Subtask 7 (theme + toggle): Theme::apply sets Fusion + light/dark palette + an
# application QSS, Theme::stylesheet(dark) differs by mode, the pure
# Theme::resolveDark() maps auto/light/dark, and the MainWindow resolves the
# persisted theme on startup and, on the View>Theme toggle, flips light/dark,
# persists the choice through Settings (isolated temp QSettings) and re-applies
# the RenderedDocument stylesheet to the live preview. Widget test (offscreen).
add_mdit_test(test_theme)

# Subtask 8.1 (preview toggle + splitter-ratio persistence) + 8.2 (status-bar
# word/char counts + modified indicator): the preview is ON by default, the
# View-menu "Preview" action (Ctrl+Shift+P, checkable) hides/shows the pane and
# persists its state, a hidden preview gives the editor the full width, the
# splitter ratio is persisted on change (setSplitterRatio and splitterMoved ->
# Settings) and restored on startup and on re-show (not reset to 50/50), and
# fresh settings start at the spec defaults (preview ON, 50/50). Also: the
# permanent status-bar word/char count readout (debounce-driven, refreshes even
# while the preview is hidden, pluralized, immediate on open/new) and the
# modified/unmodified indicator (O(1), flips on every dirty-state change).
# Widget test (offscreen QApplication, isolated temp-file QSettings backing).
add_mdit_test(test_mainwindow)

# Subtask 9.1 (Export HTML): File > Export > HTML... renders through the shared
# RenderedDocument and writes a non-empty STANDALONE .html (doctype, charset,
# the theme stylesheet, the lifted <body> content) to the path chosen through
# the scriptable askExportHtmlPath() seam; the suggested name is the document
# stem + .html; the export carries the current editor content (unsaved edits
# included) without dirtying the document; cancel writes nothing; a bad path
# fails with "Could not export <name>"; the dark theme exports the dark
# stylesheet; and relative image references are copied next to the saved HTML
# (same relative paths, subdirs recreated). Widget test (offscreen, isolated
# temp QSettings).
add_mdit_test(test_export_html)

# Subtask 9.2 (Export PDF): File > Export > PDF... renders through the shared
# RenderedDocument and prints into a QPdfWriter (A4 portrait, a set DPI) to the
# path chosen through the scriptable askExportPdfPath() seam; the result is a
# real PDF whose first bytes are the %PDF- magic header. The suggested name is
# the document stem + .pdf; the export carries the current editor content
# without dirtying the document; cancel writes nothing; a bad path fails with
# "Could not export <name>"; and renderPdf() returns false for an unwritable
# destination. Widget test (offscreen, isolated temp QSettings, PrintSupport).
add_mdit_test(test_export_pdf)

# Subtask 10.1 (CLI open + DnD polished): the application entry point, tested
# without launching the GUI. The pure AppCli::fileToOpen() CLI decision returns
# the first positional argument that is an existing .md/.markdown file (a missing
# path, a directory, a non-markdown file, or no arguments yield an empty result
# = the empty untitled state); the MainWindow then opens the CLI-chosen file and
# shows the OPENED PATH in the status bar (title carries the file name), a
# missing/bad CLI argument leaves the window untitled, and a dropped .md/
# .markdown opens with the same status path. Widget test (offscreen, isolated
# temp QSettings so Recent Files never clobbers a user's real settings).
add_mdit_test(test_entrypoint)

# Subtask 10.2 (GitHub-ready guard): asserts the repository stays
# GitHub-ready — README.md (title/features/build+exact command/run+CLI-open/
# usage/shortcuts/license note), LICENSE (MIT License + Copyright (c) 2026 Pat
# Wendorf), CHANGELOG.md (## [0.1.0] heading, Keep a Changelog style), spec.md
# present, .gitignore covering build/export artifacts, and CMakeLists.txt
# declaring project(mdit) + enable_testing() + C++20 + the Qt6 components.
# Pure file reading (QTEST_GUILESS_MAIN, no QApplication). MDIT_REPO_ROOT
# pins the source directory so the check is CWD-independent (a bare ctest
# run falls back to ../../ from the build/tests working dir).
add_mdit_test(test_github_ready)
target_compile_definitions(test_github_ready PRIVATE
    MDIT_REPO_ROOT="${CMAKE_SOURCE_DIR}")

# Post-loop polish: undo / redo while editing. EditorPane canUndo()/canRedo()
# track the document undo stack, the Ctrl+Z / Ctrl+Shift+Z keys undo and redo a
# real edit, a programmatic load starts a fresh history, Replace All is ONE undo
# step, and MainWindow's Edit > Undo / Redo actions follow the stack while the
# dirty flag returns to clean when an undo lands back on the loaded/saved text.
add_mdit_test(test_undo)

# Post-loop polish: the find/replace UI is its own non-modal POPUP WINDOW (a
# QDialog) centered over the main window on every show (it used to be an overlay
# glued under the menu bar); the title names the mode and Esc returns the focus
# to the editor. Widget test (offscreen; the main window IS shown for a frame).
add_mdit_test(test_findwindow)

# Post-loop polish: the application icon — the Pengy-mirroring white disc with a
# '#' mark (transparent outside), compiled in from resources/mdit.qrc and pinned
# by pixel/geometry assertions (round white disc, centred two-bar-cross hash,
# every 16…512 px size present) plus the window-icon wiring.
add_mdit_test(test_icon)

# Post-loop polish IV: the unsaved-changes guard on CLOSING the window (the
# title-bar X / File > Exit / session quit run the same guard as New/Open: save,
# explicitly discard, or cancel and stay open — Save routes an untitled document
# to Save As) and the Help > About box (brand mark + name/version/tagline +
# copyright/license + the Catbee link).
add_mdit_test(test_closeguard)
add_mdit_test(test_about)