// test_opennew — subtask 1.2 (headless / offscreen).
//
// Verifies Open/New are wired through the Document model in MainWindow:
//   - openFile() loads via Document (path recorded, dirty cleared, text set,
//     CRLF normalized, title updated);
//   - typing in the editor marks the document dirty (title gains '*');
//   - the dirty guard confirmDiscard() returns true when clean and, when dirty,
//     defers to the (scriptable) user's Save/Discard/Cancel choice;
//   - a refused guard (Cancel) leaves the open/new request alone;
//   - drag-and-drop opens the first local .md/.markdown path and ignores others.
//
// Widget test: builds a QApplication itself. The user's discard answer is
// scripted by a MainWindow subclass that overrides the virtual askDiscardChoice(),
// so no real modal dialog ever blocks the headless run.

#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>

// A MainWindow whose dirty-guard answer is fully scripted (no real dialog).
class ScriptedWindow : public MainWindow
{
public:
    MainWindow::DiscardChoice answer = MainWindow::DiscardChoice::Cancel;
    int asks = 0;

    MainWindow::DiscardChoice askDiscardChoice() override
    {
        ++asks;
        return answer;
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

class TestOpenNew : public QObject
{
    Q_OBJECT

private slots:
    void openFileGoesThroughDocument();
    void typingMarksDirtyAndTitles();
    void guardPassesWhenClean();
    void guardHonorsUserChoice();
    void refusedGuardBlocksOpen();
    void refusedGuardBlocksNew();
    void dropOpensMarkdownOnly();
};

void TestOpenNew::openFileGoesThroughDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "# Hello\r\nworld\r\n");

    MainWindow w;
    QVERIFY(w.openFile(path));

    // Path recorded on the Document model, dirty cleared, text CRLF-normalized.
    QCOMPARE(w.document()->currentFilePath(), path);
    QVERIFY(!w.document()->dirty());
    QCOMPARE(w.editorPane()->toPlainText(),
             QString::fromUtf8("# Hello\nworld\n"));
    // Title carries the filename and no dirty '*'.
    QVERIFY(w.windowTitle().contains(QStringLiteral("notes.md")));
    QVERIFY(!w.windowTitle().contains(QLatin1Char('*')));
}

void TestOpenNew::typingMarksDirtyAndTitles()
{
    MainWindow w;
    QVERIFY(!w.dirty());

    w.editorPane()->setPlainText(QStringLiteral("# a\n"));
    QVERIFY(w.dirty());
    QVERIFY(w.windowTitle().contains(QLatin1Char('*')));
}

void TestOpenNew::guardPassesWhenClean()
{
    ScriptedWindow w;
    // Clean document: no prompt is even needed, guard returns true.
    QVERIFY(w.confirmDiscard());
    QCOMPARE(w.asks, 0);
}

void TestOpenNew::guardHonorsUserChoice()
{
    ScriptedWindow w;
    w.editorPane()->setPlainText(QStringLiteral("# dirty\n"));
    QVERIFY(w.dirty());

    w.answer = MainWindow::DiscardChoice::Discard;
    QVERIFY(w.confirmDiscard());
    QCOMPARE(w.asks, 1);

    w.answer = MainWindow::DiscardChoice::Cancel;
    QVERIFY(!w.confirmDiscard());
    QCOMPARE(w.asks, 2);
}

void TestOpenNew::refusedGuardBlocksOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString first = writeTemp(dir.path(), "first.md", "first\n");
    const QString second = writeTemp(dir.path(), "second.md", "second\n");

    ScriptedWindow w;
    QVERIFY(w.openFile(first));
    w.editorPane()->setPlainText(QStringLiteral("edits\n")); // now dirty
    const QString oldPath = w.document()->currentFilePath();
    const QString oldText = w.editorPane()->toPlainText();

    w.answer = MainWindow::DiscardChoice::Cancel;
    QVERIFY(!w.openFile(second)); // guard refused -> open must not proceed
    QCOMPARE(w.document()->currentFilePath(), oldPath);
    QCOMPARE(w.editorPane()->toPlainText(), oldText);
    QVERIFY(w.dirty());
}

void TestOpenNew::refusedGuardBlocksNew()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "doc.md", "content\n");

    ScriptedWindow w;
    QVERIFY(w.openFile(path));
    w.editorPane()->setPlainText(QStringLiteral("edits\n")); // dirty
    const QString oldPath = w.document()->currentFilePath();

    w.answer = MainWindow::DiscardChoice::Cancel;
    w.newDocument(); // guard refused -> must stay on the loaded file
    QCOMPARE(w.document()->currentFilePath(), oldPath);
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("edits\n"));
    QVERIFY(w.dirty());
}

void TestOpenNew::dropOpensMarkdownOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString md = writeTemp(dir.path(), "drop.md", "dropped\n");
    const QString md2 = writeTemp(dir.path(), "drop.markdown", "also dropped\n");
    const QString other = writeTemp(dir.path(), "notes.txt", "ignore me\n");

    // A non-markdown file is ignored (doc stays untitled/clean).
    MainWindow w;
    w.handleDroppedPaths({other});
    QVERIFY(w.document()->currentFilePath().isEmpty());
    QVERIFY(w.editorPane()->toPlainText().isEmpty());

    // A .md file opens.
    w.handleDroppedPaths({md});
    QCOMPARE(w.document()->currentFilePath(), md);
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("dropped\n"));

    // A .markdown file opens (second window, to avoid the dirty guard).
    MainWindow w2;
    w2.handleDroppedPaths({md2});
    QCOMPARE(w2.document()->currentFilePath(), md2);
    QCOMPARE(w2.editorPane()->toPlainText(), QStringLiteral("also dropped\n"));

    // The first listed valid .md wins when several are dropped at once.
    MainWindow w3;
    w3.handleDroppedPaths({other, md2, md});
    QCOMPARE(w3.document()->currentFilePath(), md2);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestOpenNew t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_opennew.moc"
