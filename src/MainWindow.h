// MainWindow — mdit's single-window shell.
//
// A `QMainWindow` holding a `QSplitter` (EditorPane left, PreviewPane right) at
// an initial 50/50 ratio, plus a menu bar (the app's ONLY chrome row — there is
// deliberately no toolbar; its buttons duplicated the menus) and a status bar.
// `openFile()` loads a markdown path (or resets to untitled) and
// `togglePreview()` hides/shows the right pane — these are the two behaviors the
// skeleton already exercises headlessly.
//
// Later subtasks wire the menu/toolbar actions to the real features:
//   1  Open/New through the `Document` model + dirty guard
//   5  Save / Save As (done) — the menu keys are wired to real behavior
//   6  Find (Ctrl+F) + Replace (Ctrl+H) — both done, via the shared FindBar
//   9  Export HTML/PDF + recent files
//   7  Theme toggle + `Settings` persistence
#pragma once

#include "Document.h"
#include "MarkdownModel.h"
#include "Settings.h"
#include "Theme.h"

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QStringList>

class QSplitter;
class QTimer;
class QAction;
class QMenu;
class QLabel;
class QToolButton;
class AboutDialog;
class EditorPane;
class PreviewPane;
class FindBar;
class QDragEnterEvent;
class QDropEvent;
class QResizeEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // `backing` (optional) is a `QSettings` the Settings wrapper should use
    // instead of the real user settings — the test seam so an offscreen run
    // never reads/writes a user's real mdit settings (see Settings.h).
    explicit MainWindow(QWidget *parent = nullptr, QSettings *backing = nullptr);

    EditorPane *editorPane() const { return m_editor; }
    PreviewPane *previewPane() const { return m_preview; }
    QSplitter *splitter() const { return m_splitter; }

    // The View > Theme action (`Ctrl+T`) and its right-hand twin on the menu bar
    // — the sun/moon light-dark toggle (icon-only, sitting in the menu bar's
    // right-hand corner, which is the app's only chrome row). Exposed so tests
    // can assert the emoji/state and drive the toggle.
    QAction *themeAction() const { return m_themeAct; }
    QAction *themeButtonAction() const { return m_themeButtonAct; }
    // The corner button itself (shares that action: text/tooltip/trigger).
    QToolButton *themeButton() const { return m_themeButton; }

    // --- the theming system (mode x accent x UI scale) ---------------------
    // The currently applied theme (the resolved palette: mode **and** accent)
    // and its persisted inputs. Exposed for tests and the About box.
    const ::Theme &theme() const { return m_theme; }
    Settings::ThemeMode themeMode() const { return m_themeMode; }
    QString themeAccent() const { return m_accent; }
    int uiScale() const { return m_uiScale; }
    // The Mode/Accent/UI-scale submenus (View) and the corner button's popup,
    // which reuses the same QActions.
    QMenu *themeMenu() const { return m_themeMenu; }
    QMenu *accentMenu() const { return m_accentMenu; }
    QMenu *uiScaleMenu() const { return m_uiScaleMenu; }
    QMenu *themeOptionsMenu() const { return m_themeOptionsMenu; }
    QAction *themeModeAction(Settings::ThemeMode mode) const;
    QAction *accentAction(const QString &accent) const;
    QAction *uiScaleAction(int percent) const;

    // The reusable About box (Help > About), created on first use. Exposed so a
    // test can assert its contents after onAbout().
    AboutDialog *aboutDialog() const { return m_aboutDialog; }

    // Fraction (0.0..1.0) of the splitter width currently given to the editor;
    // 0.5 == a 50/50 split. Returns 0.5 when the splitter has no usable size yet.
    double splitterRatio() const;

    // Set the splitter ratio (editor fraction, clamped to 0.05..0.95 so neither
    // pane can fully collapse — the splitter is non-collapsible) and persist it
    // through Settings. When the splitter has no width yet (offscreen, before
    // the first layout) the ratio is remembered and applied on the next resize.
    void setSplitterRatio(double ratio);

    // User's answer to the dirty-discard prompt (scriptable in tests via
    // askDiscardChoice()).
    enum class DiscardChoice { Save, Discard, Cancel };

    // The document model that owns the content, file path and dirty flag.
    // Exposed so tests can inspect file/dirty state and so export (subtask 9)
    // shares the same source of truth.
    Document *document() { return &m_doc; }
    const Document *document() const { return &m_doc; }

    // Whether the current document has unsaved changes.
    bool dirty() const { return m_doc.dirty(); }

    // The typed settings wrapper the window persists its preferences through
    // (theme, and later preview visibility / splitter ratio / recent files).
    // Exposed so tests can assert persistence; production constructs it from the
    // real user settings (or a test-supplied backing, see the ctor).
    Settings *settings() { return &m_settings; }

    // The File > "Recent Files" submenu (subtask 9): the last N (default 5)
    // opened/saved markdown paths, most-recent-first, persisted through
    // Settings. Each entry reopens its file through openFile() (dirty guard
    // first). Exposed so tests can inspect the entries headlessly.
    QMenu *recentFilesMenu() const { return m_recentMenu; }
    // The persisted recent-file list (most-recent-first, capped) as currently
    // reflected in the submenu. Exposed for tests.
    QStringList recentFiles() const { return m_settings.recentFiles(); }

    // Load the markdown file at `path` (streaming read, \r\n->\n normalization)
    // through the Document model into the editor, set the preview base URL to
    // the file's directory, update the window title, and show the opened path in
    // the status bar ("Opened <path>"). Protected by the dirty guard
    // (confirmDiscard). Returns true if a file was loaded, false if the guard
    // refused, the path does not exist (resets to an empty untitled document),
    // or the file could not be read.
    bool openFile(const QString &path);

    // Reset to a blank untitled document and clear both panes. Protected by the
    // dirty guard (confirmDiscard).
    void newDocument();

    // The dirty-document guard: returns true if the document is clean, or if the
    // user explicitly agrees to discard (or successfully saves). Returns false
    // when the user cancels. `askDiscardChoice()` is virtual so tests can script
    // the user's answer without a real modal dialog.
    bool confirmDiscard();
    virtual DiscardChoice askDiscardChoice();

    // Write the current editor content to the document's file path via the
    // Document model (UTF-8, remembered line ending). Returns false for an
    // untitled document (Save As is required — onSave() routes there) or on
    // write error. Updates the title and shows a status-bar message.
    bool saveDocument();

    // Save As: asks for a path through the (scriptable) `askSaveAsPath()`
    // dialog seam, writes via the Document model (which updates
    // currentFilePath and clears dirty on success), re-renders the preview
    // (the base URL follows the new directory) and updates the title/status.
    // Returns false when the dialog was canceled (no path chosen) or on write
    // error.
    bool saveAsDocument();

    // The file name suggested to the Save As dialog: the current file name
    // when the document is saved, otherwise the untitled name with a ".md"
    // extension appended.
    QString suggestedSaveName() const;

    // The user's chosen Save As path (or an empty string on cancel).
    // Virtual so tests can script the dialog's answer without a real modal
    // (same seam pattern as askDiscardChoice()).
    virtual QString askSaveAsPath(const QString &suggestedName);

    // --- Export HTML (subtask 9). --------------------------------------------
    // Export the current content to a standalone .html file: renders through
    // the shared RenderedDocument (the same path the preview uses, so what you
    // preview is what you export), wraps the toHtml() output in a standalone
    // page (doctype + charset + the theme stylesheet), and writes it to the
    // path chosen through the (scriptable) askExportHtmlPath() seam. Relative
    // image references are copied next to the saved HTML (same relative paths)
    // so the exported file keeps working where the source images lived.
    // Returns false when the dialog was canceled (nothing written) or the
    // write failed. Does NOT change the document's dirty state (export is a
    // read-only view of the content).
    bool exportHtmlDocument();
    // The file name suggested to the Export HTML dialog: the document's stem
    // with a .html extension (saved: "notes.md" -> "notes.html"; untitled:
    // "untitled" -> "untitled.html").
    QString suggestedHtmlName() const;
    // The user's chosen export path (or an empty string on cancel).
    // Virtual so tests can script the dialog's answer without a real modal
    // (same seam pattern as askSaveAsPath()).
    virtual QString askExportHtmlPath(const QString &suggestedName);

    // --- Export PDF (subtask 9). --------------------------------------------
    // Export the current content to a standalone .pdf file: renders through the
    // shared RenderedDocument (the same path the preview / HTML export use, so
    // what you preview is what you export) and prints it into a QPdfWriter with
    // a set page size / orientation / resolution (DPI). Writes to the path
    // chosen through the (scriptable) askExportPdfPath() seam. Returns false
    // when the dialog was canceled (nothing written) or the write failed. Does
    // NOT change the document's dirty state (export is a read-only view).
    bool exportPdfDocument();
    // The file name suggested to the Export PDF dialog: the document's stem
    // with a .pdf extension (saved: "notes.md" -> "notes.pdf"; untitled:
    // "untitled" -> "untitled.pdf").
    QString suggestedPdfName() const;
    // The user's chosen export path (or an empty string on cancel).
    // Virtual so tests can script the dialog's answer without a real modal
    // (same seam pattern as askExportHtmlPath()).
    virtual QString askExportPdfPath(const QString &suggestedName);
    // The render resolution (DPI) the PDF export uses (spec: "set page size/
    // DPI"). Default 96; clamped to a positive value on write.
    int pdfResolutionDpi() const { return m_pdfResolutionDpi; }
    void setPdfResolutionDpi(int dpi) { m_pdfResolutionDpi = qMax(1, dpi); }

    // Drag-and-drop entry point: open the first dropped local `.md` / `.markdown`
    // path (dirty-guarded); other files are ignored. Public so tests can drive a
    // drop without synthesizing a QDropEvent.
    void handleDroppedPaths(const QStringList &paths);

    // Show/hide the preview pane. When hidden the editor takes the full width;
    // when re-shown the last ratio is restored. Returns the new visible state.
    bool togglePreview();
    bool isPreviewVisible() const;

    // --- Find bar (subtask 6, find half). -----------------------------------
    // The Ctrl+F find bar docked over the editor (owns the next/prev/case/count
    // chrome and drives EditorPane::find/findPrevious/matchCount). Exposed for
    // tests to drive headlessly.
    FindBar *findBar() const { return m_findBar; }
    // Show the find bar and focus its search field (Ctrl+F). If it is already
    // visible this just refocuses it.
    void showFindBar();

    // --- Undo / Redo (post-loop polish). -----------------------------------
    // The Edit menu's Undo (Ctrl+Z) / Redo (Ctrl+Shift+Z / Ctrl+Y) entries,
    // driving the editor's own document undo stack. The actions are OWNED by
    // the editor pane and scoped to it (Qt::WidgetWithChildrenShortcut), so the
    // keys only reach the document while the editor has focus — the find bar's
    // search/replace fields keep their own native undo — while the menu still
    // offers (and labels) both. Their enabled state follows the undo stack
    // (QPlainTextEdit::undoAvailable/redoAvailable).
    QAction *undoAction() const { return m_undoAct; }
    QAction *redoAction() const { return m_redoAct; }

    // --- Live preview (subtask 4.2). ---------------------------------------
    // Re-render the preview from the current document NOW (via the shared
    // RenderedDocument renderer): the base URL is derived from the document's
    // current file path (so relative images resolve against its directory) and
    // the theme follows the current light/dark state. A no-op when the preview
    // is hidden — the debounced path only renders while the pane is visible.
    void updateLivePreview();
    // The debounce interval (ms) before a keystroke re-renders the preview.
    // Defaults to PreviewDebouncer::defaultIntervalMs() (150 ms); tests shorten
    // it so the offscreen run need not wait.
    int previewDebounceMs() const { return m_debounceMs; }
    void setPreviewDebounceMs(int ms);

    // --- Status bar (subtask 8.2): word/char counts + modified indicator. ---
    // The two permanent status-bar labels (right-aligned): a word/char count
    // readout and a modified/unmodified indicator. Exposed so tests can assert
    // their text headlessly.
    QLabel *statusCountsLabel() const { return m_countsLabel; }
    QLabel *statusModifiedLabel() const { return m_modifiedLabel; }
    // Refresh the word/char count label from the editor (wordCount()/charCount
    // == the MarkdownModel canonical counts). Driven by the live-preview
    // debounce (onPreviewTimerTimeout) and by open/new so it never goes stale;
    // public so a test can force an immediate refresh.
    void refreshStatusCounts();
    // (Re)set the modified indicator from the document's dirty flag. O(1) — it
    // is refreshed on every dirty-affecting change (via updateTitle()) so it is
    // live, unlike the (deferred) count readout.
    void updateModifiedIndicator();

    // Derive the window title from the current state (filename + '*' if dirty).
    QString titleFor(bool dirty) const;

    // -- theming system -------------------------------------------------------
    // Apply the persisted mode/accent/scale: resolve the Theme, push it through
    // the application (palette + QSS + scaled font), the editor (highlighter +
    // scaled monospace font) and the preview.
    void applyTheme();
    // Persist + apply. The UI calls these from the View menus / the corner
    // button; tests drive them directly.
    void setThemeMode(Settings::ThemeMode mode);
    void setThemeAccent(const QString &accent);
    void setUiScale(int percent);
    // Whether the resolved theme is dark (mode System follows the platform).
    bool darkTheme() const { return m_theme.dark; }

public slots:
    void onNew();
    void onOpen();
    void onSave();   // Save (Ctrl+S) — untitled documents go to Save As
    void onSaveAs(); // Save As (Ctrl+Shift+S)
    void onTogglePreview();
    void onToggleTheme();
    void onFind();
    void onReplace();
    void onUndo();
    void onRedo();
    void onExportHtml();
    void onExportPdf();
    void onAbout();
    void onEditorTextChanged(); // editor textChanged -> mark model dirty + title
    void onPreviewTimerTimeout(); // debounced -> re-render the live preview
    void onEditorScrolled();     // editor scroll -> follow in the preview
    void onPreviewScrolled(double ratio); // preview scroll -> follow in the editor
    void onOpenRecent();         // a Recent Files entry was clicked -> openFile

private:
    // Record `path` in the persisted recent-files list (most-recent-first,
    // capped, de-duplicated) and refresh the "Recent Files" submenu. Called
    // after a successful open / save / save-as. A no-op for empty paths.
    void addRecentFile(const QString &path);
    // (Re)build the "Recent Files" submenu from Settings::recentFiles(): one
    // enabled action per path (most-recent-first), or a disabled placeholder
    // (and a hidden menu) when the list is empty.
    void rebuildRecentMenu();

private:
    void buildUi();
    void updateTitle();
    void showStatus(const QString &msg);
    // Refresh the find bar's match count in place (on editor textChanged) — the
    // bar stays visible while the user edits; this never moves the selection.
    void refreshFindBarCount();
    // Re-derive the Undo/Redo actions' enabled state from the editor's undo
    // stack (called on construction, on undoAvailable/redoAvailable, and after
    // each undo/redo).
    void updateUndoRedoActions();
    // Build the corner button's popup (the same Theme/Accent/UI-scale actions
    // the View submenus hold, laid out flat). Called once from buildUi().
    QMenu *buildThemeOptionsMenu();
    // Tick the Theme/Accent/UI-Scale radio actions to match the live state.
    void syncThemeActions();
    // Keep the menu-bar corner button's text/tooltip in step with the theme: a sun
    // while the theme is light, a moon while it is dark (called on construction
    // — the persisted theme is resolved before the UI is built — and on every
    // toggle).
    void updateThemeButton();
    // Reset to untitled WITHOUT the dirty guard (the caller has already asked).
    void newDocumentNoGuard();
    // (Re)schedule the debounced live-preview render; a no-op when the preview
    // is hidden (the spec: only render while the pane is visible).
    void schedulePreviewUpdate();
    // Persist the current splitter ratio through Settings (splitterMoved +
    // setSplitterRatio both funnel here).
    void persistSplitterRatio();
    // Apply the persisted splitter ratio (Settings::splitterRatio, default 0.5)
    // to the current splitter width — used on startup, on re-showing the
    // preview, and when a deferred ratio is due (no width yet at set-time).
    void applyPersistedRatio();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    // The unsaved-changes guard on window close / File > Exit / session quit
    // (dragEnterEvent/dropEvent keep the drag-and-drop open path).
    void closeEvent(QCloseEvent *event) override;
    // Re-apply a deferred splitter ratio (setSplitterRatio() called while the
    // splitter had no width yet) once the window has a real width.
    void resizeEvent(QResizeEvent *event) override;

    QSplitter *m_splitter = nullptr;
    EditorPane *m_editor = nullptr;
    PreviewPane *m_preview = nullptr;
    FindBar *m_findBar = nullptr;
    // Permanent status-bar readouts (subtask 8.2): the word/char count label
    // (refreshed on the live-preview debounce) and the modified/unmodified
    // indicator (refreshed on every dirty change).
    QLabel *m_countsLabel = nullptr;
    QLabel *m_modifiedLabel = nullptr;

    // Actions (created once in buildUi(); wired to their features in later
    // subtasks — for now the structural ones work and the rest report a
    // "coming" status so the menu/toolbar shell is complete with real keys).
    QAction *m_newAct = nullptr;
    QAction *m_openAct = nullptr;
    QAction *m_saveAct = nullptr;
    QAction *m_saveAsAct = nullptr;
    QAction *m_findAct = nullptr;
    QAction *m_replaceAct = nullptr;
    QAction *m_undoAct = nullptr; // Edit > Undo (Ctrl+Z)   — owned by the editor
    QAction *m_redoAct = nullptr; // Edit > Redo (Ctrl+Shift+Z / Ctrl+Y)
    QAction *m_previewToggleAct = nullptr;
    // View > "Toggle light/dark" (Ctrl+T) and its menu-bar-corner twin.
    QAction *m_themeAct = nullptr;
    QAction *m_themeButtonAct = nullptr; // menu-bar corner sun/moon twin of m_themeAct
    QToolButton *m_themeButton = nullptr; // the corner widget showing that action
    // The theming system's menus: View > Theme / Accent / UI Scale (radio
    // groups) and the corner button's flat popup, which shares the same actions.
    QMenu *m_themeMenu = nullptr;
    QMenu *m_accentMenu = nullptr;
    QMenu *m_uiScaleMenu = nullptr;
    QMenu *m_themeOptionsMenu = nullptr;
    QList<QAction *> m_themeModeActs; // System / Light / Dark
    QList<QAction *> m_accentActs;    // one per Theme::accents()
    QList<QAction *> m_scaleActs;     // one per Theme::offeredScales()
    // The resolved theme + its persisted inputs (mode is the Settings enum:
    // Auto == "System", Light, Dark).
    Theme m_theme;
    Settings::ThemeMode m_themeMode = Settings::ThemeMode::Auto;
    QString m_accent = QStringLiteral("default");
    int m_uiScale = 100;
    QAction *m_exportHtmlAct = nullptr;
    QAction *m_exportPdfAct = nullptr;
    QMenu *m_recentMenu = nullptr; // File > Recent Files (subtask 9)
    QAction *m_exitAct = nullptr;
    QAction *m_aboutAct = nullptr;
    // The About box (created lazily by onAbout() and re-shown afterwards).
    AboutDialog *m_aboutDialog = nullptr;

    Document m_doc;      // owns content, path and dirty flag
    bool m_updating = false; // true while we programmatically set editor text

    // Live-preview debounce (subtask 4.2): a singleShot QTimer restarted on each
    // editor change; on timeout the elapsed time is run through the pure
    // PreviewDebouncer policy and the preview renders (only if visible).
    QTimer *m_previewTimer = nullptr;
    QElapsedTimer m_editClock;
    int m_debounceMs = PreviewDebouncer::defaultIntervalMs();
    // Render resolution (DPI) for the PDF export (subtask 9). A4 portrait is
    // the page size; this is the DPI passed to the QPdfWriter.
    int m_pdfResolutionDpi = 96;
    // Guards the editor<->preview scroll sync against feedback loops: set while
    // one pane is being driven from the other, so the re-entrant scroll signal
    // is ignored.
    bool m_syncingScroll = false;
    // A splitter ratio was requested before the splitter had a width to scale
    // it into (offscreen before first layout); it is re-applied from Settings
    // on the next resizeEvent (setSplitterRatio() already persisted the value).
    bool m_pendingSplitterRatio = false;

    // Typed settings (theme / preview / splitter / recent files). Constructed
    // from the ctor-supplied `backing` (or the real user settings when none).
    // By value — Settings is non-copyable, so it is initialized in the ctor's
    // member-init list and never copied.
    Settings m_settings;
};
