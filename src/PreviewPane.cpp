#include "PreviewPane.h"

#include "Theme.h"

#include "RenderedDocument.h"

#include <QScrollBar>
#include <QTextDocument>

#include <algorithm>

PreviewPane::PreviewPane(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false); // links are rendered, not activated, in the preview
    setReadOnly(true);
    setFrameStyle(QFrame::NoFrame);
    setLineWrapMode(QTextEdit::WidgetWidth); // wrap like a reader, not an editor

    // Reuse the browser's own document as the single rendered document: export
    // reads from the same QTextDocument the pane displays.
    //
    // Expose scroll movement for the editor<->preview sync (subtask 4): whenever
    // the vertical scrollbar moves, report the new ratio.
    if (QScrollBar *sb = verticalScrollBar())
        connect(sb, &QScrollBar::valueChanged, this,
                [this](int) { emit scrollRatioChanged(scrollRatio()); });
}

void PreviewPane::setRendered(const QString &md, const QUrl &baseUrl, const Theme &theme)
{
    // RenderedDocument fills the caller's document in place (QTextDocument is
    // non-movable — see RenderedDocument.h). We render straight into the
    // browser's document so the pane shows exactly what export will emit.
    RenderedDocument::render(md, baseUrl, theme, *document());
}

void PreviewPane::setRendered(const QString &md, const QUrl &baseUrl, bool dark)
{
    setRendered(md, baseUrl, Theme::make(dark));
}

void PreviewPane::setBaseUrl(const QUrl &url)
{
    document()->setBaseUrl(url);
}

QUrl PreviewPane::baseUrl() const
{
    return document()->baseUrl();
}

double PreviewPane::scrollRatio() const
{
    if (const QScrollBar *sb = verticalScrollBar()) {
        const int range = sb->maximum() - sb->minimum();
        if (range > 0)
            return double(sb->value() - sb->minimum()) / double(range);
    }
    return 0.0;
}

void PreviewPane::setScrollRatio(double ratio)
{
    if (QScrollBar *sb = verticalScrollBar()) {
        const int range = sb->maximum() - sb->minimum();
        if (range <= 0) {
            // No overflow: pin to the top (the only meaningful position).
            sb->setValue(sb->minimum());
            return;
        }
        const double clamped = std::clamp(ratio, 0.0, 1.0);
        sb->setValue(sb->minimum() + qRound(clamped * range));
    }
}
