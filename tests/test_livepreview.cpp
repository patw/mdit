// test_livepreview — subtask 4.2 (headless / offscreen).
//
// Verifies the live-preview wiring in MainWindow:
//   * on editor change a DEBOUNCED render of the preview is scheduled, and the
//     rendered markdown (heading + bold) lands in the preview's document;
//   * opening a file renders the preview immediately (no debounce wait);
//   * the preview base URL is derived from the document's file path, so a
//     relative image reference resolves against the file's directory;
//   * no render happens while the preview is hidden, and re-showing it renders
//     the current content (it was not re-rendered while hidden);
//   * editor scroll follows the preview to the same scroll ratio (best-effort);
//   * the scroll sync is feedback-guarded: driving one pane from the other does
//     not echo back and move the driving pane a second time.
//
// Widget test: builds a QApplication itself (MainWindow is a QWidget). The
// offscreen platform (set by add_mdit_test) keeps it headless.
#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"
#include "PreviewPane.h"
#include "RenderedDocument.h"

#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>

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

class TestLivePreview : public QObject
{
    Q_OBJECT

private slots:
    void debounceRendersPreviewAfterTyping();
    void openFileRendersPreviewImmediately();
    void baseURIDerivedFromDocumentPath_resolvesRelativeImage();
    void noRenderWhilePreviewHidden_thenRendersOnReshow();
    void editorScrollFollowsPreview();
    void scrollSyncGuardPreventsFeedbackEcho();
};

// Typing in the editor schedules a debounced preview render; after the interval
// the preview's document holds the rendered markdown (an <h1> + the bold text).
void TestLivePreview::debounceRendersPreviewAfterTyping()
{
    MainWindow w;
    QVERIFY(w.isPreviewVisible()); // preview is ON by default
    w.setPreviewDebounceMs(20); // short so the offscreen run is snappy

    // The preview is empty before any content is typed.
    QVERIFY(w.previewPane()->displayedQTextDocument()->toPlainText().isEmpty());

    w.editorPane()->setPlainText(QStringLiteral("# Live\n\nSome **bold** body.\n"));
    QTest::qWait(120); // let the singleShot debounce timer fire

    QTextDocument *doc = w.previewPane()->displayedQTextDocument();
    QVERIFY(doc->toPlainText().contains(QStringLiteral("Live")));
    QVERIFY(doc->toPlainText().contains(QStringLiteral("bold body")));
    // The heading was rendered (an <h1>), not merely pasted as text.
    QVERIFY(doc->toHtml().contains(QStringLiteral("<h1")));
}

// openFile() renders the loaded content into the preview immediately — there is
// no debounce wait on load.
void TestLivePreview::openFileRendersPreviewImmediately()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeTemp(dir.path(), "notes.md", "# Titled\n\n**emph** text\n");

    MainWindow w;
    QVERIFY(w.openFile(path));

    // No QTest::qWait() in between: openFile renders synchronously.
    QTextDocument *doc = w.previewPane()->displayedQTextDocument();
    QVERIFY(doc->toPlainText().contains(QStringLiteral("Titled")));
    QVERIFY(doc->toPlainText().contains(QStringLiteral("emph text")));
    QVERIFY(doc->toHtml().contains(QStringLiteral("<h1")));
}

// The preview base URL is the document file's directory, so a relative image
// reference (`pic.png`) resolves against it (a real 4x4 file:// image).
void TestLivePreview::baseURIDerivedFromDocumentPath_resolvesRelativeImage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // A real 4x4 image sitting next to the markdown file.
    const QString pngPath = dir.filePath(QStringLiteral("pic.png"));
    QImage im(4, 4, QImage::Format_RGB32);
    im.fill(Qt::blue);
    QVERIFY(im.save(pngPath));

    const QString mdPath = writeTemp(dir.path(), "doc.md", "![p](pic.png)\n");
    QVERIFY(!mdPath.isEmpty());

    MainWindow w;
    QVERIFY(w.openFile(mdPath));

    // Base URL == the file's directory as a file:// URL (with trailing slash).
    const QUrl expectedBase =
        QUrl::fromLocalFile(dir.path() + QStringLiteral("/"));
    QCOMPARE(w.previewPane()->baseUrl(), expectedBase);

    // The relative image resolves against that base URL to a real pixmap.
    const QUrl resolved = expectedBase.resolved(QUrl(QStringLiteral("pic.png")));
    const QPixmap pm = w.previewPane()->displayedQTextDocument()
                           ->resource(QTextDocument::ImageResource, resolved)
                           .value<QPixmap>();
    QVERIFY(!pm.isNull());
    QCOMPARE(pm.size(), QSize(4, 4));
}

// While the preview is hidden, typing does NOT re-render it; re-showing it then
// renders the current content (it was not re-rendered while hidden).
void TestLivePreview::noRenderWhilePreviewHidden_thenRendersOnReshow()
{
    MainWindow w;
    w.setPreviewDebounceMs(20);

    // Hide the preview.
    QVERIFY(w.togglePreview() == false); // was visible -> now hidden
    QVERIFY(!w.isPreviewVisible());

    // Type while hidden: the debounce is skipped entirely.
    w.editorPane()->setPlainText(QStringLiteral("# Hidden\n\nno render while hidden\n"));
    QTest::qWait(120);
    // The preview was never (re)rendered while hidden.
    QVERIFY(!w.previewPane()->displayedQTextDocument()->toPlainText()
                .contains(QStringLiteral("no render while hidden")));

    // Re-show: the current content is rendered now.
    QVERIFY(w.togglePreview() == true); // was hidden -> now visible
    QVERIFY(w.isPreviewVisible());
    QVERIFY(w.previewPane()->displayedQTextDocument()->toPlainText()
                .contains(QStringLiteral("Hidden")));
}

// Scrolling the editor moves the preview to (approximately) the same ratio —
// the best-effort reader-alignment sync.
void TestLivePreview::editorScrollFollowsPreview()
{
    MainWindow w;
    w.show(); // offscreen show gives the panes real geometry
    QTest::qWait(50);

    // Make both panes overflow so their vertical scrollbars have a real range.
    QString longText;
    for (int i = 0; i < 500; ++i)
        longText += QStringLiteral("editor line %1 with enough words to overflow\n").arg(i);
    w.editorPane()->setPlainText(longText);
    w.previewPane()->setRendered(longText, QUrl(), false);
    w.previewPane()->displayedQTextDocument()->setTextWidth(300);
    w.previewPane()->resize(320, 200);
    QTest::qWait(30);

    QScrollBar *esb = w.editorPane()->verticalScrollBar();
    QScrollBar *psb = w.previewPane()->verticalScrollBar();
    QVERIFY(esb != nullptr && psb != nullptr);
    QVERIFY(esb->maximum() > esb->minimum()); // editor overflows
    QVERIFY(psb->maximum() > psb->minimum()); // preview overflows

    // Drive the editor to the middle; the preview should follow.
    w.editorPane()->setScrollRatio(0.5);
    QTest::qWait(30);
    QVERIFY(qAbs(w.previewPane()->scrollRatio() - 0.5) < 0.1);
}

// Feedback guard: an explicit editor scroll should move the editor's scrollbar
// exactly once. If the guard were broken, the preview's echo would drive the
// editor back, producing a second valueChanged.
void TestLivePreview::scrollSyncGuardPreventsFeedbackEcho()
{
    MainWindow w;
    w.show(); // offscreen show gives the panes real geometry
    QTest::qWait(50);

    QString longText;
    for (int i = 0; i < 500; ++i)
        longText += QStringLiteral("editor line %1 with enough words to overflow\n").arg(i);
    w.editorPane()->setPlainText(longText);
    w.previewPane()->setRendered(longText, QUrl(), false);
    w.previewPane()->displayedQTextDocument()->setTextWidth(300);
    w.previewPane()->resize(320, 200);
    QTest::qWait(30);

    QScrollBar *esb = w.editorPane()->verticalScrollBar();
    QScrollBar *psb = w.previewPane()->verticalScrollBar();
    QVERIFY(esb->maximum() > esb->minimum());
    QVERIFY(psb->maximum() > psb->minimum());

    QSignalSpy eSpy(esb, &QScrollBar::valueChanged);
    QVERIFY(eSpy.isValid());

    w.editorPane()->setScrollRatio(0.6); // one explicit editor move

    // Only our explicit set should have moved the editor's scrollbar: a
    // feedback echo (preview -> editor) would emit valueChanged a second time.
    QCOMPARE(eSpy.count(), 1);
    // And the preview did follow to roughly the same fraction.
    QVERIFY(qAbs(w.previewPane()->scrollRatio() - 0.6) < 0.1);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestLivePreview t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_livepreview.moc"
