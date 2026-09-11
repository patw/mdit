// test_save — subtask 5: Save (Ctrl+S) + Save As (Ctrl+Shift+S) in MainWindow.
//
// Verifies headlessly (offscreen QApplication):
//   - the File menu actions carry the spec shortcuts (Ctrl+S / Ctrl+Shift+S)
//     and are triggerable;
//   - Save on a titled document writes through the Document model (content
//     hits disk, including the remembered CRLF convention), clears dirty,
//     drops the '*' from the title, and shows a "Saved <name>" status message;
//   - Save on an UNtitled document routes to Save As (dialog seam asked once,
//     file written to the chosen path, currentFilePath updated, dirty cleared);
//   - a canceled Save As leaves every bit of state untouched;
//   - a Save As to an un-writable path fails loudly (status message) without
//     touching the model's path/dirty state;
//   - the suggested dialog name defaults from the current document (existing
//     file name, or the untitled name + ".md").
//
// The Save As dialog is scripted by overriding the virtual askSaveAsPath()
// seam (same pattern as askDiscardChoice in test_opennew), so no real modal
// dialog ever blocks the headless run. Document-level save-as
// (path update + dirty clearing) is covered by test_document.

#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QStatusBar>
#include <QTemporaryDir>

// A MainWindow whose Save As dialog answer is fully scripted (no real dialog).
class ScriptedSaveWindow : public MainWindow
{
public:
    QString scriptedSavePath;   // empty == cancel
    QString lastSuggestedName;
    int saveAsAsks = 0;

    QString askSaveAsPath(const QString &suggestedName) override
    {
        ++saveAsAsks;
        lastSuggestedName = suggestedName;
        return scriptedSavePath;
    }
};

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

static QString fileContents(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

class TestSave : public QObject
{
    Q_OBJECT

private slots:
    void saveActionsHaveSpecShortcuts();
    void saveTitledWritesThroughDocument();
    void saveRestoreRememberedCRLF();
    void saveUntitledRoutesToSaveAs();
    void saveAsCancelLeavesStateUntouched();
    void saveAsBadPathFailsWithoutStateChange();
    void suggestedNameComesFromCurrentDocument();
};

void TestSave::saveActionsHaveSpecShortcuts()
{
    MainWindow w;
    const auto actions = w.findChildren<QAction *>();

    QAction *saveAct = nullptr;
    QAction *saveAsAct = nullptr;
    for (QAction *a : actions) {
        const QString text = a->text().remove(QLatin1Char('&')).simplified();
        if (a->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_S) && text == QLatin1String("Save"))
            saveAct = a;
        if (a->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S)
            && text == QLatin1String("Save As..."))
            saveAsAct = a;
    }
    QVERIFY2(saveAct, qPrintable(QStringLiteral("no Save action with Ctrl+S")));
    QVERIFY2(saveAsAct, qPrintable(QStringLiteral("no Save As action with Ctrl+Shift+S")));
    QVERIFY(saveAct->isEnabled());
    QVERIFY(saveAsAct->isEnabled());
}

void TestSave::saveTitledWritesThroughDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "one\n");

    MainWindow w;
    QVERIFY(w.openFile(path));
    QVERIFY(!w.dirty());

    // Typing marks the document dirty: the title gains a '*'.
    w.editorPane()->setPlainText(QStringLiteral("two\n"));
    QVERIFY(w.dirty());
    QVERIFY(w.windowTitle().startsWith(QLatin1Char('*')));
    QVERIFY(w.windowTitle().contains(QStringLiteral("notes.md")));

    // Save (the menu slot): writes via the Document model.
    w.onSave();
    QVERIFY(!w.dirty());
    QCOMPARE(w.document()->currentFilePath(), path);
    QCOMPARE(fileContents(path), QString(QStringLiteral("two\n")));
    // The '*' drops off once the document is clean again.
    QVERIFY(!w.windowTitle().contains(QLatin1Char('*')));
    QVERIFY(w.windowTitle().contains(QStringLiteral("notes.md")));
    // Status bar reports the save.
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Saved notes.md")));
}

void TestSave::saveRestoreRememberedCRLF()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A CRLF file: the Document remembers the convention (test_document covers
    // the model; here we check the window-level save honors it end to end).
    const QString path = writeTemp(dir.path(), "crlf.md", "a\r\nb\r\n");

    MainWindow w;
    QVERIFY(w.openFile(path));
    w.editorPane()->setPlainText(QStringLiteral("a\nb\nc\n"));
    QVERIFY(w.dirty());

    QVERIFY(w.saveDocument());
    QCOMPARE(fileContents(path), QString(QStringLiteral("a\r\nb\r\nc\r\n")));
    QVERIFY(!w.dirty());
}

void TestSave::saveUntitledRoutesToSaveAs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("fresh.md"));

    ScriptedSaveWindow w;
    w.editorPane()->setPlainText(QStringLiteral("# brand new\n"));
    QVERIFY(w.dirty());
    QVERIFY(w.document()->currentFilePath().isEmpty());

    w.scriptedSavePath = target;
    w.onSave(); // untitled -> must route to the Save As seam

    QCOMPARE(w.saveAsAsks, 1);
    QVERIFY(QFile::exists(target));
    QCOMPARE(fileContents(target), QString(QStringLiteral("# brand new\n")));
    // Path updated, dirty cleared, title follows the new file name.
    QCOMPARE(w.document()->currentFilePath(), target);
    QVERIFY(!w.dirty());
    QVERIFY(w.windowTitle().contains(QStringLiteral("fresh.md")));
    QVERIFY(!w.windowTitle().contains(QLatin1Char('*')));
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Saved fresh.md")));

    // A later plain Save now writes to the new path (no more dialog).
    w.editorPane()->setPlainText(QStringLiteral("# brand new v2\n"));
    w.onSave();
    QCOMPARE(w.saveAsAsks, 1); // no second dialog
    QCOMPARE(fileContents(target), QString(QStringLiteral("# brand new v2\n")));
    QVERIFY(!w.dirty());
}

void TestSave::saveAsCancelLeavesStateUntouched()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "orig.md", "keep\n");

    ScriptedSaveWindow w;
    QVERIFY(w.openFile(path));
    w.editorPane()->setPlainText(QStringLiteral("changed\n"));
    QVERIFY(w.dirty());
    const QString original = fileContents(path);

    w.scriptedSavePath = QString(); // the user cancels the dialog
    QVERIFY(!w.saveAsDocument());
    QCOMPARE(w.saveAsAsks, 1);

    // Everything stays as it was: same path, still dirty, file untouched.
    QCOMPARE(w.document()->currentFilePath(), path);
    QVERIFY(w.dirty());
    QCOMPARE(w.editorPane()->toPlainText(), QString(QStringLiteral("changed\n")));
    QCOMPARE(fileContents(path), original);
    QVERIFY(w.windowTitle().startsWith(QLatin1Char('*')));
}

void TestSave::saveAsBadPathFailsWithoutStateChange()
{
    ScriptedSaveWindow w;
    w.editorPane()->setPlainText(QStringLiteral("oops\n"));
    QVERIFY(w.dirty());

    // No such directory: the write must fail, not silently succeed.
    w.scriptedSavePath = QStringLiteral("/no/such/dir/definitely/x.md");
    QVERIFY(!w.saveAsDocument());

    QCOMPARE(w.document()->currentFilePath(), QString()); // still untitled
    QVERIFY(w.dirty());
    QCOMPARE(w.editorPane()->toPlainText(), QString(QStringLiteral("oops\n")));
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Could not save x.md")));
}

void TestSave::suggestedNameComesFromCurrentDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "x\n");

    ScriptedSaveWindow w;
    QVERIFY(w.openFile(path));
    w.editorPane()->setPlainText(QStringLiteral("y\n"));
    w.scriptedSavePath = QString(); // cancel — we only care about the prefill
    w.onSaveAs();
    QCOMPARE(w.lastSuggestedName, QString(QStringLiteral("notes.md")));

    // Untitled: the untitled document name with a .md extension.
    ScriptedSaveWindow w2;
    w2.scriptedSavePath = QString();
    w2.onSaveAs();
    QCOMPARE(w2.lastSuggestedName, QString(QStringLiteral("untitled.md")));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestSave t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_save.moc"
