// test_mainwindow — subtask 8.1: the preview toggle + splitter-ratio persistence.
//
// Covers, without a real window (offscreen QApplication only):
//   * the preview is ON by default (pane visible, View-menu action checked);
//   * the View-menu "Preview" toggle action (checkable, Ctrl+Shift+P)
//     hides/shows the PreviewPane through its trigger;
//   * hiding the preview gives the editor the full splitter width;
//   * the toggle's new state is persisted (a fresh window on the same backing
//     starts hidden);
//   * the splitter ratio is persisted on change (setSplitterRatio writes the
//     value to the backing QSettings file, and the splitter is non-collapsible);
//   * re-showing the preview restores the PERSISTED ratio (not a reset to 50/50);
//   * startup restores the persisted ratio and visibility (fresh settings →
//     50/50 + preview ON, the spec defaults).
//
// Every window is constructed with an isolated temp-file QSettings backing so
// a user's real mdit settings are never read or written. Widget test → a
// custom main builds a QApplication; add_mdit_test() forces the offscreen
// platform plugin.
#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"
#include "PreviewPane.h"
#include "Settings.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QTemporaryDir>

static QString writeTemp(const QString &dir, const QString &name, const QByteArray &bytes)
{
    const QString path = dir + QLatin1Char('/') + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write(bytes);
    f.close();
    return path;
}

// A MainWindow that scripts the dirty-discard answer to Cancel, so any
// open/new that hits the guard is refused without a real modal dialog.
class NoDiscardWindow : public MainWindow
{
public:
    using MainWindow::MainWindow;
    DiscardChoice askDiscardChoice() override { return DiscardChoice::Cancel; }
};

class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    static QSettings tempSettings(const QString &dir, const QString &name)
    {
        return QSettings(dir + QStringLiteral("/mdit/") + name,
                         QSettings::IniFormat);
    }

    // The View-menu "Preview" toggle action (found by title, mnemonics dropped).
    static QAction *previewAction(MainWindow &w)
    {
        for (QMenu *m : w.menuBar()->findChildren<QMenu *>()) {
            if (m->title().remove(QLatin1Char('&')) != QLatin1String("View"))
                continue;
            for (QAction *a : m->actions()) {
                if (a->text().remove(QLatin1Char('&')) == QLatin1String("Preview"))
                    return a;
            }
        }
        return nullptr;
    }

    // Seed N recent markdown files on an isolated backing (simulating previous
    // runs) and return their absolute paths (most-recent-first, as stored).
    static QStringList seedRecent(QSettings &ini, const QString &dir, int n)
    {
        Settings s(&ini);
        QStringList paths;
        for (int i = 0; i < n; ++i)
            paths.append(dir + QStringLiteral("/recent%1.md").arg(i));
        for (const QString &p : paths) // most-recent (last) ends up first
            s.addRecentFile(p);
        s.sync();
        return s.recentFiles();
    }

private slots:
    void previewOnByDefault();
    void viewMenuToggleActionHidesAndShows();
    void togglePersistsVisibilityForNextStart();
    void editorTakesFullWidthWhenPreviewHidden();
    void splitterRatioPersistedOnChange();
    void reshownPreviewRestoresPersistedRatio();
    void startupRestoresPersistedRatioAndVisibility();
    void freshSettingsStartAtFiftyFifty();

    // --- Subtask 9.3: Recent Files (File submenu). -------------------------
    void recentMenuReflectsPersistedList();
    void recentMenuDisabledWhenEmpty();
    void openFileAddsToRecentFilesCapped();
    void saveAddsToRecentFiles();
    void recentEntryReopensFile();
    void recentReopenRespectsDirtyGuard();

    // --- Subtask 8.2: status-bar word/char counts + modified indicator. ----
    void statusBarCountsAndModifiedInitially();
    void countsRefreshOnDebounce_notPerKeystroke();
    void countsUpdateWhilePreviewHidden();
    void countsAfterOpenFile_immediate();
    void countsPluralization();
    void modifiedIndicator_followsDirty();
};

void TestMainWindow::previewOnByDefault()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("fresh.ini"));
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.isPreviewVisible());
        QCOMPARE(w.settings()->previewVisible(), true);
        QAction *act = previewAction(w);
        QVERIFY(act != nullptr);
        QVERIFY(act->isCheckable());
        QVERIFY(act->isChecked());
    }
}

void TestMainWindow::viewMenuToggleActionHidesAndShows()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("action.ini"));
    {
        MainWindow w(nullptr, &ini);
        QAction *act = previewAction(w);
        QVERIFY(act != nullptr);
        // The spec's exact key: Ctrl+Shift+P.
        QCOMPARE(act->shortcut(),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

        act->trigger();
        QVERIFY(!w.isPreviewVisible());
        QVERIFY(!act->isChecked());

        act->trigger();
        QVERIFY(w.isPreviewVisible());
        QVERIFY(act->isChecked());
    }
}

void TestMainWindow::togglePersistsVisibilityForNextStart()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("persist.ini"));
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(!w.togglePreview()); // ON -> OFF
        QVERIFY(!w.isPreviewVisible());
        // The action follows the pane state within the same live window.
        QAction *act = previewAction(w);
        QVERIFY(act != nullptr);
        QVERIFY(!act->isChecked());
    }
    // The toggle wrote the new state to the backing file...
    QSettings verify = tempSettings(dir.path(), QStringLiteral("persist.ini"));
    QCOMPARE(verify.value(QStringLiteral("previewVisible")).toBool(), false);
    // ...so a "restart" (a fresh window on the same backing) starts hidden.
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(!w.isPreviewVisible());
        QAction *act = previewAction(w);
        QVERIFY(act != nullptr);
        QVERIFY(!act->isChecked());
    }
}

void TestMainWindow::editorTakesFullWidthWhenPreviewHidden()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("width.ini"));
    {
        MainWindow w(nullptr, &ini);
        w.show(); // offscreen: give the splitter a real width
        QApplication::processEvents();
        QVERIFY(w.splitter()->width() > 0);

        QVERIFY(w.editorPane()->width() > 0); // laid out at ~50%
        QVERIFY(!w.togglePreview());
        QApplication::processEvents();

        // Hidden preview: the editor holds the full splitter width.
        QVERIFY(w.editorPane()->width() >= w.splitter()->width() - 10);

        QVERIFY(w.togglePreview());
        QApplication::processEvents();
        // Re-shown: both panes are laid out again.
        QVERIFY(w.editorPane()->width() > 0);
        QVERIFY(w.previewPane()->width() > 0);
    }
}

void TestMainWindow::splitterRatioPersistedOnChange()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("ratio.ini"));
    {
        MainWindow w(nullptr, &ini);
        w.show();
        QApplication::processEvents();

        w.setSplitterRatio(0.7);
        QApplication::processEvents();
        QVERIFY2(qAbs(w.splitterRatio() - 0.7) < 0.05,
                 qPrintable(QString("ratio %1").arg(w.splitterRatio())));

        // The change reached the backing file, not just memory.
        QSettings verify = tempSettings(dir.path(), QStringLiteral("ratio.ini"));
        QVERIFY2(qAbs(verify.value(QStringLiteral("splitterRatio")).toDouble() - 0.7)
                     < 0.05,
                 qPrintable(verify.value(QStringLiteral("splitterRatio")).toString()));
    }
}

void TestMainWindow::reshownPreviewRestoresPersistedRatio()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("restore.ini"));
    {
        MainWindow w(nullptr, &ini);
        w.show();
        QApplication::processEvents();

        w.setSplitterRatio(0.7); // persist a non-default split
        QApplication::processEvents();
        QVERIFY(!w.togglePreview()); // hide
        QApplication::processEvents();
        QVERIFY(w.togglePreview());  // re-show
        QApplication::processEvents();

        // The re-shown split is the persisted 0.7, NOT a reset to 50/50.
        QVERIFY2(qAbs(w.splitterRatio() - 0.7) < 0.05,
                 qPrintable(QString("ratio %1").arg(w.splitterRatio())));
    }
}

void TestMainWindow::startupRestoresPersistedRatioAndVisibility()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("seed.ini"));
    {
        // Simulate a previous run that hid the preview and widened the editor.
        Settings s(&ini);
        s.setSplitterRatio(0.8);
        s.setPreviewVisible(false);
        s.sync();
    }
    {
        MainWindow w(nullptr, &ini);
        w.show();
        QApplication::processEvents();
        QVERIFY(!w.isPreviewVisible()); // hidden persisted across "restart"
        // The persisted split is observable when the preview is re-shown: the
        // startup split was built from the persisted 0.8, and re-showing
        // restores it (not a 50/50 reset). (While hidden the splitter reports
        // a single-pane sizes() list, so the ratio is not measurable.)
        QVERIFY(w.togglePreview());
        QApplication::processEvents();
        QVERIFY2(qAbs(w.splitterRatio() - 0.8) < 0.05,
                 qPrintable(QString("ratio %1").arg(w.splitterRatio())));
    }
}

void TestMainWindow::freshSettingsStartAtFiftyFifty()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("defaults.ini"));
    {
        MainWindow w(nullptr, &ini);
        w.show();
        QApplication::processEvents();
        QVERIFY(w.isPreviewVisible()); // preview ON by default
        QVERIFY2(qAbs(w.splitterRatio() - 0.5) < 0.05,
                 qPrintable(QString("ratio %1").arg(w.splitterRatio())));
    }
}

// --- Subtask 9.3: Recent Files (File submenu). ------------------------------

// The File > "Recent Files" submenu lists the persisted paths, most-recent-
// first, and is enabled when there is at least one entry.
void TestMainWindow::recentMenuReflectsPersistedList()
{
    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("recent.ini"));
    const QStringList seeded = seedRecent(ini, dir.path(), 3);
    QVERIFY(seeded.size() == 3);

    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.recentFilesMenu() != nullptr);
        QVERIFY(w.recentFilesMenu()->isEnabled());
        QCOMPARE(w.recentFiles(), seeded);
        const QList<QAction *> acts = w.recentFilesMenu()->actions();
        QCOMPARE(acts.size(), 3);
        // Most-recent-first, in the same order as the persisted list.
        for (int i = 0; i < 3; ++i) {
            QVERIFY(acts.at(i)->isEnabled());
            QCOMPARE(acts.at(i)->text(), seeded.at(i));
        }
    }
}

// With an empty (fresh) settings, the Recent Files menu item is disabled and
// holds only a non-interactive placeholder.
void TestMainWindow::recentMenuDisabledWhenEmpty()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("recentempty.ini"));
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.recentFilesMenu() != nullptr);
        QVERIFY(!w.recentFilesMenu()->isEnabled()); // no entries -> disabled item
        QCOMPARE(w.recentFiles(), QStringList());
        const QList<QAction *> acts = w.recentFilesMenu()->actions();
        QCOMPARE(acts.size(), 1);
        QCOMPARE(acts.at(0)->text(), QStringLiteral("No recent files"));
        QVERIFY(!acts.at(0)->isEnabled());
    }
}

// openFile() records the opened path in the persisted recent list (most-recent
// first) and the list is capped at the default 5 — opening a 6th drops the
// oldest.
void TestMainWindow::openFileAddsToRecentFilesCapped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("recentopen.ini"));
    {
        MainWindow w(nullptr, &ini);
        QStringList paths;
        for (int i = 0; i < 6; ++i) {
            const QString p = writeTemp(dir.path(),
                                        QStringLiteral("f%1.md").arg(i),
                                        QStringLiteral("file %1\n").arg(i).toUtf8());
            QVERIFY(!p.isEmpty());
            QVERIFY(w.openFile(p));
            paths.append(p);
        }
        // Capped to the default 5; the oldest (f0) is dropped, most-recent first.
        QCOMPARE(w.recentFiles().size(), 5);
        QCOMPARE(w.recentFiles().first(), paths.last());
        QVERIFY(!w.recentFiles().contains(paths.first()));
    }
    // The list reached disk, so a fresh window on the same backing shows it.
    {
        MainWindow w(nullptr, &ini);
        QCOMPARE(w.recentFiles().size(), 5);
        QCOMPARE(w.recentFilesMenu()->actions().size(), 5);
    }
}

// Saving a document (Save and Save As) also joins the recent-files list.
void TestMainWindow::saveAddsToRecentFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("recentsave.ini"));
    const QString path = writeTemp(dir.path(), QStringLiteral("saveme.md"),
                                   "hello\n");
    QVERIFY(!path.isEmpty());
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.openFile(path));
        w.editorPane()->setPlainText(QStringLiteral("edited content\n"));
        QVERIFY(w.saveDocument()); // Save -> record the current path
        QVERIFY(w.recentFiles().contains(path));
    }
}

// Clicking a Recent Files entry reopens that file (via openFile, dirty-guarded).
void TestMainWindow::recentEntryReopensFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), QStringLiteral("reopen.md"),
                                   "# Reopen me\n");
    QVERIFY(!path.isEmpty());
    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("recentreopen.ini"));
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.openFile(path)); // populate the recent list
        // Switch away, then click the recent entry to reopen.
        w.newDocument();
        QCOMPARE(w.document()->title(), QStringLiteral("untitled"));

        QAction *entry = nullptr;
        for (QAction *a : w.recentFilesMenu()->actions())
            if (a->data().toString() == path)
                entry = a;
        QVERIFY(entry != nullptr);
        entry->trigger();
        QCOMPARE(w.document()->currentFilePath(), path);
        QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("# Reopen me\n"));
    }
}

// A Recent Files entry is dirty-guarded: with a dirty document and a Cancelled
// discard, clicking the entry does NOT swap in the other file (content + path
// stay intact). Both files are opened through this window first so b.md is in
// the live recent menu; then the current doc (a.md) is dirtied and the b.md
// entry is triggered — the NoDiscardWindow scripts the guard to Cancel.
void TestMainWindow::recentReopenRespectsDirtyGuard()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString a = writeTemp(dir.path(), QStringLiteral("a.md"), "file A\n");
    const QString b = writeTemp(dir.path(), QStringLiteral("b.md"), "file B\n");
    QVERIFY(!a.isEmpty() && !b.isEmpty());
    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("recentguard.ini"));
    {
        NoDiscardWindow w(nullptr, &ini);
        QVERIFY(w.openFile(b)); // record b.md in the recent list
        QVERIFY(w.openFile(a)); // current doc is now a.md (clean)
        w.editorPane()->setPlainText(QStringLiteral("unsaved edits to A\n"));
        QVERIFY(w.dirty());

        // Find the b.md entry in the live recent menu and trigger it.
        QAction *entry = nullptr;
        for (QAction *act : w.recentFilesMenu()->actions())
            if (act->data().toString() == b)
                entry = act;
        QVERIFY(entry != nullptr);
        entry->trigger(); // -> openFile(b) -> confirmDiscard -> Cancel

        // The guard refused: a.md is still current, content untouched, still dirty.
        QCOMPARE(w.document()->currentFilePath(), a);
        QCOMPARE(w.editorPane()->toPlainText(),
                 QStringLiteral("unsaved edits to A\n"));
        QVERIFY(w.dirty());
    }
}

// --- Subtask 8.2: status-bar word/char counts + modified indicator. ---------

// A fresh (untitled, empty) document shows 0 words / 0 characters and is not
// modified. The two permanent labels exist and are populated at construction.
void TestMainWindow::statusBarCountsAndModifiedInitially()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("sb0.ini"));
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.statusCountsLabel() != nullptr);
        QVERIFY(w.statusModifiedLabel() != nullptr);
        QCOMPARE(w.statusCountsLabel()->text(),
                 QStringLiteral("0 words, 0 characters"));
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Unmodified"));
    }
}

// The word/char count is DEBOUNCED, not per-keystroke: right after a change the
// label still holds its previous value (0/0); only after the debounce interval
// elapses does it reflect the new content. The modified indicator, by contrast,
// is O(1) and flips to "Modified" immediately on the first edit.
void TestMainWindow::countsRefreshOnDebounce_notPerKeystroke()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("sb1.ini"));
    {
        MainWindow w(nullptr, &ini);
        w.setPreviewDebounceMs(20);

        w.editorPane()->setPlainText(QStringLiteral("one two three four five"));

        // Immediately after the change the count readout is still its previous
        // value — the O(n) count is deferred to the debounce, not the keystroke.
        QCOMPARE(w.statusCountsLabel()->text(),
                 QStringLiteral("0 words, 0 characters"));
        // ...but the modified indicator is live (O(1)) and already flipped.
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Modified"));

        QTest::qWait(120); // let the singleShot debounce timer fire

        QVERIFY(w.editorPane()->wordCount() == 5);
        QCOMPARE(w.editorPane()->charCount(), 23); // 5 words + 4 spaces
        const QString expected = QStringLiteral("%1 words, %2 characters")
                                     .arg(w.editorPane()->wordCount())
                                     .arg(w.editorPane()->charCount());
        QCOMPARE(w.statusCountsLabel()->text(), expected);
    }
}

// The debounce now drives the counts independent of preview visibility: typing
// with the preview hidden still refreshes the count readout (the render is the
// guarded part, the count is not).
void TestMainWindow::countsUpdateWhilePreviewHidden()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("sb2.ini"));
    {
        MainWindow w(nullptr, &ini);
        w.setPreviewDebounceMs(20);
        QVERIFY(!w.togglePreview()); // hide the preview
        QVERIFY(!w.isPreviewVisible());

        w.editorPane()->setPlainText(QStringLiteral("hello world hello"));
        QTest::qWait(120);

        QCOMPARE(w.editorPane()->wordCount(), 3);
        QCOMPARE(w.statusCountsLabel()->text(),
                 QStringLiteral("3 words, 17 characters"));
    }
}

// openFile() refreshes the counts immediately (no debounce wait on load), so a
// freshly opened document shows its real word/char counts, not the stale 0/0.
void TestMainWindow::countsAfterOpenFile_immediate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(
        dir.path(), QStringLiteral("counted.md"),
        "# Heading\n\nhello **world** there\n");
    QVERIFY(!path.isEmpty());

    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("sb3.ini"));
    {
        MainWindow w(nullptr, &ini);
        QVERIFY(w.openFile(path));

        QVERIFY(w.editorPane()->wordCount() > 0);
        const QString expected = QStringLiteral("%1 words, %2 characters")
                                     .arg(w.editorPane()->wordCount())
                                     .arg(w.editorPane()->charCount());
        QCOMPARE(w.statusCountsLabel()->text(), expected);
        // A clean load is not modified.
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Unmodified"));
    }
}

// The count readout pluralizes correctly: a single word/character reads
// "1 word, 1 character", multiple read "words"/"characters". Forced synchronously
// through refreshStatusCounts() (public seam) so no debounce wait is needed.
void TestMainWindow::countsPluralization()
{
    const QTemporaryDir dir;
    QSettings ini = tempSettings(dir.path(), QStringLiteral("sb4.ini"));
    {
        MainWindow w(nullptr, &ini);

        w.editorPane()->setPlainText(QStringLiteral("abc"));
        w.refreshStatusCounts();
        QCOMPARE(w.statusCountsLabel()->text(),
                 QStringLiteral("1 word, 3 characters"));

        w.editorPane()->setPlainText(QStringLiteral("a"));
        w.refreshStatusCounts();
        QCOMPARE(w.statusCountsLabel()->text(),
                 QStringLiteral("1 word, 1 character"));

        w.editorPane()->setPlainText(QStringLiteral("a b"));
        w.refreshStatusCounts();
        QCOMPARE(w.statusCountsLabel()->text(),
                 QStringLiteral("2 words, 3 characters"));
    }
}

// The modified indicator tracks the Document dirty flag through the normal
// lifecycle, without any modal (no dirty-guard prompt is triggered):
//   clean (fresh) -> "Unmodified"
//   clean load    -> "Unmodified"
//   edit (dirty)  -> "Modified"   (immediately, O(1))
//   save (clean)  -> "Unmodified"
void TestMainWindow::modifiedIndicator_followsDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), QStringLiteral("mod.md"),
                                   "initial line\n");
    QVERIFY(!path.isEmpty());

    const QTemporaryDir sdir;
    QSettings ini = tempSettings(sdir.path(), QStringLiteral("sb5.ini"));
    {
        MainWindow w(nullptr, &ini);
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Unmodified"));

        QVERIFY(w.openFile(path)); // a clean load is not modified
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Unmodified"));

        w.editorPane()->setPlainText(QStringLiteral("edited content here"));
        // Live on the edit (no debounce wait for the indicator).
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Modified"));

        QVERIFY(w.saveDocument()); // write + clear dirty
        QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Unmodified"));
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestMainWindow t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_mainwindow.moc"
