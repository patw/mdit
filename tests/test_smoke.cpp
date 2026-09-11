// test_smoke — subtask 0.2 window-shell smoke test (headless / offscreen).
//
// Verifies the MainWindow skeleton builds and behaves without a real display:
// the QSplitter holds the EditorPane on the left and the PreviewPane on the
// right at a ~50/50 split, the window has ONE chrome row — the menu bar (with
// the light/dark toggle in its right-hand corner) plus a status bar, and no
// toolbar — the preview is visible by default, togglePreview() hides/shows it,
// and openFile() loads a real .md file (with \r\n normalization) / resets to
// untitled for a missing path.
//
// This is a widget test, so it constructs a QApplication itself (QTEST_MAIN only
// creates a QCoreApplication, which has no widgets). The offscreen platform
// plugin is set by add_mdit_test() in tests/CMakeLists.txt.

#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"
#include "PreviewPane.h"

#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>

class TestSmoke : public QObject
{
    Q_OBJECT

private slots:
    void constructsAndLayouts();
    void splitterIsFiftyFifty();
    void hasMenuBarAndStatusBarOnly();
    void previewVisibleByDefaultAndToggles();
    void openFileLoadsTextAndNormalizes();
    void openMissingPathIsUntitled();
    void newDocumentClearsPanes();
};

void TestSmoke::constructsAndLayouts()
{
    MainWindow w;
    QVERIFY(w.splitter() != nullptr);
    QCOMPARE(w.splitter()->count(), 2);

    // Editor on the left, preview on the right.
    QWidget *left = w.splitter()->widget(0);
    QWidget *right = w.splitter()->widget(1);
    QVERIFY(qobject_cast<EditorPane *>(left) == w.editorPane());
    QVERIFY(qobject_cast<PreviewPane *>(right) == w.previewPane());
}

void TestSmoke::splitterIsFiftyFifty()
{
    MainWindow w;
    w.show();
    QApplication::processEvents();
    // The constructor sets an equal split; even if the offscreen layout has not
    // yet assigned pixels, splitterRatio() guards to 0.5 for an empty size.
    double ratio = w.splitterRatio();
    QVERIFY2(qAbs(ratio - 0.5) < 0.05,
             qPrintable(QString("expected ~0.5, got %1").arg(ratio)));
}

void TestSmoke::hasMenuBarAndStatusBarOnly()
{
    MainWindow w;
    QVERIFY(w.menuBar() != nullptr);
    QVERIFY(w.statusBar() != nullptr);
    // ONE row of chrome: the menu bar. The toolbar was removed — its buttons
    // (New/Open/Save/Preview) duplicated the menus in a second row.
    QCOMPARE(w.findChildren<QToolBar *>().size(), 0);

    // The light/dark toggle lives in the menu bar's right-hand corner.
    QToolButton *corner =
        qobject_cast<QToolButton *>(w.menuBar()->cornerWidget(Qt::TopRightCorner));
    QVERIFY2(corner != nullptr, "the theme toggle must sit in the menu bar corner");
    QCOMPARE(corner, w.themeButton());

    // The menu bar exposes File / Edit / View / Help.
    const auto menus = w.menuBar()->findChildren<QMenu *>();
    QStringList titles;
    for (QMenu *m : menus)
        titles << m->title().remove(QLatin1Char('&')); // drop mnemonics
    QVERIFY2(titles.contains(QStringLiteral("File")), qPrintable(titles.join(",")));
    QVERIFY2(titles.contains(QStringLiteral("Edit")), qPrintable(titles.join(",")));
    QVERIFY2(titles.contains(QStringLiteral("View")), qPrintable(titles.join(",")));
    QVERIFY2(titles.contains(QStringLiteral("Help")), qPrintable(titles.join(",")));
}

void TestSmoke::previewVisibleByDefaultAndToggles()
{
    MainWindow w;
    QVERIFY(w.isPreviewVisible());

    // Hiding the preview makes it not visible.
    QVERIFY(!w.togglePreview());
    QVERIFY(!w.isPreviewVisible());

    // Showing it again restores it (and re-balances the split).
    QVERIFY(w.togglePreview());
    QVERIFY(w.isPreviewVisible());
}

void TestSmoke::openFileLoadsTextAndNormalizes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("notes.md"));
    {
        QFile f(path);
        // Binary: QIODevice::Text translates \n -> \r\n on Windows, which would
        // turn the fixture's CRLF into CR CR LF and break the expectation.
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Title\r\n\r\nsome **bold** text\r\n");
    }

    MainWindow w;
    QVERIFY(w.openFile(path));
    QCOMPARE(w.editorPane()->toPlainText(),
             QString::fromUtf8("# Title\n\nsome **bold** text\n"));
    QVERIFY(w.windowTitle().contains(QStringLiteral("notes.md")));
}

void TestSmoke::openMissingPathIsUntitled()
{
    MainWindow w;
    QVERIFY(!w.openFile(QStringLiteral("/no/such/file/does-not-exist-xyz.md")));
    QVERIFY(w.editorPane()->toPlainText().isEmpty());
    // Untitled -> the bare app name, no filename, no dirty star.
    QVERIFY(w.windowTitle() == QLatin1String("mdit"));
}

void TestSmoke::newDocumentClearsPanes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("x.md"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("# x\n");
    }
    MainWindow w;
    QVERIFY(w.openFile(path));
    QCOMPARE(w.editorPane()->toPlainText(), QString::fromUtf8("# x\n"));
    // The document is clean right after a load, so newDocument() needs no
    // discard prompt and simply clears both panes.
    w.newDocument();
    QVERIFY(w.editorPane()->toPlainText().isEmpty());
    QVERIFY(w.windowTitle() == QLatin1String("mdit"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestSmoke t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_smoke.moc"
