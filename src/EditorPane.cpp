#include "EditorPane.h"

#include "MarkdownHighlighter.h"
#include "MarkdownModel.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

EditorPane::EditorPane(QWidget *parent)
    : QPlainTextEdit(parent)
{
    // Monospaced source font (a `QFont` with a monospace style hint resolves to
    // a real monospace face even where no literal "monospace" family is present).
    QFont mono;
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
    setLineWrapMode(QPlainTextEdit::NoWrap); // one line per block -> a uniform gutter

    // Own the markdown highlighter; it drives per-block formatting on the doc.
    m_highlighter = new MarkdownHighlighter(document(), /*dark=*/false, this);

    // The left-hand line-number margin.
    m_lineNumberArea = new LineNumberArea(this);

    // Keep the margin in sync with the document:
    //   * a different number of lines  -> resize the gutter width;
    //   * vertical scrolling           -> repaint the gutter;
    //   * any content change           -> repaint the gutter + broadcast the
    //                                     textChanged passthrough signal;
    //   * a layout change (e.g. font)  -> resize + repaint the gutter.
    connect(this, &QPlainTextEdit::blockCountChanged, this,
            &EditorPane::updateLineNumberAreaWidth);
    if (QScrollBar *sb = verticalScrollBar())
        connect(sb, &QScrollBar::valueChanged, this, [this](int) {
            updateLineNumberArea(QRect(), 0);
        });
    connect(document(), &QTextDocument::contentsChanged, this,
            [this] { updateLineNumberArea(QRect(), 0); });
    connect(document(), &QTextDocument::documentLayoutChanged, this,
            [this] {
                updateLineNumberAreaWidth(lineCount());
                updateLineNumberArea(QRect(), 0);
            });
    // The explicit textChanged passthrough (re-broadcast on the concrete type).
    connect(this, &QPlainTextEdit::textChanged, this,
            [this] { emit editorTextChanged(); });

    updateLineNumberAreaWidth(lineCount());
}

// -- Counts. ------------------------------------------------------------------

int EditorPane::wordCount() const
{
    // Delegate to the canonical pure implementation (MarkdownModel) so the
    // status-bar counts and the text model can never diverge.
    return MarkdownModel::wordCount(toPlainText());
}

int EditorPane::charCount() const
{
    // QChar count — includes spaces and line breaks.
    return MarkdownModel::charCount(toPlainText());
}

// -- Line-number margin. ------------------------------------------------------

int EditorPane::lineNumberAreaWidth() const
{
    // 3px left padding + a digit per digit of the largest line number.
    int digits = 1;
    int maxBlock = qMax(1, lineCount());
    while (maxBlock >= 10) {
        maxBlock /= 10;
        ++digits;
    }
    return 3 + digits * QFontMetricsF(font()).horizontalAdvance(QLatin1Char('9'));
}

QStringList EditorPane::lineNumbers() const
{
    const int n = lineCount();
    QStringList nums;
    nums.reserve(n);
    for (int i = 1; i <= n; ++i)
        nums.append(QString::number(i));
    return nums;
}

void EditorPane::updateLineNumberAreaWidth(int /*newBlockCount*/)
{
    // Reserve the left gutter for the margin and size/position the margin to
    // match the current contents rect.
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void EditorPane::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }
}

void EditorPane::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    const QRect clip = event->rect();
    painter.setPen(palette().color(QPalette::PlaceholderText));

    // The editor uses NoWrap, so block N sits exactly at y = N * line-height
    // (QTextDocument::blockBoundingGeometry is not available on this Qt build,
    // so we derive the offset from the uniform line metrics instead).
    const int lineH = qRound(QFontMetricsF(font()).lineSpacing());
    const int scroll = verticalScrollBar()->value();
    const int areaW = m_lineNumberArea->width();

    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        const int top = block.blockNumber() * lineH - scroll;
        // Stop once we are past the visible window (blocks are in order, so no
        // later block can be visible again).
        if (top > clip.bottom() + lineH)
            break;
        if (top + lineH >= clip.top() && top <= clip.bottom() && block.isVisible()) {
            const QString num = QString::number(block.blockNumber() + 1);
            const int numW = qRound(QFontMetricsF(font()).horizontalAdvance(num));
            painter.drawText(areaW - 5 - numW, top, num);
        }
        block = block.next();
    }
}

void EditorPane::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    // Reposition/resize the margin to fill the contents rect on every resize.
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// -- Find / replace helpers. --------------------------------------------------

bool EditorPane::find(const QString &text, int flags)
{
    if (text.isEmpty())
        return false;
    QTextDocument::FindFlags ff;
    if (flags & Qt::MatchCaseSensitive)
        ff |= QTextDocument::FindCaseSensitively;
    const bool useRegex = (flags & Qt::MatchRegularExpression) != 0;

    const QTextCursor cur = textCursor();
    const int from = cur.hasSelection() ? cur.selectionEnd() : cur.position();

    QTextCursor found;
    if (useRegex) {
        // On this Qt build fromWildcard(pattern, caseSensitivity, ...) — the
        // sensitivity lives in the regex, not the document FindFlags; regex is
        // engaged simply by passing a QRegularExpression to find().
        const Qt::CaseSensitivity cs =
            (flags & Qt::MatchCaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive;
        found = document()->find(QRegularExpression::fromWildcard(text, cs), from, ff);
    } else {
        found = document()->find(text, from, ff);
    }

    if (found.isNull()) {
        // A miss collapses the current selection (deselect) so a stale match
        // highlight is not left on screen.
        QTextCursor cur = textCursor();
        cur.clearSelection();
        setTextCursor(cur);
        return false;
    }
    setTextCursor(found); // selects the match
    centerCursor();
    return true;
}

bool EditorPane::findPrevious(const QString &text, int flags)
{
    if (text.isEmpty())
        return false;
    QTextDocument::FindFlags ff;
    if (flags & Qt::MatchCaseSensitive)
        ff |= QTextDocument::FindCaseSensitively;
    const bool useRegex = (flags & Qt::MatchRegularExpression) != 0;

    const QTextCursor cur = textCursor();
    const int from = cur.hasSelection() ? cur.selectionStart() : cur.position();

    QTextCursor found;
    if (useRegex) {
        const Qt::CaseSensitivity cs =
            (flags & Qt::MatchCaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive;
        found = document()->find(QRegularExpression::fromWildcard(text, cs), from,
                                 ff | QTextDocument::FindBackward);
    } else {
        found = document()->find(text, from, ff | QTextDocument::FindBackward);
    }

    if (found.isNull()) {
        QTextCursor cur = textCursor();
        cur.clearSelection();
        setTextCursor(cur);
        return false;
    }
    setTextCursor(found);
    centerCursor();
    return true;
}

int EditorPane::matchCount(const QString &text, Qt::CaseSensitivity cs) const
{
    if (text.isEmpty())
        return 0;
    const QString hay = toPlainText();
    int count = 0;
    int pos = 0;
    while ((pos = hay.indexOf(text, pos, cs)) != -1) {
        ++count;
        pos += text.size();
    }
    return count;
}

int EditorPane::replaceAll(const QString &text, const QString &with,
                           Qt::CaseSensitivity cs)
{
    if (text.isEmpty())
        return 0;
    const int n = matchCount(text, cs);
    if (n == 0)
        return 0; // nothing matched -> no edit, and no undo step to record

    // Rewrite through a QTextCursor inside ONE edit block, so the whole sweep is
    // a single undo step: a bare setPlainText() would clear the undo stack and
    // make Replace All un-undoable. The FindFlags mirror matchCount()'s case
    // rules so the reported count and the replacements cannot disagree.
    const QTextDocument::FindFlags ff =
        (cs == Qt::CaseSensitive) ? QTextDocument::FindCaseSensitively
                                  : QTextDocument::FindFlags();

    QTextCursor block(document());
    block.beginEditBlock(); // one undo command for the entire sweep
    int replaced = 0;
    QTextCursor hit = document()->find(text, 0, ff);
    while (!hit.isNull()) {
        hit.insertText(with); // replaces the selection; the cursor ends after it
        ++replaced;
        hit = document()->find(text, hit.position(), ff);
    }
    block.endEditBlock();

    QTextCursor c(document()); // the pre-existing contract: cursor at the start
    c.movePosition(QTextCursor::Start);
    setTextCursor(c);
    return replaced;
}

// -- Undo / redo (post-loop polish). ------------------------------------------

bool EditorPane::canUndo() const
{
    return document()->isUndoAvailable();
}

bool EditorPane::canRedo() const
{
    return document()->isRedoAvailable();
}

void EditorPane::markClean()
{
    // Establishes the "content as saved / as loaded" checkpoint the modified
    // flag is measured against (undo back to it clears the flag again).
    document()->setModified(false);
}

bool EditorPane::isModified() const
{
    return document()->isModified();
}

// -- Markdown highlighting. ---------------------------------------------------

void EditorPane::setHighlighterDark(bool dark)
{
    if (m_highlighter)
        m_highlighter->setDark(dark);
}

void EditorPane::setTheme(const Theme &theme, int uiScalePercent)
{
    // The editor's monospace face follows the UI scale (the gutter width and
    // line height derive from the font, so they scale with it), and the
    // highlighter follows the theme — accent included.
    QFont mono = Theme::scaledMonoFont(uiScalePercent);
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
    if (m_highlighter)
        m_highlighter->applyTheme(theme);
    updateLineNumberAreaWidth(lineCount());
}

// -- Vertical scroll ratio (editor<->preview sync seam). ----------------------

double EditorPane::scrollRatio() const
{
    if (const QScrollBar *sb = verticalScrollBar()) {
        const int range = sb->maximum() - sb->minimum();
        if (range > 0)
            return double(sb->value() - sb->minimum()) / double(range);
    }
    return 0.0;
}

void EditorPane::setScrollRatio(double ratio)
{
    if (QScrollBar *sb = verticalScrollBar()) {
        const int range = sb->maximum() - sb->minimum();
        if (range <= 0) {
            // No overflow: pin to the top (the only meaningful position).
            sb->setValue(sb->minimum());
            return;
        }
        const double clamped = qBound(0.0, ratio, 1.0);
        sb->setValue(sb->minimum() + qRound(clamped * range));
    }
}
