// test_entrypoint — subtask 10.1 (headless / offscreen).
//
// Verifies the application entry point WITHOUT launching the GUI (the real
// main() blocks in app.exec(), so the decision is isolated in the pure
// AppCli::fileToOpen() and the open is driven through MainWindow):
//   - the CLI decision (AppCli::fileToOpen): the FIRST positional argument that
//     is an existing `.md` / `.markdown` file is returned (as an absolute path);
//     a missing path, a non-regular file (a directory), a non-markdown file, or
//     no arguments yield an empty result — the app then starts in the empty
//     untitled state (mdit loads only markdown files);
//   - the entry-point behavior through the window: opening the CLI-chosen file
//     records the path on the Document, loads the content, and shows the OPENED
//     PATH in the status bar (with the file name in the title); a missing or
//     non-markdown CLI argument leaves the window untitled/empty;
//   - drag-and-drop of a `.md` / `.markdown` opens the file and shows its path
//     in the status bar (the same entry-point status as a CLI open).
//
// Widget test: builds a QApplication itself. Each window is constructed with an
// isolated temp-file QSettings backing (the MainWindow backing seam) so opening
// a file — which records it in Recent Files — never clobbers a user's real
// mdit settings.

#include "testmain.h"
#include "AppCli.h"
#include "EditorPane.h"
#include "MainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStatusBar>
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

class TestEntryPoint : public QObject
{
    Q_OBJECT

private slots:
    // --- Pure CLI decision (AppCli::fileToOpen). ----------------------------
    void cliNoArgsIsUntitled();
    void cliExistingMarkdownOpens();
    void cliMarkdownExtensionOpens();
    void cliSuffixIsCaseInsensitive();
    void cliMissingPathIsUntitled();
    void cliNonMarkdownIgnored();
    void cliDirectoryIsIgnored();
    void cliFirstValidMarkdownWins();
    // --- Entry-point behavior through the window. ---------------------------
    void entryPointOpensFileAndShowsPath();
    void entryPointMissingArgLeavesUntitled();
    void entryPointBadArgLeavesUntitled();
    // --- Drag-and-drop (tied to the same entry-point status path). ----------
    void dropOpensAndShowsPath();
};

// --- Pure CLI decision -------------------------------------------------------

void TestEntryPoint::cliNoArgsIsUntitled()
{
    QVERIFY(AppCli::fileToOpen(QStringList()).isEmpty());
    QVERIFY(AppCli::fileToOpen({QStringLiteral("")}).isEmpty());
}

void TestEntryPoint::cliExistingMarkdownOpens()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString md = writeTemp(dir.path(), QStringLiteral("a.md"), "hi\n");
    QCOMPARE(AppCli::fileToOpen({md}), md);
}

void TestEntryPoint::cliMarkdownExtensionOpens()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString md = writeTemp(dir.path(), QStringLiteral("a.markdown"), "hi\n");
    QCOMPARE(AppCli::fileToOpen({md}), md);
}

void TestEntryPoint::cliSuffixIsCaseInsensitive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString md = writeTemp(dir.path(), QStringLiteral("a.MD"), "hi\n");
    QCOMPARE(AppCli::fileToOpen({md}), md);
}

void TestEntryPoint::cliMissingPathIsUntitled()
{
    QVERIFY(AppCli::fileToOpen(
        {QStringLiteral("/no/such/file/missing-xyz.md")}).isEmpty());
}

void TestEntryPoint::cliNonMarkdownIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString txt = writeTemp(dir.path(), QStringLiteral("a.txt"), "hi\n");
    QVERIFY(AppCli::fileToOpen({txt}).isEmpty());
}

void TestEntryPoint::cliDirectoryIsIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A directory whose name ends in .md is not a regular file -> ignored.
    const QString sub = dir.path() + QStringLiteral("/subdir.md");
    QVERIFY(QDir().mkpath(sub));
    QVERIFY(QFileInfo(sub).isDir());
    QVERIFY(AppCli::fileToOpen({sub}).isEmpty());
}

void TestEntryPoint::cliFirstValidMarkdownWins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString missing = QStringLiteral("/no/such/missing.md");
    const QString txt = writeTemp(dir.path(), QStringLiteral("a.txt"), "x\n");
    const QString second = writeTemp(dir.path(), QStringLiteral("second.md"), "2\n");
    const QString first = writeTemp(dir.path(), QStringLiteral("first.md"), "1\n");

    // The first markdown file wins; a missing path and a non-markdown file are
    // skipped before it.
    QCOMPARE(AppCli::fileToOpen({missing, txt, second, first}), second);
}

// --- Entry-point behavior through the window ---------------------------------

void TestEntryPoint::entryPointOpensFileAndShowsPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path =
        writeTemp(dir.path(), QStringLiteral("cli.md"),
                  "# CLI\n\nopened from the command line\n");

    QSettings ini(dir.path() + QStringLiteral("/test.ini"), QSettings::IniFormat);
    MainWindow w(nullptr, &ini);

    // Reproduce main(): decide which argument to open, then open it.
    const QString toOpen = AppCli::fileToOpen({path});
    QCOMPARE(toOpen, path);
    QVERIFY(w.openFile(toOpen));

    // The Document recorded the path and loaded the content.
    QCOMPARE(w.document()->currentFilePath(), path);
    QCOMPARE(w.editorPane()->toPlainText(),
             QString::fromUtf8("# CLI\n\nopened from the command line\n"));
    QVERIFY(!w.document()->dirty());

    // The status bar shows the OPENED PATH (the point of this subtask) and the
    // title carries the file name.
    QCOMPARE(w.statusBar()->currentMessage(), QStringLiteral("Opened ") + path);
    QVERIFY(w.windowTitle().contains(QStringLiteral("cli.md")));
}

void TestEntryPoint::entryPointMissingArgLeavesUntitled()
{
    QSettings ini(QDir::tempPath() + QStringLiteral("/mdit_entry_test.ini"),
                  QSettings::IniFormat);
    MainWindow w(nullptr, &ini);

    // A missing CLI argument -> AppCli reports nothing to open.
    const QString toOpen =
        AppCli::fileToOpen({QStringLiteral("/no/such/file/missing-xyz.md")});
    QVERIFY(toOpen.isEmpty());

    // main() would not call openFile; the window stays in its empty untitled
    // state (bare app-name title, no file, no content, not dirty).
    QVERIFY(w.document()->currentFilePath().isEmpty());
    QVERIFY(w.editorPane()->toPlainText().isEmpty());
    QVERIFY(!w.dirty());
    QCOMPARE(w.windowTitle(), QStringLiteral("mdit"));
}

void TestEntryPoint::entryPointBadArgLeavesUntitled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString txt = writeTemp(dir.path(), QStringLiteral("notes.txt"),
                                  "not markdown\n");

    QSettings ini(dir.path() + QStringLiteral("/test.ini"), QSettings::IniFormat);
    MainWindow w(nullptr, &ini);

    // An existing non-markdown file is a "bad arg": fileToOpen ignores it
    // (mdit loads only markdown files).
    QVERIFY(AppCli::fileToOpen({txt}).isEmpty());
    // So the window remains untitled/empty (single-file, markdown-only focus).
    QVERIFY(w.document()->currentFilePath().isEmpty());
    QVERIFY(w.editorPane()->toPlainText().isEmpty());
    QCOMPARE(w.windowTitle(), QStringLiteral("mdit"));
}

// --- Drag-and-drop -----------------------------------------------------------

void TestEntryPoint::dropOpensAndShowsPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString md = writeTemp(dir.path(), QStringLiteral("drop.md"), "dropped\n");

    QSettings ini(dir.path() + QStringLiteral("/test.ini"), QSettings::IniFormat);
    MainWindow w(nullptr, &ini);

    // Dropping a .md opens it (dirty guard is a no-op here — the doc is clean).
    w.handleDroppedPaths({md});
    QCOMPARE(w.document()->currentFilePath(), md);
    QCOMPARE(w.editorPane()->toPlainText(), QStringLiteral("dropped\n"));
    // The dropped file's path is shown in the status bar (same as a CLI open).
    QCOMPARE(w.statusBar()->currentMessage(), QStringLiteral("Opened ") + md);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestEntryPoint t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_entrypoint.moc"
