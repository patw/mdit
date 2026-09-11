// RenderedDocument — pure, GUI-independent markdown -> QTextDocument renderer.
//
// Subtask 2.1. This is the single rendering path shared by the live preview
// (PreviewPane) and export (HTML/PDF in subtask 9): both call render() so that
// "what you preview is what you export". It is intentionally free of any QWidget
// so it can be exercised headlessly by test_render with only a QGuiApplication
// (QPixmap needs one, but no window is ever shown).
//
// Design notes:
//   * Rendering uses Qt's built-in CommonMark/GFM importer via
//     QTextDocument::setMarkdown(md, features()) — no external markdown library,
//     no QtWebEngine. features() is the fullest flag set this Qt build exposes.
//   * The base URL is set BEFORE setMarkdown() so relative resource references
//     (e.g. ![](image.png)) are resolved against the markdown file's directory.
//   * A theme-aware stylesheet (light/dark) is applied per-document via
//     QTextDocument::setDefaultStyleSheet(), which on this build is a
//     per-instance property, so each rendered document carries its own theme.
//
//   * Return-value shape (spec deviation, recorded in spec.md + AGENTS.md):
//     QTextDocument is non-copyable and non-movable (Q_DISABLE_COPY, no move
//     ctor), and GCC 15 refuses to return a *configured* local QTextDocument by
//     value (NRVO is not guaranteed for named locals, and the copy/move needed
//     as a fallback are deleted). render() therefore FILLS a caller-supplied
//     QTextDocument in place (out-parameter) instead of returning one by value.
//     The caller owns the document's lifetime (stack doc in tests, the
//     QTextBrowser's document in the preview).
#pragma once

#include <QPageSize>
#include <QTextDocument>
#include <QUrl>
#include <QString>

class Theme;

class RenderedDocument
{
public:
    // The fullest markdown feature flag set available on this Qt build.
    //
    // On this Qt version QTextDocument::MarkdownFeature exposes only the two
    // dialect presets (MarkdownDialectCommonMark = 0 and MarkdownDialectGitHub,
    // a bitset that enables tables, autolinks, strikethrough, task lists, etc.)
    // plus MarkdownNoHTML — there are no per-feature enums to OR together. The
    // GitHub (GFM) dialect is therefore the maximum feature set and what the
    // spec's "full feature flag set (Tables, CommonMark, ...)" resolves to.
    static QTextDocument::MarkdownFeatures features();

    // Render markdown into `out` (in place, clearing any prior content):
    //   1. set the base URL (so relative images/resources resolve),
    //   2. import the markdown with the full feature set,
    //   3. apply the theme-aware stylesheet.
    // The `Theme` overload uses the full palette (mode **and accent**); the bool
    // overload is the Default-accent light/dark theme.
    static void render(const QString &md, const QUrl &baseUrl, const Theme &theme,
                       QTextDocument &out);
    static void render(const QString &md, const QUrl &baseUrl, bool dark, QTextDocument &out);

    // The theme-aware stylesheet (CSS) applied to rendered documents, built from
    // the theme's `doc_*` tokens (background/text/heading/link/code/table
    // colours), so the preview and the exports follow the accent.
    static QString stylesheet(const Theme &theme);
    static QString stylesheet(bool dark);

    // Assemble a standalone HTML page from a rendered document's toHtml()
    // output (the full <html>...</html> fragment QTextDocument emits): a
    // <!DOCTYPE html> wrapper, a <head> with a UTF-8 charset, the document
    // title, and the theme stylesheet in a <style> block, and the lifted
    // <body> content. If `documentHtml` carries no <body> tag (an older /
    // different Qt output shape) the raw fragment is used as the body as-is.
    // Pure string assembly — used by the HTML export (MainWindow) so the
    // exported file looks exactly like the preview (same renderer, same
    // stylesheet).
    static QString standaloneHtml(const QString &title,
                                  const QString &documentHtml,
                                  const Theme &theme);
    static QString standaloneHtml(const QString &title,
                                  const QString &documentHtml,
                                  bool dark);

    // Render markdown to a standalone .pdf at `outPath` through the SAME
    // rendering path as the preview/HTML export (render() into a local
    // QTextDocument, then QTextDocument::print() into a QPdfWriter). The page
    // size/orientation, resolution (DPI) and margins are set on the writer
    // before printing, per the spec ("set page size/DPI"). Returns true only
    // when a real PDF was written (verified by the %PDF- magic header); false
    // on any failure (unwritable path, etc.). Requires a QGuiApplication for
    // the paint engine, but no window is shown.
    static bool renderPdf(const QString &md, const QUrl &baseUrl, const Theme &theme,
                          const QString &outPath,
                          QPageSize::PageSizeId pageSizeId,
                          int resolutionDpi);
    static bool renderPdf(const QString &md, const QUrl &baseUrl, bool dark,
                          const QString &outPath,
                          QPageSize::PageSizeId pageSizeId,
                          int resolutionDpi);
};
