// test_previewpane — exercises the widget-side `PreviewPane`.
//
// Covers the subtask 2.2 API:
//   * setRendered() puts the shared-renderer content into the pane's document,
//     and displayedQTextDocument() hands back that same (live) document with the
//     expected rendered markdown (headings, text).
//   * setBaseUrl() points the document's base URL at the file's directory, so a
//     relative image resolves against it (a real file:// image).
//   * scrollRatio() is 0 for content that does not overflow; with tall content
//     setScrollRatio()/scrollRatio() round-trip (top / middle / bottom), and
//     scrollRatioChanged fires when the vertical scrollbar moves.
//
// This is a widget test: a real `QApplication` is required (QTextBrowser is a
// QWidget). add_mdit_test forces QT_QPA_PLATFORM=offscreen, so it runs
// headless — no window is ever shown.
#include <QtTest>

#include "testmain.h"
#include "PreviewPane.h"
#include "RenderedDocument.h"

#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>

class TestPreviewPane : public QObject
{
    Q_OBJECT
private slots:
    void displayedDocument_isTheLiveBrowserDocument();
    void setRendered_rendersMarkdownIntoPane();
    void setBaseUrl_enablesRelativeImageResolution();
    void scrollRatio_isZeroWhenContentFits();
    void scrollRatio_roundTripsOnOverflowingContent();
    void scrollRatioChanged_firesOnScroll();
};

// displayedQTextDocument() must return the browser's own document (the one the
// pane actually shows), non-null — that is what export (subtask 9) reuses.
void TestPreviewPane::displayedDocument_isTheLiveBrowserDocument()
{
    PreviewPane pane;
    QTextDocument *doc = pane.displayedQTextDocument();
    QVERIFY(doc != nullptr);
    QCOMPARE(doc, pane.document());
}

// setRendered() runs the shared renderer into the pane and the rendered markdown
// (heading + text) is present in the document's plain text.
void TestPreviewPane::setRendered_rendersMarkdownIntoPane()
{
    PreviewPane pane;
    pane.setRendered(QStringLiteral("# Hello\n\nSome **bold** body.\n"), QUrl(), false);

    QTextDocument *doc = pane.displayedQTextDocument();
    QVERIFY(doc != nullptr);
    QVERIFY(doc->toPlainText().contains(QStringLiteral("Hello")));
    QVERIFY(doc->toPlainText().contains(QStringLiteral("bold body")));
    // The heading was actually rendered (an <h1>), not just pasted as text.
    QVERIFY(doc->toHtml().contains(QStringLiteral("<h1")));
    // The theme stylesheet was applied by the shared renderer.
    QCOMPARE(doc->defaultStyleSheet(), RenderedDocument::stylesheet(false));
}

// setBaseUrl() points the document's base URL at the file's directory, and a
// relative image then resolves against it (a real 4x4 file:// image).
void TestPreviewPane::setBaseUrl_enablesRelativeImageResolution()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pngPath = tmp.filePath(QStringLiteral("pic.png"));
    QImage im(4, 4, QImage::Format_RGB32);
    im.fill(Qt::blue);
    QVERIFY(im.save(pngPath));

    const QUrl base = QUrl::fromLocalFile(tmp.path() + QStringLiteral("/"));
    PreviewPane pane;
    pane.setBaseUrl(base);
    QCOMPARE(pane.baseUrl(), base);

    pane.setRendered(QStringLiteral("![p](pic.png)"), base, false);
    const QUrl resolved = base.resolved(QUrl(QStringLiteral("pic.png")));
    const QPixmap pm =
        pane.displayedQTextDocument()->resource(QTextDocument::ImageResource, resolved).value<QPixmap>();
    QVERIFY(!pm.isNull());
    QCOMPARE(pm.size(), QSize(4, 4));
}

// Content that fits within the viewport has no scroll range -> ratio 0.
void TestPreviewPane::scrollRatio_isZeroWhenContentFits()
{
    PreviewPane pane;
    pane.resize(600, 400);
    pane.setRendered(QStringLiteral("just one short line\n"), QUrl(), false);
    // No overflow -> the vertical scrollbar has no usable range.
    QVERIFY(pane.verticalScrollBar()->maximum() <= pane.verticalScrollBar()->minimum());
    QCOMPARE(pane.scrollRatio(), 0.0);
    // Setting a ratio with no overflow pins to the top (stays 0).
    pane.setScrollRatio(0.7);
    QCOMPARE(pane.scrollRatio(), 0.0);
}

// With tall, overflowing content the scrollbar has a real range and
// setScrollRatio()/scrollRatio() round-trip to top / middle / bottom.
void TestPreviewPane::scrollRatio_roundTripsOnOverflowingContent()
{
    PreviewPane pane;
    pane.resize(400, 200);

    QString md;
    for (int i = 0; i < 200; ++i)
        md += QStringLiteral("paragraph number %1 with enough words to span a line\n\n").arg(i);
    pane.setRendered(md, QUrl(), false);

    // Force a definite layout width so the document height (and thus the
    // scrollbar range) is well-defined offscreen.
    pane.displayedQTextDocument()->setTextWidth(380);
    QScrollBar *sb = pane.verticalScrollBar();
    QVERIFY(sb != nullptr);
    const int range = sb->maximum() - sb->minimum();
    QVERIFY(range > 0); // the content overflows the 200px-tall pane

    pane.setScrollRatio(0.0);
    QVERIFY(qAbs(pane.scrollRatio() - 0.0) < 0.02);

    pane.setScrollRatio(0.5);
    QVERIFY(qAbs(pane.scrollRatio() - 0.5) < 0.02);

    pane.setScrollRatio(1.0);
    QVERIFY(qAbs(pane.scrollRatio() - 1.0) < 0.02);

    // Out-of-range values are clamped.
    pane.setScrollRatio(-3.0);
    QVERIFY(qAbs(pane.scrollRatio() - 0.0) < 0.02);
    pane.setScrollRatio(5.0);
    QVERIFY(qAbs(pane.scrollRatio() - 1.0) < 0.02);
}

// scrollRatioChanged fires when the vertical scrollbar moves, carrying the new
// ratio. (This is the seam subtask 4 hooks the editor<->preview sync to.)
void TestPreviewPane::scrollRatioChanged_firesOnScroll()
{
    PreviewPane pane;
    pane.resize(400, 200);
    QString md;
    for (int i = 0; i < 200; ++i)
        md += QStringLiteral("line %1 with enough words to wrap onto a row\n\n").arg(i);
    pane.setRendered(md, QUrl(), false);
    pane.displayedQTextDocument()->setTextWidth(380);
    QScrollBar *sb = pane.verticalScrollBar();
    QVERIFY(sb != nullptr);
    QVERIFY(sb->maximum() > sb->minimum());

    QSignalSpy spy(&pane, &PreviewPane::scrollRatioChanged);
    QVERIFY(spy.isValid());

    sb->setValue((sb->maximum() - sb->minimum()) / 2);
    QVERIFY(spy.count() >= 1);
    QVERIFY(qAbs(spy.last().at(0).toDouble() - pane.scrollRatio()) < 0.02);
}

// QTEST_MAIN only creates a QCoreApplication; a QTextBrowser is a QWidget and
// needs a QApplication. The offscreen env from add_mdit_test keeps this
// headless.
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestPreviewPane t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_previewpane.moc"
