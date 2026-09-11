// test_render — exercises the pure `RenderedDocument` markdown renderer.
//
// Covers the required markdown coverage (headings, bold, emphasis, inline code,
// fenced code, a GFM table, a link, a blockquote, a task list) plus base-URL
// image resolution (a real file:// image resolved against the document's base
// URL) and the light/dark theme stylesheet.
//
// The module is GUI-free; it renders into a caller-supplied QTextDocument (an
// out-parameter — QTextDocument is non-movable, see RenderedDocument.h), so each
// test fills its own local QTextDocument and keeps it by reference (no by-value
// copy). The test only needs a QGuiApplication because resolving an image to a
// QPixmap requires the GUI platform (created offscreen, no window is shown).
#include <QtTest>

#include "RenderedDocument.h"

#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTextDocument>

class TestRender : public QObject
{
    Q_OBJECT
private slots:
    void fullFeatureSet_isGitHubDialect();
    void rendersHeadings();
    void rendersBoldAndEmphasis();
    void rendersInlineCode();
    void rendersFencedCode();
    void rendersGfmTable();
    void rendersLink();
    void rendersBlockquote();
    void rendersTaskList();
    void rendersImage_resolvesBaseUrl();
    void appliesThemeStylesheet_andPropagatesBaseUrl();
};

// The rendered feature set must be the fullest available (GFM) — a strict
// superset of the bare CommonMark preset.
void TestRender::fullFeatureSet_isGitHubDialect()
{
    QVERIFY(RenderedDocument::features() != QTextDocument::MarkdownDialectCommonMark);
    QCOMPARE(RenderedDocument::features(), QTextDocument::MarkdownDialectGitHub);
}

// # / ## produce h1 / h2 and their text survives in the plain-text output.
void TestRender::rendersHeadings()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("# Title\n\n## Sub\n"), QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("<h1")));
    QVERIFY(html.contains(QStringLiteral("<h2")));
    QVERIFY(d.toPlainText().contains(QStringLiteral("Title")));
    QVERIFY(d.toPlainText().contains(QStringLiteral("Sub")));
}

// **bold** and *emph* map to strong/italic spans (this Qt build uses inline
// font styles rather than <strong>/<em> tags).
void TestRender::rendersBoldAndEmphasis()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("hello **bold** and *emph* here"), QUrl(), false, d);
    const QString html = d.toHtml();
    // Bold: font-weight:700 (this build) or a <strong> tag (other builds).
    QVERIFY(html.contains(QStringLiteral("font-weight:700")) || html.contains(QStringLiteral("<strong")));
    // Emphasis: italic (this build) or an <em> tag (other builds).
    QVERIFY(html.contains(QStringLiteral("font-style:italic")) || html.contains(QStringLiteral("<em>")));
    const QString plain = d.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("bold")));
    QVERIFY(plain.contains(QStringLiteral("emph")));
}

// Inline `code` renders as monospace (this build) or a <code> tag (other builds).
void TestRender::rendersInlineCode()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("use the `render()` call"), QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("monospace")) || html.contains(QStringLiteral("<code>")));
    QVERIFY(d.toPlainText().contains(QStringLiteral("render()")));
}

// A fenced code block renders as a <pre> block in monospace, content preserved.
void TestRender::rendersFencedCode()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("```\nint main(){return 0;}\n```\n"), QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("<pre")) || html.contains(QStringLiteral("monospace")));
    QVERIFY(d.toPlainText().contains(QStringLiteral("int main(){return 0;}")));
}

// A GFM table renders as an HTML <table> with the header + cell text preserved.
void TestRender::rendersGfmTable()
{
    const QString md =
        QStringLiteral("| colA | colB |\n|------|------|\n| a1   | b1   |\n");
    QTextDocument d;
    RenderedDocument::render(md, QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("<table")));
    const QString plain = d.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("colA")));
    QVERIFY(plain.contains(QStringLiteral("a1")));
    QVERIFY(plain.contains(QStringLiteral("b1")));
}

// A markdown link renders as an <a href> with the URL and anchor text.
void TestRender::rendersLink()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("see [docs](http://example.com/x) now"), QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("<a href")));
    QVERIFY(html.contains(QStringLiteral("http://example.com/x")));
    QVERIFY(d.toPlainText().contains(QStringLiteral("docs")));
}

// A blockquote preserves its content and is marked up as a quoted/indented block.
void TestRender::rendersBlockquote()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("> quoted wisdom here\n"), QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(d.toPlainText().contains(QStringLiteral("quoted wisdom here")));
    // This build indents blockquotes (margin-left); other builds use <blockquote>.
    QVERIFY(html.contains(QStringLiteral("blockquote")) || html.contains(QStringLiteral("margin-left")));
}

// Task-list items render as list items flagged checked / unchecked.
void TestRender::rendersTaskList()
{
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("- [ ] todo item\n- [x] done item\n"), QUrl(), false, d);
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("unchecked")));
    QVERIFY(html.contains(QStringLiteral("checked")));
    const QString plain = d.toPlainText();
    QVERIFY(plain.contains(QStringLiteral("todo item")));
    QVERIFY(plain.contains(QStringLiteral("done item")));
}

// Base-URL image resolution: a relative ![](image.png) resolves against the
// document's base URL (a real file:// directory) and loads without a window.
void TestRender::rendersImage_resolvesBaseUrl()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pngPath = tmp.filePath(QStringLiteral("image.png"));
    QImage im(4, 4, QImage::Format_RGB32);
    im.fill(Qt::red);
    QVERIFY(im.save(pngPath));

    const QUrl base = QUrl::fromLocalFile(tmp.path() + QStringLiteral("/"));
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("![logo](image.png)"), base, false, d);

    // The image is recognized and its relative src is preserved in the HTML.
    const QString html = d.toHtml();
    QVERIFY(html.contains(QStringLiteral("<img")));
    QVERIFY(html.contains(QStringLiteral("image.png")));

    // Resolving the relative resource against the base URL yields the real image.
    const QUrl resolved = base.resolved(QUrl(QStringLiteral("image.png")));
    const QPixmap pm = d.resource(QTextDocument::ImageResource, resolved).value<QPixmap>();
    QVERIFY(!pm.isNull());
    QCOMPARE(pm.size(), QSize(4, 4));
}

// render() applies a per-document theme stylesheet (light != dark, both present)
// and propagates the base URL through to the filled document.
void TestRender::appliesThemeStylesheet_andPropagatesBaseUrl()
{
    const QString light = RenderedDocument::stylesheet(false);
    const QString dark = RenderedDocument::stylesheet(true);
    QVERIFY(!light.isEmpty());
    QVERIFY(!dark.isEmpty());
    QVERIFY(light != dark); // the two themes actually differ
    QVERIFY(light.contains(QStringLiteral("background-color")));
    QVERIFY(dark.contains(QStringLiteral("background-color")));

    QTextDocument ldoc;
    RenderedDocument::render(QStringLiteral("# Hi"), QUrl(), false, ldoc);
    QTextDocument ddoc;
    RenderedDocument::render(QStringLiteral("# Hi"), QUrl(), true, ddoc);
    QCOMPARE(ldoc.defaultStyleSheet(), light);
    QCOMPARE(ddoc.defaultStyleSheet(), dark);

    const QUrl base = QUrl(QStringLiteral("file:///home/u/docs/"));
    QTextDocument d;
    RenderedDocument::render(QStringLiteral("x"), base, false, d);
    QCOMPARE(d.baseUrl(), base);
}

// QTEST_MAIN only creates a QCoreApplication; QPixmap (used by the image
// resolution test) requires a QGuiApplication. The offscreen env set by
// add_mdit_test keeps this headless.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    TestRender t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_render.moc"
