// PreviewPane — the right-hand live markdown preview.
//
// Subtask 2.2: a read-only `QTextBrowser` that hosts the rendered
// `QTextDocument`. It is the widget-side half of the split:
//   * `setRendered(md, baseUrl, dark)` renders markdown into the browser's own
//     document via the shared `RenderedDocument` renderer (the same path export
//     uses in subtask 9 — "what you preview is what you export").
//   * `setBaseUrl(url)` points the document's base URL at the markdown file's
//     directory so relative image paths (`image.png`, `./screenshots/x.png`)
//     resolve against it.
//   * `displayedQTextDocument()` hands back the live `QTextDocument` so export
//     (HTML/PDF) can reuse exactly what is on screen.
//   * `scrollRatio()` / `setScrollRatio()` expose the vertical scroll position
//     as a 0..1 fraction; subtask 4 wires these into the editor<->preview
//     scroll-sync (best-effort, feedback-guarded). `scrollRatioChanged` fires
//     whenever the vertical scrollbar moves so a peer can follow.
//
// Headless-safe: everything is exercised under `QT_QPA_PLATFORM=offscreen` with
// a `QApplication`; no real window is ever shown.
#pragma once

#include <QTextBrowser>
#include <QUrl>

class QTextDocument;

class Theme;

class PreviewPane : public QTextBrowser
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);

    // Render `md` into this pane's document (in place) with the given base URL
    // and theme. Uses the shared RenderedDocument renderer, so the content
    // matches export exactly. Any prior content is replaced.
    // Renders markdown into the browser's OWN document through RenderedDocument
    // (the same path export uses, so the preview is what you get). The `Theme`
    // overload carries the accent; the bool one is the Default-accent theme.
    void setRendered(const QString &md, const QUrl &baseUrl, const Theme &theme);
    void setRendered(const QString &md, const QUrl &baseUrl, bool dark);

    // The live rendered document (owned by the browser). Export (subtask 9)
    // calls toHtml()/print() on this. Never null.
    QTextDocument *displayedQTextDocument() const { return document(); }

    // Point the document's base URL at the markdown file's directory (a
    // file:// URL) so relative resource references resolve. A no-op base URL
    // leaves absolute `file://`/`data:` references working.
    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const;

    // Vertical scroll position as a fraction in [0,1] (0 = top, 1 = bottom).
    // Returns 0 when the content does not overflow the viewport.
    double scrollRatio() const;
    // Scroll to the given fraction of the vertical range (clamped to [0,1]).
    // A no-op (stays at top) when the content does not overflow.
    void setScrollRatio(double ratio);

signals:
    // Emitted whenever the vertical scrollbar moves (valueChanged), carrying the
    // new scrollRatio(). Subtask 4 connects this to the editor for sync.
    void scrollRatioChanged(double ratio);
};
