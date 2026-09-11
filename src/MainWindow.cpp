#include "MainWindow.h"

#include "AboutDialog.h"
#include "AppIcons.h"
#include "EditorPane.h"
#include "FindBar.h"
#include "PreviewPane.h"
#include "RenderedDocument.h"
#include "Settings.h"
#include "Theme.h"

#include <utility> // std::as_const

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QLabel>
#include <QMenu>
#include <QPageSize>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVector>

MainWindow::MainWindow(QWidget *parent, QSettings *backing)
    : QMainWindow(parent), m_settings(backing)
{
    // Resolve the persisted theming BEFORE building the UI, so the menus'
    // checked states and the applied palette/fonts are right from first paint:
    // mode (System/Light/Dark) x accent x UI scale, straight from PengyCPP's
    // model — plus, of course, the old auto/light/dark behaviour.
    m_themeMode = m_settings.theme();
    m_accent = m_settings.themeAccent();
    m_uiScale = m_settings.uiScale();
    m_theme = Theme::make(m_themeMode, m_accent, Theme::isSystemDark(*qApp));
    // The window/taskbar icon (white disc + '#', compiled in via the resource);
    // set here so the window carries it even when main() is not the caller
    // (tests), where it also mirrors the application icon.
    setWindowIcon(AppIcons::windowIcon());
    setWindowTitle(titleFor(/*dirty=*/false));
    resize(1100, 750);
    setAcceptDrops(true); // drag-and-drop of .md / .markdown onto the window
    buildUi();
    applyTheme();
}

void MainWindow::buildUi()
{
    // --- Splitter: editor (left) | preview (right), initial 50/50. -----------
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_editor = new EditorPane(m_splitter);
    m_preview = new PreviewPane(m_splitter);
    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setHandleWidth(4);
    m_splitter->setChildrenCollapsible(false);
    // Initial split honors the persisted ratio (default 0.5 == 50/50): the
    // splitter scales whatever sizes it is given to its real width, so a ratio
    // of 0.8 becomes 880/220 of the ~1100 px window instead of equal halves.
    const double initialRatio = m_settings.splitterRatio();
    m_splitter->setSizes({qRound(1100.0 * initialRatio),
                          qRound(1100.0 * (1.0 - initialRatio))});
    setCentralWidget(m_splitter);

    // --- Find / Replace dialog (subtask 6; a popup since the post-loop
    // polish). ------------------------------------------------------------
    // A non-modal dialog OWNED by the main window and centered over it on every
    // show (it used to be an overlay glued under the menu bar); it drives the
    // editor's search helpers. Ctrl+F / Ctrl+H (Edit menu) show it.
    m_findBar = new FindBar(m_editor, this);

    // Every action the tests/automation need to reach gets a stable
    // objectName (text and platform key sequences differ per platform: on a bare
    // CI runner QKeySequence::Quit is empty, which made a shortcut-based lookup
    // grab the wrong action).
    // --- Menu bar. -----------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->setObjectName(QStringLiteral("menu.file"));
    m_newAct = fileMenu->addAction(tr("&New"), QKeySequence::New);
    m_newAct->setObjectName(QStringLiteral("action.new"));
    m_openAct = fileMenu->addAction(tr("&Open..."), QKeySequence::Open);
    m_openAct->setObjectName(QStringLiteral("action.open"));
    m_saveAct = fileMenu->addAction(tr("&Save"), QKeySequence::Save);
    m_saveAct->setObjectName(QStringLiteral("action.save"));
    m_saveAsAct = fileMenu->addAction(tr("Save &As..."), QKeySequence::SaveAs);
    m_saveAsAct->setObjectName(QStringLiteral("action.saveAs"));
    QMenu *exportMenu = fileMenu->addMenu(tr("&Export"));
    m_exportHtmlAct = exportMenu->addAction(tr("Export &HTML..."));
    m_exportPdfAct = exportMenu->addAction(tr("Export &PDF..."));
    // File > Recent Files (subtask 9): the last N persisted opened/saved
    // paths, most-recent-first, reopened through openFile() (dirty guard).
    // Built from the persisted list right now (may already be populated from
    // a previous run); empty until the first open/save (a hidden placeholder).
    m_recentMenu = fileMenu->addMenu(tr("Recent &Files"));
    fileMenu->addSeparator();
    m_exitAct = fileMenu->addAction(tr("E&xit"), QKeySequence::Quit);
    m_exitAct->setObjectName(QStringLiteral("action.exit"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->setObjectName(QStringLiteral("menu.edit"));
    // Undo / Redo (Ctrl+Z / Ctrl+Shift+Z, + Ctrl+Y). The actions are parented to
    // the EDITOR and shortcut-scoped to it, so the keys only reach the document
    // while the editor has focus (the find bar's fields keep their own native
    // undo) while the Edit menu still shows both entries and their keys.
    m_undoAct = new QAction(tr("&Undo"), m_editor);
    m_undoAct->setShortcut(QKeySequence::Undo);
    m_undoAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_redoAct = new QAction(tr("&Redo"), m_editor);
    m_redoAct->setShortcuts({QKeySequence::Redo, QKeySequence(Qt::CTRL | Qt::Key_Y)});
    m_redoAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    editMenu->addAction(m_undoAct);
    editMenu->addAction(m_redoAct);
    editMenu->addSeparator();
    m_findAct = editMenu->addAction(tr("&Find..."), QKeySequence::Find);
    m_findAct->setObjectName(QStringLiteral("action.find"));
    m_replaceAct = editMenu->addAction(tr("&Replace..."), QKeySequence(Qt::CTRL | Qt::Key_H));
    m_replaceAct->setObjectName(QStringLiteral("action.replace"));

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->setObjectName(QStringLiteral("menu.view"));
    m_previewToggleAct = viewMenu->addAction(tr("&Preview"));
    m_previewToggleAct->setCheckable(true);
    m_previewToggleAct->setChecked(true);
    m_previewToggleAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    viewMenu->addSeparator();

    // Quick toggle (Ctrl+T): flip between Light and Dark regardless of the
    // current accent. The menu-bar corner button shares this action.
    m_themeAct = viewMenu->addAction(tr("Toggle light/&dark"));
    m_themeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));

    // --- the theming system: Theme (mode) / Accent / UI Scale ----------------
    // Radio groups, so the menus always show what is active. The corner button's
    // popup reuses these very actions (a QMenu can only be a submenu in one
    // place, but a QAction may live in many menus).
    m_themeMenu = viewMenu->addMenu(tr("T&heme"));
    auto *modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);
    const QVector<QPair<Settings::ThemeMode, QString>> modes{
        {Settings::ThemeMode::Auto, tr("&System")},
        {Settings::ThemeMode::Light, tr("&Light")},
        {Settings::ThemeMode::Dark, tr("&Dark")}};
    for (const auto &entry : modes) {
        QAction *act = m_themeMenu->addAction(entry.second);
        act->setCheckable(true);
        modeGroup->addAction(act);
        act->setData(int(entry.first));
        m_themeModeActs.append(act);
        connect(act, &QAction::triggered, this,
                [this, mode = entry.first]() { setThemeMode(mode); });
    }

    m_accentMenu = viewMenu->addMenu(tr("&Accent"));
    auto *accentGroup = new QActionGroup(this);
    accentGroup->setExclusive(true);
    for (const QString &accent : Theme::accents()) {
        QAction *act = m_accentMenu->addAction(Theme::accentTitleFor(accent));
        act->setCheckable(true);
        accentGroup->addAction(act);
        act->setData(accent);
        m_accentActs.append(act);
        connect(act, &QAction::triggered, this,
                [this, accent]() { setThemeAccent(accent); });
    }

    m_uiScaleMenu = viewMenu->addMenu(tr("&UI Scale"));
    auto *scaleGroup = new QActionGroup(this);
    scaleGroup->setExclusive(true);
    for (const int percent : Theme::offeredScales()) {
        QAction *act = m_uiScaleMenu->addAction(tr("%1%").arg(percent));
        act->setCheckable(true);
        scaleGroup->addAction(act);
        act->setData(percent);
        m_scaleActs.append(act);
        connect(act, &QAction::triggered, this,
                [this, percent]() { setUiScale(percent); });
    }

    // The menu-bar corner twin: the quick light/dark toggle as an icon-only
    // sun/moon button pinned to the RIGHT-HAND CORNER of the menu bar (the menu
    // bar is the app's only chrome row — there is no toolbar). It carries no
    // shortcut of its own (Ctrl+T belongs to the menu entry, so the pair can
    // never be an ambiguous shortcut); its arrow opens the full theme popup.
    m_themeButtonAct = new QAction(this);
    m_themeButton = new QToolButton(menuBar());
    m_themeButton->setObjectName(QStringLiteral("ThemeButton"));
    m_themeButton->setDefaultAction(m_themeButtonAct); // shares text/tooltip/trigger
    m_themeButton->setAutoRaise(true); // flat until hovered, like the menu bar
    m_themeButton->setFocusPolicy(Qt::NoFocus);
    m_themeButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_themeOptionsMenu = buildThemeOptionsMenu();
    m_themeButton->setMenu(m_themeOptionsMenu);
    menuBar()->setCornerWidget(m_themeButton, Qt::TopRightCorner);
    updateThemeButton();

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->setObjectName(QStringLiteral("menu.help"));
    m_aboutAct = helpMenu->addAction(tr("&About mdit"));
    m_aboutAct->setObjectName(QStringLiteral("action.about"));

    // --- Menu bar only: no toolbar. -----------------------------------------
    // The app has exactly ONE row of chrome — the standard File/Edit/View/Help
    // menu bar, with the light/dark toggle in its right-hand corner (created
    // above). The old QToolBar duplicated the menus (New/Open/Save/Preview) in a
    // second row and has been removed; every one of those actions is still
    // reachable from the menus (File > New/Open/Save, View > Preview).

    // --- Status bar. ---------------------------------------------------------
    // Left: the transient message area ("Ready" / "Saved <name>" / ...). Right:
    // two PERMANENT readouts (subtask 8.2) — the modified indicator and the
    // word/char counts. addPermanentWidget() keeps them pinned to the right,
    // independent of whatever the transient message is showing.
    statusBar()->showMessage(tr("Ready"));
    m_countsLabel = new QLabel(this);
    m_modifiedLabel = new QLabel(this);
    m_countsLabel->setObjectName(QStringLiteral("StatusCounts"));
    m_modifiedLabel->setObjectName(QStringLiteral("StatusModified"));
    statusBar()->addPermanentWidget(m_modifiedLabel);
    statusBar()->addPermanentWidget(m_countsLabel);
    updateModifiedIndicator(); // initial state: a fresh document is clean
    refreshStatusCounts();     // ...and has 0 words / 0 characters

    // --- Wiring (skeleton-stage behavior). ----------------------------------
    connect(m_newAct, &QAction::triggered, this, &MainWindow::onNew);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::onOpen);
    connect(m_saveAct, &QAction::triggered, this, &MainWindow::onSave);
    connect(m_saveAsAct, &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(m_exportHtmlAct, &QAction::triggered, this, &MainWindow::onExportHtml);
    connect(m_exportPdfAct, &QAction::triggered, this, &MainWindow::onExportPdf);
    connect(m_findAct, &QAction::triggered, this, &MainWindow::onFind);
    connect(m_undoAct, &QAction::triggered, this, &MainWindow::onUndo);
    connect(m_redoAct, &QAction::triggered, this, &MainWindow::onRedo);
    connect(m_replaceAct, &QAction::triggered, this, &MainWindow::onReplace);
    connect(m_previewToggleAct, &QAction::triggered, this, &MainWindow::onTogglePreview);
    connect(m_themeAct, &QAction::triggered, this, &MainWindow::onToggleTheme);
    connect(m_themeButtonAct, &QAction::triggered, this, &MainWindow::onToggleTheme);
    connect(m_exitAct, &QAction::triggered, this, &QWidget::close);
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    // Typing marks the document dirty through the model and refreshes the title.
    connect(m_editor, &QPlainTextEdit::textChanged, this,
            &MainWindow::onEditorTextChanged);
    // ...and, while the find bar is open, keeps its match count current (the
    // bar's refreshCount() is a count-only no-op for the selection).
    connect(m_editor, &QPlainTextEdit::textChanged, this,
            &MainWindow::refreshFindBarCount);
    // The Edit > Undo/Redo entries are enabled only while the editor's document
    // actually has something to undo/redo (a fresh document has neither).
    connect(m_editor, &QPlainTextEdit::undoAvailable, this,
            [this](bool) { updateUndoRedoActions(); });
    connect(m_editor, &QPlainTextEdit::redoAvailable, this,
            [this](bool) { updateUndoRedoActions(); });
    updateUndoRedoActions();

    // --- Live preview: a singleShot debounce timer (subtask 4.2). -----------
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(m_debounceMs);
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::onPreviewTimerTimeout);

    // --- Editor<->preview scroll-ratio sync (best-effort, feedback-guarded).
    // Each pane's scroll moves the other to the same fraction; m_syncingScroll
    // stops the re-entrant signal from driving it straight back.
    if (QScrollBar *esb = m_editor->verticalScrollBar())
        connect(esb, &QScrollBar::valueChanged, this, &MainWindow::onEditorScrolled);
    connect(m_preview, &PreviewPane::scrollRatioChanged, this, &MainWindow::onPreviewScrolled);

    // --- Splitter-ratio persistence (subtask 8). ---------------------------
    // Dragging the handle persists the new editor fraction so the next run
    // (and a re-shown preview) restores it.
    connect(m_splitter, &QSplitter::splitterMoved, this,
            &MainWindow::persistSplitterRatio);

    // --- Persisted preview visibility (default ON per spec). ---------------
    // A hidden preview survives an app restart, with the menu/toolbar action
    // reflecting it. This runs AFTER the actions exist (m_previewToggleAct is
    // created in the menu-bar section above — do not move it earlier).
    if (!m_settings.previewVisible()) {
        m_preview->hide();
        m_previewToggleAct->setChecked(false);
    }

    // --- Recent Files (subtask 9): seed the submenu from the persisted list. -
    rebuildRecentMenu();
}

double MainWindow::splitterRatio() const
{
    const QList<int> sizes = m_splitter->sizes();
    int total = 0;
    for (int s : sizes)
        total += s;
    if (total <= 0 || sizes.isEmpty())
        return 0.5;
    return static_cast<double>(sizes.first()) / static_cast<double>(total);
}

void MainWindow::setSplitterRatio(double ratio)
{
    // Clamp into the non-collapsible range (0.0/1.0 would fully collapse the
    // other pane; QSplitter would fight that with the widgets' minimum sizes).
    ratio = qBound(0.05, ratio, 0.95);
    m_settings.setSplitterRatio(ratio);
    m_settings.sync();
    // The splitter has no width to scale the ratio into until it is laid out
    // (offscreen, before the first show) — remember it and apply on resize.
    int total = 0;
    for (int s : m_splitter->sizes())
        total += s;
    if (total <= 0) {
        m_pendingSplitterRatio = true;
        return;
    }
    m_splitter->setSizes({qRound(total * ratio), total - qRound(total * ratio)});
}

void MainWindow::persistSplitterRatio()
{
    // Only persist while both panes are laid out: a hidden preview makes
    // sizes() a degenerate one-widget list whose "ratio" (~1.0) is not the
    // user's remembered split (the real one was persisted before the hide).
    if (!isPreviewVisible())
        return;
    m_settings.setSplitterRatio(splitterRatio());
    m_settings.sync();
}

void MainWindow::applyPersistedRatio()
{
    int total = 0;
    for (int s : m_splitter->sizes())
        total += s;
    if (total <= 0)
        return;
    const double ratio = m_settings.splitterRatio(); // default 0.5
    m_splitter->setSizes({qRound(total * ratio), total - qRound(total * ratio)});
}

bool MainWindow::openFile(const QString &path)
{
    // Dirty guard first: never discard unsaved changes without asking.
    if (!confirmDiscard())
        return false;

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        newDocumentNoGuard();
        return false;
    }

    // The Document model does the streaming read + CRLF->LF normalization and
    // remembers the source line ending for a round-trip save.
    if (!m_doc.load(info.absoluteFilePath())) {
        newDocumentNoGuard();
        return false;
    }

    m_updating = true; // don't let the programmatic setPlainText re-mark dirty
    m_editor->setPlainText(m_doc.text());
    m_updating = false;
    // The loaded content is the clean checkpoint: undoing every subsequent edit
    // must land back on "unmodified" (no '*' in the title). setPlainText() has
    // already dropped the previous document's undo stack (Qt contract), so the
    // freshly loaded file is not undoable back into it.
    m_editor->markClean();

    // Render the loaded content into the preview immediately (the debounced
    // path handles subsequent typing). updateLivePreview() derives the base URL
    // from currentFilePath so relative images resolve against this directory.
    updateLivePreview();
    refreshStatusCounts(); // show the loaded file's counts (not the stale 0/0)
    updateTitle();
    // Show the OPENED PATH (not just the file name) in the status bar so the
    // user can see exactly which file the entry point (CLI / drag-and-drop)
    // loaded (subtask 10.1: "update the window/status to show the opened path").
    showStatus(tr("Opened %1").arg(info.absoluteFilePath()));
    addRecentFile(info.absoluteFilePath()); // File > Recent Files (subtask 9)
    return true;
}

void MainWindow::newDocument()
{
    if (!confirmDiscard())
        return;
    newDocumentNoGuard();
}

void MainWindow::newDocumentNoGuard()
{
    m_doc.setCurrentFilePath(QString()); // empty == untitled
    m_doc.setUntitledName(m_doc.nextUntitled());
    m_doc.setText(QString());
    m_doc.setDirty(false);

    m_updating = true;
    m_editor->clear();
    m_preview->clear();
    m_updating = false;
    m_editor->markClean(); // a blank document has nothing to undo and is clean

    refreshStatusCounts(); // back to 0 words / 0 characters
    updateTitle();
    showStatus(tr("New document"));
}

bool MainWindow::confirmDiscard()
{
    if (!m_doc.dirty())
        return true;
    switch (askDiscardChoice()) {
    case DiscardChoice::Discard:
        return true;
    case DiscardChoice::Save:
        // Same routing as Save (Ctrl+S): an untitled document has no path yet,
        // so "Save" means Save As (and the window closes only if it worked).
        return m_doc.currentFilePath().isEmpty() ? saveAsDocument()
                                                 : saveDocument();
    case DiscardChoice::Cancel:
    default:
        return false;
    }
}

MainWindow::DiscardChoice MainWindow::askDiscardChoice()
{
    // Real, modal Save/Discard/Cancel prompt. Virtual + headless-safe to
    // override: tests script the answer instead of driving a real dialog.
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("mdit"));
    box.setText(tr("The current document has unsaved changes."));
    box.setInformativeText(tr("Do you want to save your changes before discarding?"));
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Save);
    switch (box.exec()) {
    case QMessageBox::Save:
        return DiscardChoice::Save;
    case QMessageBox::Discard:
        return DiscardChoice::Discard;
    default:
        return DiscardChoice::Cancel;
    }
}

bool MainWindow::saveDocument()
{
    // Sync editor -> model, then write via the Document model (UTF-8, remembered
    // line ending, clears dirty on success). An untitled document has no path
    // to write to — onSave() routes those to Save As instead.
    m_doc.setText(m_editor->toPlainText());
    if (m_doc.currentFilePath().isEmpty())
        return false;
    if (!m_doc.save()) {
        showStatus(tr("Could not save %1").arg(
            QFileInfo(m_doc.currentFilePath()).fileName()));
        return false;
    }
    // What is on disk is the new clean checkpoint for the undo-aware dirty flag.
    m_editor->markClean();
    updateTitle(); // drops the dirty '*'
    showStatus(tr("Saved %1").arg(QFileInfo(m_doc.currentFilePath()).fileName()));
    addRecentFile(m_doc.currentFilePath()); // saved files join Recent Files
    return true;
}

bool MainWindow::saveAsDocument()
{
    // The (scriptable) dialog seam returns the chosen path, or empty on cancel.
    const QString path = askSaveAsPath(suggestedSaveName());
    if (path.isEmpty())
        return false; // canceled: nothing is written, state untouched

    // Sync editor -> model, then write via the Document model. Saving with an
    // explicit path updates currentFilePath and clears dirty on success.
    m_doc.setText(m_editor->toPlainText());
    if (!m_doc.save(path)) {
        showStatus(tr("Could not save %1").arg(QFileInfo(path).fileName()));
        return false;
    }
    m_editor->markClean(); // the just-written file is the clean checkpoint
    updateTitle(); // now the new file name, without the dirty '*'
    // The base URL (relative image resolution) follows the file's directory,
    // so re-render once it has moved.
    updateLivePreview();
    showStatus(tr("Saved %1").arg(QFileInfo(path).fileName()));
    addRecentFile(path); // saved files join Recent Files
    return true;
}

QString MainWindow::suggestedSaveName() const
{
    // Default the suggested name/extension from the current document (spec §6):
    // the existing file name when saved, otherwise the untitled name with a
    // .md extension so the dialog pre-fills a sensible markdown file name.
    const QString path = m_doc.currentFilePath();
    if (!path.isEmpty())
        return QFileInfo(path).fileName();
    return m_doc.title() + QStringLiteral(".md");
}

QString MainWindow::askSaveAsPath(const QString &suggestedName)
{
    // Real modal Save dialog, filtered to markdown and pre-filled with the
    // suggested name. Virtual + headless-safe to override: tests script the
    // chosen path instead of driving a real dialog (askDiscardChoice pattern).
    const QString filter = tr("Markdown files (*.md *.markdown);;All files (*)");
    // NOTE: QFileDialog::toNativeSeparators is absent from this Qt build, so
    // the (already slash-free) suggested name is passed through as-is.
    return QFileDialog::getSaveFileName(this, tr("Save Markdown File"),
                                        suggestedName, filter);
}

void MainWindow::handleDroppedPaths(const QStringList &paths)
{
    // Single-file focus: open the first dropped local .md / .markdown path and
    // ignore everything else.
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.isFile())
            continue;
        const QString suffix = info.suffix().toLower();
        if (suffix != QStringLiteral("md") && suffix != QStringLiteral("markdown"))
            continue;
        openFile(path);
        return;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls)
        if (url.isLocalFile())
            paths << url.toLocalFile();
    handleDroppedPaths(paths);
    event->acceptProposedAction();
}

bool MainWindow::isPreviewVisible() const
{
    // isHidden() reflects the explicit show/hide toggle and is true only when we
    // deliberately hid the pane (independent of whether the top-level window has
    // been shown yet), which is what the preview-toggle feature cares about.
    return !m_preview->isHidden();
}

bool MainWindow::togglePreview()
{
    const bool nowVisible = m_preview->isHidden(); // currently hidden -> will show
    m_preview->setVisible(nowVisible);
    m_previewToggleAct->setChecked(nowVisible);
    m_settings.setPreviewVisible(nowVisible); // persist for the next start
    m_settings.sync();
    // When re-shown, restore the PERSISTED ratio (splitterMoved / setSplitterRatio
    // kept it current; default 0.5) and render the current content so the
    // preview is not left stale from while it was hidden (typing did not
    // re-render it while hidden). While hidden the editor holds the full width
    // automatically — QSplitter gives the spare space to the visible pane.
    if (nowVisible) {
        applyPersistedRatio();
        updateLivePreview();
    }
    return nowVisible;
}

QString MainWindow::titleFor(bool dirty) const
{
    QString base = QStringLiteral("mdit");
    const QString path = m_doc.currentFilePath();
    if (!path.isEmpty())
        base = QFileInfo(path).fileName() + QLatin1Char(' ') + base;
    return (dirty ? QStringLiteral("*") : QString()) + base;
}

// -- the theming system (mode x accent x UI scale) ---------------------------

QMenu *MainWindow::buildThemeOptionsMenu()
{
    // The corner button's popup: the SAME actions the View submenus hold, laid
    // out flat (mode / accent / UI scale) so one menu covers everything.
    auto *menu = new QMenu(this);
    for (QAction *act : std::as_const(m_themeModeActs))
        menu->addAction(act);
    menu->addSeparator();
    for (QAction *act : std::as_const(m_accentActs))
        menu->addAction(act);
    menu->addSeparator();
    for (QAction *act : std::as_const(m_scaleActs))
        menu->addAction(act);
    return menu;
}

void MainWindow::setThemeMode(Settings::ThemeMode mode)
{
    m_themeMode = mode;
    m_settings.setTheme(mode); // persisted (System/Light/Dark)
    m_settings.sync();
    applyTheme();
}

void MainWindow::setThemeAccent(const QString &accent)
{
    m_accent = Theme::isKnownAccent(accent) ? accent : Theme::defaultAccent();
    m_settings.setThemeAccent(m_accent);
    m_settings.sync();
    applyTheme();
}

void MainWindow::setUiScale(int percent)
{
    m_uiScale = Theme::clampScale(percent);
    m_settings.setUiScale(m_uiScale);
    m_settings.sync();
    applyTheme();
}

void MainWindow::applyTheme()
{
    // Resolve the palette (System follows the platform) and push it everywhere:
    // the application (Fusion + palette + QSS + scaled application font), the
    // editor (scaled monospace font + accent-aware highlighter) and the preview
    // (the theme's document tokens — what the exports use too).
    m_theme = Theme::make(m_themeMode, m_accent, Theme::isSystemDark(*qApp));
    if (QApplication *app = qApp)
        Theme::apply(*app, m_theme, m_uiScale);
    if (m_editor)
        m_editor->setTheme(m_theme, m_uiScale);
    updateLivePreview();
    updateThemeButton();
    syncThemeActions();
}

void MainWindow::syncThemeActions()
{
    for (QAction *act : std::as_const(m_themeModeActs))
        act->setChecked(act->data().toInt() == int(m_themeMode));
    for (QAction *act : std::as_const(m_accentActs))
        act->setChecked(act->data().toString() == m_accent);
    for (QAction *act : std::as_const(m_scaleActs))
        act->setChecked(act->data().toInt() == m_uiScale);
    if (m_themeAct)
        m_themeAct->setChecked(m_theme.dark);
}

QAction *MainWindow::themeModeAction(Settings::ThemeMode mode) const
{
    for (QAction *act : m_themeModeActs) {
        if (act->data().toInt() == int(mode))
            return act;
    }
    return nullptr;
}

QAction *MainWindow::accentAction(const QString &accent) const
{
    for (QAction *act : m_accentActs) {
        if (act->data().toString() == accent)
            return act;
    }
    return nullptr;
}

QAction *MainWindow::uiScaleAction(int percent) const
{
    for (QAction *act : m_scaleActs) {
        if (act->data().toInt() == percent)
            return act;
    }
    return nullptr;
}

void MainWindow::updateTitle()
{
    // Reflect the live dirty state (title carries '*' while there are unsaved
    // changes) sourced from the Document model, and keep the status-bar
    // modified indicator in lockstep (it is O(1) — just reading the flag — so
    // it is refreshed here on every dirty-affecting change: typing, open, new,
    // save and save-as).
    setWindowTitle(titleFor(m_doc.dirty()));
    updateModifiedIndicator();
}

void MainWindow::refreshStatusCounts()
{
    // Word/char readout from the editor (== the MarkdownModel canonical counts).
    // Deliberately deferred to the debounce / open / new rather than run on
    // every keystroke — counting a large file is O(n) work the spec keeps off
    // the per-keystroke path.
    if (!m_countsLabel)
        return;
    const int words = m_editor->wordCount();
    const int chars = m_editor->charCount();
    m_countsLabel->setText(
        QStringLiteral("%1 %2, %3 %4")
            .arg(words)
            .arg(words == 1 ? tr("word") : tr("words"))
            .arg(chars)
            .arg(chars == 1 ? tr("character") : tr("characters")));
}

void MainWindow::updateModifiedIndicator()
{
    if (!m_modifiedLabel)
        return;
    m_modifiedLabel->setText(m_doc.dirty() ? tr("Modified") : tr("Unmodified"));
}

void MainWindow::onEditorTextChanged()
{
    if (m_updating)
        return; // programmatic update (open/new) — the model already holds the text
    m_doc.setText(m_editor->toPlainText()); // marks dirty only when it changed
    // An undo/redo that lands exactly on the last clean checkpoint (the loaded
    // file / the last save) is not a modification any more: mirror the editor
    // document's modified flag so the title's '*' and the status-bar indicator
    // go back to clean. A redo past the checkpoint leaves it modified, which the
    // setText() above already recorded as dirty.
    if (m_doc.dirty() && !m_editor->isModified())
        m_doc.setDirty(false);
    updateTitle();
    schedulePreviewUpdate(); // debounced live-preview re-render (only if visible)
}

void MainWindow::schedulePreviewUpdate()
{
    // The debounce timer is no longer gated on preview visibility. It now drives
    // TWO things on timeout: the (cheap) status-bar word/char counts, which must
    // stay current even while the preview is hidden, and the live-preview render
    // (the expensive part), which is guarded INSIDE updateLivePreview() (a no-op
    // while hidden). So a hidden preview still costs no render time — only the
    // O(n) count refresh runs on the timer — while the counts never go stale.
    m_editClock.restart();   // time zero at this change
    m_previewTimer->start(); // singleShot; restarted by every further change
}

void MainWindow::onPreviewTimerTimeout()
{
    // The status-bar word/char readout refreshes on the debounce (spec §3:
    // "Word/char counts in the status bar update on the same debounce"),
    // whether or not the preview is visible — the render below is the guarded
    // part, not the count.
    refreshStatusCounts();
    // Run the elapsed time through the pure policy so the render decision is
    // testable logic, not an opaque timer. A singleShot restarted on each change
    // always fires at/after the interval, so renderNeeded is true here.
    const PreviewDebouncer::Decision d =
        PreviewDebouncer::evaluate(m_editClock.elapsed(), m_debounceMs);
    if (d.renderNeeded)
        updateLivePreview(); // no-op while the pane is hidden (guarded inside)
    else // belt & braces: re-arm for the remaining wait (never hit in practice)
        m_previewTimer->start(d.waitMs);
}

void MainWindow::updateLivePreview()
{
    if (!isPreviewVisible())
        return;
    // Base URL = the document file's directory (a file:// URL), so relative
    // image paths resolve; untitled documents render with no base URL (absolute
    // file:// and data: references still work).
    QUrl base;
    const QString path = m_doc.currentFilePath();
    if (!path.isEmpty())
        base = QUrl::fromLocalFile(QFileInfo(path).absolutePath() + QStringLiteral("/"));
    m_preview->setBaseUrl(base);
    m_preview->setRendered(m_doc.text(), base, m_theme);
}

void MainWindow::setPreviewDebounceMs(int ms)
{
    m_debounceMs = qMax(0, ms);
    if (m_previewTimer)
        m_previewTimer->setInterval(m_debounceMs);
}

// --- Editor<->preview scroll sync (best-effort, feedback-guarded). ----------

void MainWindow::onEditorScrolled()
{
    if (m_syncingScroll)
        return; // we are the one driving the preview; ignore its echo
    m_syncingScroll = true;
    m_preview->setScrollRatio(m_editor->scrollRatio());
    m_syncingScroll = false;
}

void MainWindow::onPreviewScrolled(double ratio)
{
    if (m_syncingScroll)
        return; // we are the one driving the editor; ignore its echo
    m_syncingScroll = true;
    m_editor->setScrollRatio(ratio);
    m_syncingScroll = false;
}
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // A ratio requested before the splitter had a width (setSplitterRatio on a
    // never-shown window) is now due: the splitter is laid out, so apply it.
    // The value was already persisted by setSplitterRatio, so the same helper
    // re-applies it from Settings.
    if (m_pendingSplitterRatio) {
        m_pendingSplitterRatio = false;
        applyPersistedRatio();
    }
}

void MainWindow::showStatus(const QString &msg)
{
    statusBar()->showMessage(msg);
}

// --- Recent Files (subtask 9). ----------------------------------------------

void MainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    m_settings.addRecentFile(path); // most-recent-first, dedupe, cap
    m_settings.sync();
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu)
        return;
    // Rebuild the menu from scratch: drop any existing entry actions, then add
    // one enabled action per persisted path (most-recent-first). Each entry
    // stores its path in the action's data and reopens through openFile()
    // (which applies the dirty guard). The whole menu is hidden when there is
    // nothing to show.
    qDeleteAll(m_recentMenu->actions());
    const QStringList recent = m_settings.recentFiles();
    for (const QString &path : recent) {
        QAction *a = m_recentMenu->addAction(path);
        a->setData(path);
        connect(a, &QAction::triggered, this, &MainWindow::onOpenRecent);
    }
    if (recent.isEmpty()) {
        // A non-interactive placeholder so the (visible) menu is not empty.
        QAction *a = m_recentMenu->addAction(tr("No recent files"));
        a->setEnabled(false);
        m_recentMenu->setEnabled(false); // a disabled "Recent Files" menu item
    } else {
        m_recentMenu->setEnabled(true);
    }
}

void MainWindow::onOpenRecent()
{
    // The clicked entry: read its path from the sender's action data and
    // reopen through openFile() — the dirty guard runs there first.
    QAction *a = qobject_cast<QAction *>(sender());
    if (!a)
        return;
    const QString path = a->data().toString();
    if (!path.isEmpty())
        openFile(path);
}

// --- Slots. ------------------------------------------------------------------

void MainWindow::onNew()
{
    newDocument(); // dirty-guarded
}

void MainWindow::onOpen()
{
    // Interactive open: a .md / .markdown file dialog. Headless tests drive
    // openFile() / handleDroppedPaths() directly instead of this dialog.
    const QString filter = tr("Markdown files (*.md *.markdown);;All files (*)");
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open Markdown File"), QString(), filter);
    if (path.isEmpty())
        return;
    openFile(path);
}

void MainWindow::onSave()
{
    // Spec §6: Save to the current path — but an untitled document has no
    // current path, so Save means Save As (prompt for a destination).
    if (m_doc.currentFilePath().isEmpty()) {
        saveAsDocument();
        return;
    }
    saveDocument();
}

void MainWindow::onSaveAs()
{
    saveAsDocument();
}

void MainWindow::onTogglePreview()
{
    togglePreview();
}

void MainWindow::onToggleTheme()
{
    // Flip between the two explicit modes (the accent is untouched). Toggling
    // also leaves System mode, exactly like PengyCPP's Light/Dark override.
    setThemeMode(m_theme.dark ? Settings::ThemeMode::Light
                              : Settings::ThemeMode::Dark);
}

void MainWindow::updateThemeButton()
{
    if (!m_themeButtonAct)
        return;
    // Sun while light, moon while dark (a bare emoji: no text, no checkable
    // state — the tooltip spells out the state, the click action and the
    // current accent, and the arrow opens the full theme menu).
    m_themeButtonAct->setText(m_theme.dark ? QStringLiteral("\U0001F319")
                                           : QStringLiteral("\u2600\uFE0F"));
    m_themeButtonAct->setToolTip(
        m_theme.dark
            ? tr("Dark theme (%1) — click for light, arrow for all themes")
                  .arg(m_theme.accentTitle)
            : tr("Light theme (%1) — click for dark, arrow for all themes")
                  .arg(m_theme.accentTitle));
}

void MainWindow::onFind()
{
    // Ctrl+F / Edit>Find: show (or refocus) the docked find bar.
    showFindBar();
}

void MainWindow::showFindBar()
{
    m_findBar->activate();
}

void MainWindow::refreshFindBarCount()
{
    // Keep the find bar's count current while the user edits with it open. This
    // is count-only — it never moves the editor selection or steals focus — so
    // it is safe to run on every editor textChanged.
    m_findBar->refreshCount();
}

void MainWindow::onReplace()
{
    // Ctrl+H / Edit>Replace: show the find bar with its replace row unfolded
    // (same bar as Ctrl+F — Replace acts on the current match, Replace All on
    // every match, both through the EditorPane helpers).
    m_findBar->activateReplace();
}

void MainWindow::onUndo()
{
    // The editor owns the undo stack; the action is only here so the Edit menu
    // can offer it (and label the key).
    m_editor->undo();
    updateUndoRedoActions();
}

void MainWindow::onRedo()
{
    m_editor->redo();
    updateUndoRedoActions();
}

void MainWindow::updateUndoRedoActions()
{
    if (m_undoAct)
        m_undoAct->setEnabled(m_editor->canUndo());
    if (m_redoAct)
        m_redoAct->setEnabled(m_editor->canRedo());
}

// --- Export HTML (subtask 9). ----------------------------------------------

// Undo the HTML entities Qt's toHtml() emits inside attribute values
// (an '&' in a file name comes back as "&amp;", etc.).
static QString unescapeHtmlAttribute(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    out.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    return out;
}

// Every `src="..."` of an <img> tag in a toHtml() fragment, de-duplicated,
// in document order.
static QStringList imageSourcesInHtml(const QString &html)
{
    static const QRegularExpression
        imgRe(QStringLiteral("<img[^>]*\\bsrc=([\"'])([^\"']+)\\1"));
    QStringList out;
    QRegularExpressionMatchIterator it = imgRe.globalMatch(html);
    while (it.hasNext()) {
        const QString src = unescapeHtmlAttribute(it.next().captured(2));
        if (!out.contains(src))
            out << src;
    }
    return out;
}

QString MainWindow::suggestedHtmlName() const
{
    // Default the suggested name from the document's stem (spec §10 / subtask
    // 9): "notes.md" -> "notes.html"; an untitled document exports as
    // "untitled.html" (or "untitled 1.html" ...).
    const QString path = m_doc.currentFilePath();
    if (!path.isEmpty())
        return QFileInfo(path).completeBaseName() + QStringLiteral(".html");
    return m_doc.title() + QStringLiteral(".html");
}

QString MainWindow::askExportHtmlPath(const QString &suggestedName)
{
    // Real modal Save dialog, filtered to HTML and pre-filled with the
    // suggested name. Virtual + headless-safe to override: tests script the
    // chosen path instead of driving a real dialog (askSaveAsPath pattern).
    const QString filter = tr("HTML files (*.html *.htm);;All files (*)");
    return QFileDialog::getSaveFileName(this, tr("Export HTML"),
                                        suggestedName, filter);
}

bool MainWindow::exportHtmlDocument()
{
    // The (scriptable) dialog seam returns the chosen path, or empty on cancel.
    const QString path = askExportHtmlPath(suggestedHtmlName());
    if (path.isEmpty())
        return false; // canceled: nothing is written, state untouched

    // Export the CURRENT editor content (unsaved edits included) through the
    // same renderer the preview uses — "what you preview is what you export".
    // setText() here is a no-op for the dirty flag in practice: onEditorText-
    // Changed already synced the editor into the model on every keystroke.
    m_doc.setText(m_editor->toPlainText());

    // Base URL = the document file's directory (relative images resolve),
    // exactly as updateLivePreview() derives it; untitled -> no base URL.
    QUrl base;
    const QString mdPath = m_doc.currentFilePath();
    const QString mdDir =
        mdPath.isEmpty() ? QString() : QFileInfo(mdPath).absolutePath();
    if (!mdDir.isEmpty())
        base = QUrl::fromLocalFile(mdDir + QStringLiteral("/"));

    QTextDocument doc;
    RenderedDocument::render(m_doc.text(), base, m_theme, doc);
    const QString fragment = doc.toHtml();
    const QString page = RenderedDocument::standaloneHtml(
        QFileInfo(path).completeBaseName(), fragment, m_theme);

    // Spec §10: relative image references are copied next to the saved HTML
    // (the same relative path is kept, so the exported file keeps working
    // relative to wherever it was saved). Schemes (file://, http://, data:,
    // qrc:) and absolute paths are left as-is; missing source files are
    // skipped (a broken link degrades to the alt text, like in the preview).
    const QString destDir = QFileInfo(path).absolutePath();
    for (const QString &src : imageSourcesInHtml(fragment)) {
        const bool scheme = src.contains(QLatin1Char(':')); // file:, http:, data:, C:\
        // NOTE: "absolute" is a Qt macro on this build — do not use it as an
        // identifier.
        const bool isAbsolute = src.startsWith(QLatin1Char('/'))
                                || src.startsWith(QLatin1Char('~'));
        if (scheme || isAbsolute || mdDir.isEmpty())
            continue;
        const QString rel = src.startsWith(QLatin1String("./"))
            ? src.mid(2) : src;
        const QString srcFile = mdDir + QLatin1Char('/') + rel;
        const QString destFile = destDir + QLatin1Char('/') + rel;
        if (!QFileInfo::exists(srcFile) || QFile::exists(destFile))
            continue;
        if (QDir().mkpath(QFileInfo(destFile).absolutePath())
            && QFile::copy(srcFile, destFile))
            continue; // copied (or a benign race: it already exists now)
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showStatus(tr("Could not export %1").arg(QFileInfo(path).fileName()));
        return false;
    }
    f.write(page.toUtf8());
    f.close();
    showStatus(tr("Exported %1").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::onExportHtml()
{
    exportHtmlDocument(); // File > Export > HTML... (dialog seam + status)
}

QString MainWindow::suggestedPdfName() const
{
    // Default the suggested name from the document's stem (mirrors the HTML
    // export): "notes.md" -> "notes.pdf"; untitled -> "untitled.pdf".
    const QString path = m_doc.currentFilePath();
    if (!path.isEmpty())
        return QFileInfo(path).completeBaseName() + QStringLiteral(".pdf");
    return m_doc.title() + QStringLiteral(".pdf");
}

QString MainWindow::askExportPdfPath(const QString &suggestedName)
{
    // Real modal Save dialog, filtered to PDF and pre-filled with the
    // suggested name. Virtual + headless-safe to override: tests script the
    // chosen path instead of driving a real dialog (askExportHtmlPath pattern).
    const QString filter = tr("PDF files (*.pdf);;All files (*)");
    return QFileDialog::getSaveFileName(this, tr("Export PDF"),
                                        suggestedName, filter);
}

bool MainWindow::exportPdfDocument()
{
    // The (scriptable) dialog seam returns the chosen path, or empty on cancel.
    const QString path = askExportPdfPath(suggestedPdfName());
    if (path.isEmpty())
        return false; // canceled: nothing is written, state untouched

    // Export the CURRENT editor content (unsaved edits included) through the
    // same renderer the preview / HTML export use. setText() here is a no-op
    // for the dirty flag in practice (onEditorTextChanged already synced it).
    m_doc.setText(m_editor->toPlainText());

    // Base URL = the document file's directory (relative image resources
    // resolve), exactly as updateLivePreview() derives it; untitled -> none.
    QUrl base;
    const QString mdPath = m_doc.currentFilePath();
    const QString mdDir =
        mdPath.isEmpty() ? QString() : QFileInfo(mdPath).absolutePath();
    if (!mdDir.isEmpty())
        base = QUrl::fromLocalFile(mdDir + QStringLiteral("/"));

    // A4 portrait at the configured DPI; RenderedDocument::renderPdf verifies
    // the %PDF magic header on success.
    if (!RenderedDocument::renderPdf(m_doc.text(), base, m_theme, path,
                                     QPageSize::A4, m_pdfResolutionDpi)) {
        showStatus(tr("Could not export %1").arg(QFileInfo(path).fileName()));
        return false;
    }
    showStatus(tr("Exported %1").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::onExportPdf()
{
    exportPdfDocument(); // File > Export > PDF... (dialog seam + status)
}

void MainWindow::onAbout()
{
    // A real About box (mark + name + version + tagline + license + Catbee
    // link), created once and re-shown; it centers itself over this window.
    if (!m_aboutDialog)
        m_aboutDialog = new AboutDialog(this);
    m_aboutDialog->showAbout();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Closing the window (the title-bar X, File > Exit / Ctrl+Q, or the session
    // manager asking to quit) must never silently drop unsaved changes: it runs
    // the same guard as New/Open — save, explicitly discard, or cancel (in which
    // case the window stays open).
    if (confirmDiscard())
        event->accept();
    else
        event->ignore();
}
