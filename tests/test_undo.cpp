// test_undo — undo / redo (post-loop polish): "Ctrl+Z while editing".
//
// Headless (offscreen) coverage of the whole undo story:
//   * `EditorPane` — canUndo()/canRedo() follow the document's undo stack; a
//     real edit is undoable with the **Ctrl+Z key** (the shortcut the user
//     asked for) and redoable with Ctrl+Shift+Z; a programmatic load
//     (setPlainText, i.e. File > Open / New) does NOT leave the previous
//     document undoable;
//   * `EditorPane::replaceAll()` is **one** undo step — one Ctrl+Z restores the
//     entire sweep (it used to go through setPlainText() and wipe the stack),
//     and a no-match replace records no edit at all;
//   * `MainWindow` — the Edit menu offers **Undo (Ctrl+Z)** and **Redo
//     (Ctrl+Shift+Z / Ctrl+Y)**, enabled exactly while there is something to
//     undo/redo, and triggering them rewrites the document;
//   * the dirty flag is **undo-aware**: undoing back to the loaded/saved
//     content drops the '*' from the title and flips the status-bar indicator
//     to "Unmodified"; redo (or undoing past the last save) brings them back.
//
// Every undoable edit in these tests is a single `QTextCursor::insertText`
// command (via typeAtEnd()), so "one undo step" is deterministic regardless of
// how Qt groups consecutive keystrokes.
#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QTemporaryDir>
#include <QTextCursor>

namespace {

// Append `text` at the end of the buffer as ONE undoable edit.
void typeAtEnd(EditorPane &e, const QString &text)
{
    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::End);
    e.setTextCursor(c);
    e.insertPlainText(text);
}

QString writeTemp(const QString &dir, const QString &name, const QByteArray &bytes)
{
    const QString path = dir + QLatin1Char('/') + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write(bytes);
    f.close();
    return path;
}

// The menu bar's menu whose (accel-stripped) title is `title`.
QMenu *menuByTitle(MainWindow &w, const QString &title)
{
    for (QMenu *m : w.menuBar()->findChildren<QMenu *>())
        if (m->title().remove(QLatin1Char('&')) == title)
            return m;
    return nullptr;
}

QString plain(const QAction *a) { return a->text().remove(QLatin1Char('&')); }

} // namespace

class TestUndo : public QObject
{
    Q_OBJECT
private slots:
    // --- EditorPane ---------------------------------------------------------
    void canUndoAndCanRedoTrackTheStack();
    void ctrlZKeyUndoesTyping_andCtrlShiftZRedoes();
    void programmaticLoadIsNotUndoable();
    void replaceAllIsOneUndoStep();
    void blankReplaceAllRecordsNoUndoStep();

    // --- MainWindow: menu wiring + behavior ---------------------------------
    void editMenuOffersUndoAndRedoWithStandardKeys();
    void undoRedoActionsTrackAvailability();
    void undoActionRevertsAndRedoReapplies();

    // --- dirty flag is undo-aware -------------------------------------------
    void undoBackToLoadedContentClearsDirty();
    void saveResetsTheCheckpoint();
};

// canUndo()/canRedo() mirror the document's undo stack: empty on a fresh pane,
// available after an edit, swapped by undo (and back by redo).
void TestUndo::canUndoAndCanRedoTrackTheStack()
{
    EditorPane e;
    QVERIFY(!e.canUndo());
    QVERIFY(!e.canRedo());

    e.setPlainText(QStringLiteral("abc")); // programmatic: no undo history
    QVERIFY(!e.canUndo());
    QVERIFY(!e.canRedo());

    typeAtEnd(e, QStringLiteral("def"));
    QCOMPARE(e.toPlainText(), QStringLiteral("abcdef"));
    QVERIFY(e.canUndo());
    QVERIFY(!e.canRedo());

    e.undo();
    QCOMPARE(e.toPlainText(), QStringLiteral("abc"));
    QVERIFY(!e.canUndo());
    QVERIFY(e.canRedo());

    e.redo();
    QCOMPARE(e.toPlainText(), QStringLiteral("abcdef"));
    QVERIFY(e.canUndo());
    QVERIFY(!e.canRedo());
}

// The shortcut the user asked for: pressing Ctrl+Z in the editor undoes the
// last edit (Ctrl+Shift+Z redoes it).
void TestUndo::ctrlZKeyUndoesTyping_andCtrlShiftZRedoes()
{
    EditorPane e;
    e.show(); // offscreen: give the pane a window so the key is delivered
    e.setFocus();
    e.setPlainText(QStringLiteral("abc"));
    typeAtEnd(e, QStringLiteral("def"));
    QCOMPARE(e.toPlainText(), QStringLiteral("abcdef"));

    QTest::keyClick(&e, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(e.toPlainText(), QStringLiteral("abc"));

    QTest::keyClick(&e, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(e.toPlainText(), QStringLiteral("abcdef"));

    // Ctrl+Y is the alternate redo binding.
    QTest::keyClick(&e, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(e.toPlainText(), QStringLiteral("abc"));
    QTest::keyClick(&e, Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(e.toPlainText(), QStringLiteral("abcdef"));

    // ONE Ctrl+Z undoes exactly one step: the Edit>Undo action and the widget's
    // own built-in undo handling must not both fire. (The step count is measured
    // first, so this holds however Qt groups the insertions.)
    EditorPane e2;
    e2.show();
    e2.setFocus();
    typeAtEnd(e2, QStringLiteral("one"));
    typeAtEnd(e2, QStringLiteral(" two"));
    QCOMPARE(e2.toPlainText(), QStringLiteral("one two"));
    int steps = 0;
    while (e2.canUndo()) {
        e2.undo();
        ++steps;
    }
    QVERIFY(steps >= 1);
    QCOMPARE(e2.toPlainText(), QString());
    for (int i = 0; i < steps; ++i)
        e2.redo();
    QCOMPARE(e2.toPlainText(), QStringLiteral("one two"));

    QTest::keyClick(&e2, Qt::Key_Z, Qt::ControlModifier);
    int remaining = 0;
    while (e2.canUndo()) {
        e2.undo();
        ++remaining;
    }
    QCOMPARE(remaining, steps - 1);
}

// A programmatic load (File > Open / New -> setPlainText/clear) starts a fresh
// undo history: the user cannot Ctrl+Z back into the previous document.
void TestUndo::programmaticLoadIsNotUndoable()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("old document"));
    typeAtEnd(e, QStringLiteral(" edited"));
    QVERIFY(e.canUndo());

    e.setPlainText(QStringLiteral("# a different file\n"));
    QCOMPARE(e.toPlainText(), QStringLiteral("# a different file\n"));
    QVERIFY2(!e.canUndo(), "a freshly loaded document must not be undoable");
    QVERIFY(!e.canRedo());

    e.clear(); // File > New
    QVERIFY(!e.canUndo());
    QVERIFY(!e.canRedo());
}

// Replace All is a single undo step: one undo restores every occurrence.
void TestUndo::replaceAllIsOneUndoStep()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("cat cat cat"));
    QCOMPARE(e.replaceAll(QStringLiteral("cat"), QStringLiteral("dog")), 3);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog dog dog"));
    QVERIFY(e.canUndo());

    e.undo(); // ONE step, not three
    QCOMPARE(e.toPlainText(), QStringLiteral("cat cat cat"));
    QVERIFY(!e.canUndo());
    QVERIFY(e.canRedo());

    e.redo();
    QCOMPARE(e.toPlainText(), QStringLiteral("dog dog dog"));

    // Case sensitivity still flows through (and is still one step).
    e.setPlainText(QStringLiteral("cat CAT cat"));
    QCOMPARE(e.replaceAll(QStringLiteral("cat"), QStringLiteral("dog"), Qt::CaseSensitive), 2);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog CAT dog"));
    e.undo();
    QCOMPARE(e.toPlainText(), QStringLiteral("cat CAT cat"));
}

// A replace with no matches changes nothing — and records no undo step.
void TestUndo::blankReplaceAllRecordsNoUndoStep()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("nothing to see"));
    QCOMPARE(e.replaceAll(QStringLiteral("zebra"), QStringLiteral("horse")), 0);
    QCOMPARE(e.toPlainText(), QStringLiteral("nothing to see"));
    QVERIFY2(!e.canUndo(), "a no-op replace must not add an undo step");

    QCOMPARE(e.replaceAll(QString(), QStringLiteral("x")), 0); // empty query
    QVERIFY(!e.canUndo());
}

// The Edit menu carries Undo (Ctrl+Z) and Redo (Ctrl+Shift+Z / Ctrl+Y) above
// the existing Find/Replace entries.
void TestUndo::editMenuOffersUndoAndRedoWithStandardKeys()
{
    MainWindow w;
    QAction *undo = w.undoAction();
    QAction *redo = w.redoAction();
    QVERIFY(undo != nullptr);
    QVERIFY(redo != nullptr);
    QCOMPARE(plain(undo), QStringLiteral("Undo"));
    QCOMPARE(plain(redo), QStringLiteral("Redo"));

    // Ctrl+Z / Ctrl+Shift+Z, plus Ctrl+Y as the alternate redo.
    QCOMPARE(undo->shortcut(), QKeySequence(QKeySequence::Undo));
    QCOMPARE(undo->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Z));
    QVERIFY(redo->shortcuts().contains(QKeySequence(QKeySequence::Redo)));
    QVERIFY(redo->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_Y)));

    // The keys are scoped to the document editor (so Ctrl+Z while typing in the
    // find bar's field still undoes the FIELD, not the document).
    QCOMPARE(undo->parent(), w.editorPane());
    QCOMPARE(redo->parent(), w.editorPane());
    QCOMPARE(undo->shortcutContext(), Qt::WidgetWithChildrenShortcut);
    QCOMPARE(redo->shortcutContext(), Qt::WidgetWithChildrenShortcut);

    // They live in the Edit menu, in order, and Find/Replace are still there.
    QMenu *edit = menuByTitle(w, QStringLiteral("Edit"));
    QVERIFY(edit != nullptr);
    const QList<QAction *> acts = edit->actions();
    const int iUndo = acts.indexOf(undo);
    const int iRedo = acts.indexOf(redo);
    QVERIFY2(iUndo >= 0, "Undo is not in the Edit menu");
    QVERIFY2(iRedo > iUndo, "Redo should sit below Undo in the Edit menu");
    bool hasFind = false;
    bool hasReplace = false;
    for (QAction *a : acts) {
        const QString t = plain(a);
        hasFind = hasFind || t.startsWith(QStringLiteral("Find"));
        hasReplace = hasReplace || t.startsWith(QStringLiteral("Replace"));
    }
    QVERIFY2(hasFind, "Edit > Find disappeared");
    QVERIFY2(hasReplace, "Edit > Replace disappeared");
}

// Nothing to undo on a fresh document; the enabled state follows the stack.
void TestUndo::undoRedoActionsTrackAvailability()
{
    MainWindow w;
    QVERIFY(!w.undoAction()->isEnabled());
    QVERIFY(!w.redoAction()->isEnabled());

    typeAtEnd(*w.editorPane(), QStringLiteral("hello"));
    QVERIFY(w.undoAction()->isEnabled());
    QVERIFY(!w.redoAction()->isEnabled());

    w.undoAction()->trigger();
    QCOMPARE(w.editorPane()->toPlainText(), QString());
    QVERIFY(!w.undoAction()->isEnabled());
    QVERIFY(w.redoAction()->isEnabled());

    w.redoAction()->trigger();
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("hello"));
    QVERIFY(w.undoAction()->isEnabled());
    QVERIFY(!w.redoAction()->isEnabled());
}

// Triggering the Edit > Undo / Redo actions rewrites the document (this is the
// menu-click path; the Ctrl+Z key path is covered in the EditorPane test).
void TestUndo::undoActionRevertsAndRedoReapplies()
{
    MainWindow w;
    typeAtEnd(*w.editorPane(), QStringLiteral("# title\n"));
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("# title\n"));
    QVERIFY(w.dirty());

    w.undoAction()->trigger();
    QCOMPARE(w.editorPane()->toPlainText(), QString());
    w.redoAction()->trigger();
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("# title\n"));
}

// Undo back to the loaded content == no unsaved changes: the title loses the
// '*', the status bar says "Unmodified" and Undo is no longer offered.
void TestUndo::undoBackToLoadedContentClearsDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "one\n");
    QVERIFY(!path.isEmpty());

    MainWindow w;
    QVERIFY(w.openFile(path));
    QVERIFY(!w.dirty());
    QVERIFY2(!w.undoAction()->isEnabled(), "loading a file is not an undoable edit");

    typeAtEnd(*w.editorPane(), QStringLiteral("two\n"));
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("one\ntwo\n"));
    QVERIFY(w.dirty());
    QVERIFY(w.windowTitle().startsWith(QLatin1Char('*')));
    QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Modified"));

    w.undoAction()->trigger();
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("one\n"));
    QVERIFY2(!w.dirty(), "undo back to the loaded content must be clean again");
    QVERIFY(!w.windowTitle().contains(QLatin1Char('*')));
    QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Unmodified"));
    QVERIFY(!w.undoAction()->isEnabled());

    w.redoAction()->trigger();
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("one\ntwo\n"));
    QVERIFY(w.dirty());
    QVERIFY(w.windowTitle().startsWith(QLatin1Char('*')));
    QCOMPARE(w.statusModifiedLabel()->text(), QStringLiteral("Modified"));
}

// Saving moves the checkpoint: undoing back to the saved content is clean, and
// undoing *past* the save is dirty again (it differs from the file on disk).
void TestUndo::saveResetsTheCheckpoint()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "n.md", "one\n");
    QVERIFY(!path.isEmpty());

    MainWindow w;
    QVERIFY(w.openFile(path));
    typeAtEnd(*w.editorPane(), QStringLiteral("two\n"));
    QVERIFY(w.saveDocument());
    QVERIFY(!w.dirty());
    QVERIFY(!w.redoAction()->isEnabled());

    typeAtEnd(*w.editorPane(), QStringLiteral("three\n"));
    QVERIFY(w.dirty());

    w.undoAction()->trigger(); // back to exactly what was written
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("one\ntwo\n"));
    QVERIFY(!w.dirty());
    QVERIFY(!w.windowTitle().contains(QLatin1Char('*')));

    w.undoAction()->trigger(); // now past the saved checkpoint
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("one\n"));
    QVERIFY2(w.dirty(), "content no longer matches the file on disk -> dirty");
    QVERIFY(w.windowTitle().startsWith(QLatin1Char('*')));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestUndo t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_undo.moc"
