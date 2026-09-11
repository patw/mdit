// test_closeguard — the unsaved-changes guard when CLOSING the window.
//
// Closing mdit (the title-bar X, File > Exit / Ctrl+Q, or the desktop session
// manager asking to quit) must never silently drop unsaved changes:
// `MainWindow::closeEvent()` runs the same guard as New/Open. This suite pins
// that contract:
//   * a clean window closes without asking at all;
//   * a dirty window ASKS, and Cancel keeps it open (the close is ignored, the
//     buffer is untouched);
//   * Discard closes it and the file on disk keeps what it had;
//   * Save writes through the Document model and *then* closes;
//   * Save on an UNTITLED document routes to Save As (and a cancelled Save As
//     keeps the window open, because nothing was written);
//   * File > Exit (Ctrl+Q) goes through the same guard.
//
// The user's answer is scripted by a subclass overriding the virtual
// askDiscardChoice()/askSaveAsPath() seams, so no real modal dialog ever blocks
// the headless run.
#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextCursor>

namespace {

// A MainWindow whose guard answer and Save As target are fully scripted.
class GuardedWindow : public MainWindow
{
public:
    MainWindow::DiscardChoice answer = MainWindow::DiscardChoice::Cancel;
    int asks = 0;
    QString saveAsPath; // empty == the user cancelled Save As
    int saveAsAsks = 0;

    MainWindow::DiscardChoice askDiscardChoice() override
    {
        ++asks;
        return answer;
    }
    QString askSaveAsPath(const QString &) override
    {
        ++saveAsAsks;
        return saveAsPath;
    }
};

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

QString fileContents(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QAction *actionWithShortcut(QWidget &w, const QKeySequence &seq)
{
    for (QAction *a : w.findChildren<QAction *>())
        if (a->shortcut() == seq)
            return a;
    return nullptr;
}

} // namespace

class TestCloseGuard : public QObject
{
    Q_OBJECT
private slots:
    void cleanWindowClosesWithoutAsking();
    void dirtyWindowAsks_thenCancelKeepsItOpen();
    void discardClosesAndKeepsTheFileOnDisk();
    void saveWritesThenCloses();
    void saveOnUntitledDocumentRoutesToSaveAs();
    void exitActionRunsTheSameGuard();
};

// Nothing unsaved -> no prompt, the window just closes.
void TestCloseGuard::cleanWindowClosesWithoutAsking()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "clean.md", "one\n");

    GuardedWindow w;
    QVERIFY(w.openFile(path));
    w.show();
    QVERIFY(!w.dirty());

    QVERIFY2(w.close(), "a clean window must close without a prompt");
    QCOMPARE(w.asks, 0);
    QVERIFY(!w.isVisible());
}

// Dirty -> asks; Cancel refuses the close and leaves everything in place.
void TestCloseGuard::dirtyWindowAsks_thenCancelKeepsItOpen()
{
    GuardedWindow w;
    w.show();
    typeAtEnd(*w.editorPane(), QStringLiteral("# draft\n"));
    QVERIFY(w.dirty());

    w.answer = MainWindow::DiscardChoice::Cancel;
    QVERIFY2(!w.close(), "a cancelled close must be refused");
    QCOMPARE(w.asks, 1);
    QVERIFY2(w.isVisible(), "the window must stay open after Cancel");
    QVERIFY(w.dirty());
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("# draft\n"));
}

// Discard closes the window; the file keeps what was already on disk.
void TestCloseGuard::discardClosesAndKeepsTheFileOnDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "one\n");

    GuardedWindow w;
    QVERIFY(w.openFile(path));
    w.show();
    typeAtEnd(*w.editorPane(), QStringLiteral("two\n"));
    QVERIFY(w.dirty());

    w.answer = MainWindow::DiscardChoice::Discard;
    QVERIFY(w.close());
    QCOMPARE(w.asks, 1);
    QVERIFY(!w.isVisible());
    QCOMPARE(fileContents(path), QStringLiteral("one\n")); // not written
    // (The in-memory model still holds the edit — the window is closing; what
    // matters is that nothing was written and nothing was asked twice.)
}

// Save on a titled document writes it, clears dirty, and then closes.
void TestCloseGuard::saveWritesThenCloses()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "one\n");

    GuardedWindow w;
    QVERIFY(w.openFile(path));
    w.show();
    typeAtEnd(*w.editorPane(), QStringLiteral("two\n"));
    QVERIFY(w.dirty());

    w.answer = MainWindow::DiscardChoice::Save;
    QVERIFY(w.close());
    QCOMPARE(w.asks, 1);
    QCOMPARE(w.saveAsAsks, 0); // titled: no dialog needed
    QVERIFY(!w.dirty());
    QCOMPARE(fileContents(path), QStringLiteral("one\ntwo\n"));
    QVERIFY(!w.isVisible());
}

// Save on an untitled document routes to Save As; a cancelled Save As must keep
// the window open (nothing was written, so nothing may be discarded).
void TestCloseGuard::saveOnUntitledDocumentRoutesToSaveAs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.filePath(QStringLiteral("fresh.md"));

    {
        GuardedWindow w;
        w.show();
        typeAtEnd(*w.editorPane(), QStringLiteral("brand new\n"));
        QVERIFY(w.dirty());

        w.answer = MainWindow::DiscardChoice::Save;
        w.saveAsPath = target;
        QVERIFY(w.close());
        QCOMPARE(w.saveAsAsks, 1);
        QCOMPARE(fileContents(target), QStringLiteral("brand new\n"));
        QVERIFY(!w.isVisible());
    }

    {
        GuardedWindow w;
        w.show();
        typeAtEnd(*w.editorPane(), QStringLiteral("stays open\n"));
        w.answer = MainWindow::DiscardChoice::Save;
        w.saveAsPath = QString(); // the user cancels the Save As dialog
        QVERIFY2(!w.close(), "a cancelled Save As must not close the window");
        QCOMPARE(w.saveAsAsks, 1);
        QVERIFY(w.isVisible());
        QVERIFY(w.dirty());
        QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("stays open\n"));
    }
}

// File > Exit (Ctrl+Q) triggers the same closeEvent guard.
void TestCloseGuard::exitActionRunsTheSameGuard()
{
    GuardedWindow w;
    w.show();
    typeAtEnd(*w.editorPane(), QStringLiteral("x\n"));

    QAction *exit = actionWithShortcut(w, QKeySequence(QKeySequence::Quit));
    QVERIFY2(exit != nullptr, "no File > Exit action with Ctrl+Q");

    w.answer = MainWindow::DiscardChoice::Cancel;
    exit->trigger();
    QCOMPARE(w.asks, 1);
    QVERIFY2(w.isVisible(), "Ctrl+Q must be refusable while unsaved");

    w.answer = MainWindow::DiscardChoice::Discard;
    exit->trigger();
    QCOMPARE(w.asks, 2);
    QVERIFY2(!w.isVisible(), "File > Exit discards and closes when asked to");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestCloseGuard t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_closeguard.moc"
