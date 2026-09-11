#include "RenderedDocument.h"

#include "Theme.h"

#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QFile>
#include <QTextDocument>

// The full feature set: on this Qt build the only "full" preset is the GitHub
// (GFM) dialect, which turns on tables, autolinks, strikethrough, task lists and
// everything the CommonMark core provides. See the comment on features() in the
// header for why there is no finer-grained flag set to OR together here.
QTextDocument::MarkdownFeatures RenderedDocument::features()
{
    return QTextDocument::MarkdownDialectGitHub;
}

void RenderedDocument::render(const QString &md, const QUrl &baseUrl, const Theme &theme,
                             QTextDocument &out)
{
    // Fill in place: setMarkdown() replaces any prior content on `out`.
    out.setBaseUrl(baseUrl);        // first, so relative resources resolve
    out.setMarkdown(md, features());
    out.setDefaultStyleSheet(stylesheet(theme));
}

void RenderedDocument::render(const QString &md, const QUrl &baseUrl, bool dark, QTextDocument &out)
{
    // The Default-accent light/dark theme (the pre-accent behaviour).
    render(md, baseUrl, Theme::make(dark), out);
}

QString RenderedDocument::stylesheet(const Theme &theme)
{
    // Every colour comes from the theme's document (`doc_*`) tokens, so the
    // preview, HTML export and PDF export all follow the mode AND the accent
    // and none of them can drift from the others.
    return QStringLiteral(
               "body { color: %1; background-color: %2; }"
               "h1 { font-size: 2em; color: %3; }"
               "h2 { font-size: 1.6em; color: %4; }"
               "h3 { font-size: 1.3em; color: %5; }"
               "h4, h5, h6 { color: %6; }"
               "code, pre { font-family: 'monospace'; background-color: %7; color: %8; }"
               "pre { padding: 6px; }"
               "table { border-collapse: collapse; }"
               "th, td { border: 1px solid %9; padding: 4px 8px; }"
               "th { background-color: %10; }")
        .arg(theme[QStringLiteral("doc_fg")],
             theme[QStringLiteral("doc_bg")],
             theme[QStringLiteral("doc_h1")],
             theme[QStringLiteral("doc_h2")],
             theme[QStringLiteral("doc_h3")],
             theme[QStringLiteral("doc_h4")],
             theme[QStringLiteral("doc_code_bg")],
             theme[QStringLiteral("doc_code_fg")],
             theme[QStringLiteral("doc_table_border")])
        + QStringLiteral("th { background-color: %1; }")
              .arg(theme[QStringLiteral("doc_table_head_bg")])
        + QStringLiteral("blockquote { color: %1; }")
              .arg(theme[QStringLiteral("doc_quote_fg")])
        + QStringLiteral("a { color: %1; }").arg(theme[QStringLiteral("doc_link")])
        + QStringLiteral("hr { background-color: %1; }")
              .arg(theme[QStringLiteral("doc_hr")]);
}

QString RenderedDocument::stylesheet(bool dark)
{
    // Kept for the pre-accent callers/tests: the Default-accent theme.
    return stylesheet(Theme::make(dark));
}

static QString escapeHtmlAttribute(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return out;
}

QString RenderedDocument::standaloneHtml(const QString &title,
                                         const QString &documentHtml,
                                         const Theme &theme)
{
    // Lift the <body> content out of the toHtml() fragment so we can wrap it in
    // our own page skeleton (a real doctype + a head that carries the theme
    // stylesheet — which QTextDocument::toHtml() does NOT include). If the
    // fragment has no <body> tag (a different Qt output shape) fall back to the
    // raw fragment as the body: still a valid, non-empty standalone page.
    QString body = documentHtml;
    // The toHtml() output opens the body as <body link=".." vlink=".."> (an
    // attributed tag) — find the closing '>' of the opening tag, not the bare
    // "<body>" spelling.
    const int bodyTag = documentHtml.indexOf(QStringLiteral("<body"));
    const int bodyOpen = bodyTag >= 0
        ? documentHtml.indexOf(QLatin1Char('>'), bodyTag)
        : -1;
    const int bodyClose = documentHtml.lastIndexOf(QStringLiteral("</body>"));
    if (bodyOpen > bodyTag && bodyClose > bodyOpen)
        body = documentHtml.mid(bodyOpen + 1, bodyClose - bodyOpen - 1);

    return QStringLiteral("<!DOCTYPE html>\n")
         + QStringLiteral("<html>\n")
         + QStringLiteral("<head>\n")
         + QStringLiteral("<meta charset=\"utf-8\">\n")
         + QStringLiteral("<title>%1</title>\n").arg(escapeHtmlAttribute(title))
         + QStringLiteral("<style>\n%1\n</style>\n").arg(stylesheet(theme))
         + QStringLiteral("</head>\n")
         + QStringLiteral("<body>\n%1\n</body>\n").arg(body.trimmed())
         + QStringLiteral("</html>\n");
}

bool RenderedDocument::renderPdf(const QString &md, const QUrl &baseUrl, const Theme &theme,
                                 const QString &outPath,
                                 QPageSize::PageSizeId pageSizeId,
                                 int resolutionDpi)
{
    // Same rendering path as the preview / HTML export: render into a local
    // document (so the theme stylesheet and base URL are applied exactly as
    // they are on screen), then print it into a QPdfWriter. What you preview
    // is what you export.
    QTextDocument doc;
    render(md, baseUrl, theme, doc);

    {
        QPdfWriter writer(outPath);
        // Page size + orientation + margins (spec: "set page size/DPI"). Qt6
        // has no QPageSetup — QPagedPaintDevice takes a QPageLayout, which is
        // built here from the requested page size (default A4), portrait
        // orientation and an 18 mm margin (keeps content off the page edge).
        const QPageSize page(pageSizeId);
        const QPageLayout layout(page, QPageLayout::Portrait,
                                 QMarginsF(18.0, 18.0, 18.0, 18.0),
                                 QPageLayout::Millimeter);
        writer.setPageLayout(layout);
        // Resolution / DPI. Guard against a bad (<=0) value: keep the writer's
        // default rather than requesting a nonsense DPI.
        if (resolutionDpi > 0)
            writer.setResolution(resolutionDpi);
        doc.print(&writer); // QPdfWriter IS a QPagedPaintDevice
    } // writer destroyed here -> the PDF is finalized on disk

    // Success == a real PDF was produced (the %PDF- magic header at byte 0).
    // A path that could not be opened/created never gets the header.
    QFile f(outPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(5);
    f.close();
    return head == QByteArrayLiteral("%PDF-");
}

QString RenderedDocument::standaloneHtml(const QString &title,
                                         const QString &documentHtml,
                                         bool dark)
{
    // Kept for the pre-accent callers: the Default-accent light/dark theme.
    return standaloneHtml(title, documentHtml, Theme::make(dark));
}

bool RenderedDocument::renderPdf(const QString &md, const QUrl &baseUrl, bool dark,
                                 const QString &outPath,
                                 QPageSize::PageSizeId pageSizeId,
                                 int resolutionDpi)
{
    // Kept for the pre-accent callers: the Default-accent light/dark theme.
    return renderPdf(md, baseUrl, Theme::make(dark), outPath, pageSizeId, resolutionDpi);
}
