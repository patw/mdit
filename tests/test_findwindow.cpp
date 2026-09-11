// test_findwindow — the Find / Replace POPUP WINDOW (post-loop polish).
//
// The find/replace chrome used to be an overlay glued to the top-left of the
// window (it sat right under the menu bar and looked wrong). It is now its own
// non-modal popup window, centered over the main window every time it is shown.
// This suite pins that contract:
//   * `FindBar` IS a top-level `QDialog` window owned by the main window — not a
//     child overlay;
//   * Ctrl+F (showFindBar()) and Ctrl+H (onReplace()) show it NON-modally,
//     centered on the main window's frame;
//   * it stays centered when it grows as the replace row is revealed;
//   * the window title names the mode ("Find" / "Find and Replace");
//   * Esc closes it and hands the keyboard focus back to the editor;
//   * the search/replace behavior itself is unchanged (a short end-to-end pass:
//     query -> count -> next -> Replace All).
//
// Widget test: offscreen QApplication; the main window IS shown (that is what
// gives it a real frame to center on), but the popup is never user-driven.
#include "testmain.h"
#include "EditorPane.h"
#include "FindBar.h"
#include "MainWindow.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QStatusBar>

namespace {

// How far (px) the two centres may differ — exact in a real WM-less offscreen
// run, a couple of pixels of slack for decoration/frame rounding elsewhere.
constexpr int kCenterTolerance = 2;

QPoint centerDelta(const QWidget &inner, const QWidget &host)
{
    return inner.frameGeometry().center() - host.frameGeometry().center();
}

QAction *actionWithShortcut(QWidget &w, const QKeySequence &seq)
{
    for (QAction *a : w.findChildren<QAction *>())
        if (a->shortcut() == seq)
            return a;
    return nullptr;
}

} // namespace

class TestFindWindow : public QObject
{
    Q_OBJECT
private slots:
    void findBarIsItsOwnPopupWindow();
    void ctrlFShowsItCenteredOnTheMainWindow();
    void ctrlHShowsTheReplaceRow_andStaysCentered();
    void titleNamesTheMode();
    void escClosesThePopupAndReturnsFocusToTheEditor();
    void hiddenFindDialogDoesNotRecountOnEditorChanges();
    void findAndReplaceStillDriveTheEditor();
};

// The find/replace UI is a top-level dialog owned by the window — not a child
// widget floating inside it (the old overlay design).
void TestFindWindow::findBarIsItsOwnPopupWindow()
{
    MainWindow w;
    FindBar *bar = w.findBar();
    QVERIFY(bar != nullptr);
    QVERIFY2(qobject_cast<QDialog *>(bar) != nullptr, "FindBar must be a QDialog");
    QVERIFY2(bar->isWindow(), "FindBar must be its own top-level window");
    QCOMPARE(bar->parentWidget(), static_cast<QWidget *>(&w));
    QVERIFY2(!bar->isModal(), "the find popup must be non-modal");
    // It is not part of the splitter/editor subtree it used to be glued to.
    QVERIFY(bar->parentWidget() != static_cast<QWidget *>(w.editorPane()));
}

// Ctrl+F shows the popup, centered on the main window's frame.
void TestFindWindow::ctrlFShowsItCenteredOnTheMainWindow()
{
    MainWindow w;
    w.resize(1000, 700);
    w.show(); // offscreen show gives the window a real frame to center on

    FindBar *bar = w.findBar();
    QVERIFY(!bar->isVisible()); // hidden until asked for

    w.showFindBar();
    QVERIFY2(bar->isVisible(), "Ctrl+F must show the popup");

    const QPoint delta = centerDelta(*bar, w);
    QVERIFY2(qAbs(delta.x()) <= kCenterTolerance && qAbs(delta.y()) <= kCenterTolerance,
             qPrintable(QStringLiteral("popup not centered: off by (%1, %2)")
                            .arg(delta.x())
                            .arg(delta.y())));

    // ...and it comes back centered the next time it is shown (e.g. after being
    // dragged away): re-showing recenters it.
    bar->move(0, 0);
    bar->closeFindBar();
    QVERIFY(!bar->isVisible());
    w.showFindBar();
    const QPoint again = centerDelta(*bar, w);
    QVERIFY2(qAbs(again.x()) <= kCenterTolerance && qAbs(again.y()) <= kCenterTolerance,
             qPrintable(QStringLiteral("popup not re-centered: off by (%1, %2)")
                            .arg(again.x())
                            .arg(again.y())));
}

// Ctrl+H reveals the replace row (the popup grows) and it stays centered.
void TestFindWindow::ctrlHShowsTheReplaceRow_andStaysCentered()
{
    MainWindow w;
    w.resize(1000, 700);
    w.show();

    FindBar *bar = w.findBar();
    w.showFindBar();
    QVERIFY(bar->isVisible());
    QVERIFY(!bar->replaceRowVisible()); // find-only by default
    const int oneRow = bar->height();

    w.onReplace(); // Ctrl+H
    QVERIFY(bar->isVisible());
    QVERIFY2(bar->replaceRowVisible(), "Ctrl+H must reveal the replace row");
    QVERIFY2(bar->height() > oneRow, qPrintable(QStringLiteral("no growth: 1-row=%1 2-row=%2").arg(oneRow).arg(bar->height())));
    QVERIFY(!bar->findChild<QLineEdit *>(QStringLiteral("ReplaceQuery"))->isHidden());

    const QPoint delta = centerDelta(*bar, w);
    QVERIFY2(qAbs(delta.x()) <= kCenterTolerance && qAbs(delta.y()) <= kCenterTolerance,
             qPrintable(QStringLiteral("popup drifted when it grew: (%1, %2)")
                            .arg(delta.x())
                            .arg(delta.y())));
}

// The title bar tells the user which mode the popup is in.
void TestFindWindow::titleNamesTheMode()
{
    MainWindow w;
    FindBar *bar = w.findBar();
    QCOMPARE(bar->windowTitle(), QStringLiteral("Find"));

    w.showFindBar();
    QCOMPARE(bar->windowTitle(), QStringLiteral("Find"));

    w.onReplace();
    QCOMPARE(bar->windowTitle(), QStringLiteral("Find and Replace"));

    bar->setReplaceRowVisible(false);
    QCOMPARE(bar->windowTitle(), QStringLiteral("Find"));
}

// Esc closes the popup (emitting closed()) and the editor gets the focus back so
// typing resumes in the document.
void TestFindWindow::escClosesThePopupAndReturnsFocusToTheEditor()
{
    MainWindow w;
    w.show();
    w.showFindBar();

    FindBar *bar = w.findBar();
    QVERIFY(bar->isVisible());
    QLineEdit *query = bar->findChild<QLineEdit *>(QStringLiteral("FindQuery"));
    QVERIFY(query != nullptr);
    query->setFocus(); // as if the user had just typed a query

    QSignalSpy closedSpy(bar, &FindBar::closed);
    QVERIFY(closedSpy.isValid());
    QTest::keyClick(bar, Qt::Key_Escape);

    QVERIFY2(!bar->isVisible(), "Esc must close the popup");
    QCOMPARE(closedSpy.count(), 1);
    // The editor is the window's focus widget again, so typing lands in the
    // document (QWidget::focusWidget() is the window-local answer and is valid
    // headlessly, where an offscreen window is never "active").
    QCOMPARE(w.focusWidget(), static_cast<QWidget *>(w.editorPane()));
}

// A hidden Find dialog must not rescan a document on every editor change. The
// stale label proves MainWindow did not call refreshCount(); opening the dialog
// again performs the normal fresh search/count.
void TestFindWindow::hiddenFindDialogDoesNotRecountOnEditorChanges()
{
    MainWindow w;
    EditorPane *editor = w.editorPane();
    editor->setPlainText(QStringLiteral("cat"));
    w.showFindBar();
    FindBar *bar = w.findBar();
    bar->setQuery(QStringLiteral("cat"));
    QCOMPARE(bar->matchLabel(), QStringLiteral("1 of 1"));

    bar->closeFindBar();
    QVERIFY(!bar->isVisible());
    editor->setPlainText(QStringLiteral("dog"));
    QCOMPARE(bar->matchLabel(), QStringLiteral("1 of 1"));

    w.showFindBar();
    QCOMPARE(bar->matchLabel(), QStringLiteral("no matches"));
}

// The popup still drives the (unchanged) editor search helpers end to end.
void TestFindWindow::findAndReplaceStillDriveTheEditor()
{
    MainWindow w;
    EditorPane *editor = w.editorPane();
    editor->setPlainText(QStringLiteral("cat bat cat"));
    editor->setFocus();

    w.showFindBar();
    FindBar *bar = w.findBar();
    bar->setQuery(QStringLiteral("cat")); // typing in the popup's field
    QCOMPARE(bar->matchCount(), 2);
    QVERIFY(editor->textCursor().hasSelection());
    QCOMPARE(editor->textCursor().selectedText(), QStringLiteral("cat"));

    bar->findNext();
    QCOMPARE(bar->matchLabel(), QStringLiteral("2 of 2"));

    w.onReplace();
    bar->setReplaceText(QStringLiteral("dog"));
    QCOMPARE(bar->replaceAll(), 2);
    QCOMPARE(editor->toPlainText(), QStringLiteral("dog bat dog"));
    QCOMPARE(bar->matchLabel(), QStringLiteral("no matches"));

    // Ctrl+F / Ctrl+H (the Edit menu actions) still reach this same popup.
    QAction *find = actionWithShortcut(w, QKeySequence(QKeySequence::Find));
    QAction *replace = actionWithShortcut(w, QKeySequence(Qt::CTRL | Qt::Key_H));
    QVERIFY(find != nullptr);
    QVERIFY(replace != nullptr);
    bar->closeFindBar();
    QVERIFY(!bar->isVisible());
    bar->setReplaceRowVisible(false); // back to find-only mode
    find->trigger();
    QVERIFY(bar->isVisible());
    QVERIFY(!bar->replaceRowVisible());
    replace->trigger();
    QVERIFY(bar->replaceRowVisible());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestFindWindow t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_findwindow.moc"
