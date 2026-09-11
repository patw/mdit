// EditorPane — the left-hand markdown source editor (subtask 3).
//
// A `QPlainTextEdit` subclass that adds:
//   * a **line-number margin** — a dedicated left-hand `LineNumberArea` widget
//     that repaints whenever the document scrolls vertically, its layout (e.g.
//     the font) changes, or the widget resizes (the classic Qt "Code Editor"
//     pattern);
//   * the **MarkdownHighlighter** — owned here and installed on the document,
//     theme-repaintable via setHighlighterDark();
//   * **word/char counts** — always current, computed on demand;
//   * **find / replace helpers** — find()/findPrevious() (select + scroll to a
//     match), matchCount(), and replaceAll(); subtask 6 builds the Find/Replace
//     UI on top of these;
//   * a **textChanged passthrough** — `QPlainTextEdit::textChanged` is inherited
//     and re-broadcast by the base; this pane also emits `editorTextChanged()`
//     (a stable signal on the concrete type) and refreshes the line-number
//     margin on every change so the gutter, counts and highlighting stay in sync.
//
// The margin is testable without a display: `lineNumbers()` returns the exact
// "1".."N" strings the gutter paints (one per document block), and
// `lineNumberAreaWidth()` returns the width the gutter needs — so a test can
// assert the block count and the digit-width sizing GUI-free.
#pragma once

#include <QPlainTextEdit>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <Qt>
#include <QWidget>

class MarkdownHighlighter;
class Theme;

class EditorPane : public QPlainTextEdit
{
    Q_OBJECT
public:
    // The dedicated left-margin widget that paints the line numbers. A plain
    // QWidget (no signals/slots, so no moc of its own is needed); it delegates
    // its size and painting back to the owning EditorPane.
    class LineNumberArea : public QWidget
    {
    public:
        explicit LineNumberArea(EditorPane *editor)
            : QWidget(editor), m_editor(editor) {}

        QSize sizeHint() const override
        {
            return QSize(m_editor->lineNumberAreaWidth(), 0);
        }

    protected:
        void paintEvent(QPaintEvent *event) override
        {
            m_editor->lineNumberAreaPaintEvent(event);
        }

    private:
        EditorPane *m_editor;
    };
    friend class LineNumberArea;

    explicit EditorPane(QWidget *parent = nullptr);

    // -- Counts (always current; computed on demand, no stale cache). --------
    // Number of whitespace-separated words (trailing/leading blanks ignored);
    // an empty buffer is 0 words.
    int wordCount() const;
    // Number of Unicode characters in the buffer (QChar count, includes
    // whitespace and line breaks).
    int charCount() const;

    // -- Line-number margin. -------------------------------------------------
    // The dedicated left-margin widget that paints the line numbers.
    LineNumberArea *lineNumberAreaWidget() const { return m_lineNumberArea; }
    // Width (in px) the margin needs for the widest line number right now.
    int lineNumberAreaWidth() const;
    // The exact strings the margin paints, one per document block: "1".."N".
    // Exposed so tests can assert the gutter's block count / content without
    // having to drive a real paint event.
    QStringList lineNumbers() const;
    // The number of line numbers rendered — always == document()->blockCount().
    int lineCount() const { return document()->blockCount(); }

    // -- Vertical scroll ratio (the subtask-4 editor<->preview sync seam). ----
    // Vertical scroll position as a fraction in [0,1] (0 = top, 1 = bottom).
    // Returns 0 when the content does not overflow the viewport.
    double scrollRatio() const;
    // Scroll to the given fraction of the vertical range (clamped to [0,1]).
    // A no-op (pins to the top) when the content does not overflow.
    void setScrollRatio(double ratio);

    // -- Find / replace helpers (subtask 6 builds the Find/Replace UI on these).
    // Search forward from the current selection end (or the buffer start when
    // the selection is empty) for `text`; on a hit, select it and scroll the
    // pane to it. Returns false when there is no match (or `text` is empty).
    // `flags` is a Qt::MatchFlags set (e.g. Qt::MatchCaseSensitive).
    bool find(const QString &text, int flags = 0);
    // The reverse of find(): search backward from the current selection start.
    bool findPrevious(const QString &text, int flags = 0);
    // Total number of (non-overlapping) occurrences of `text` in the buffer.
    int matchCount(const QString &text, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    // Replace every occurrence of `text` with `with`; returns the number of
    // replacements made. The cursor is left at the document start.
    int replaceAll(const QString &text, const QString &with,
                   Qt::CaseSensitivity cs = Qt::CaseInsensitive);

    // ---- Undo / redo (post-loop polish). ----------------------------------
    // undo()/redo() are inherited from QPlainTextEdit and drive the document's
    // undo stack; these two make the availability explicit (and testable) so
    // the Edit-menu actions and their enabled state can follow it. Every real
    // edit is undoable, and replaceAll() is a SINGLE undo step (one Ctrl+Z
    // restores the whole sweep). A programmatic setPlainText()/clear() resets
    // the undo stack (Qt contract), so a freshly opened file is never
    // "undoable" back into the previous document.
    bool canUndo() const;
    bool canRedo() const;

    // The dirty-flag checkpoint. A QTextDocument remembers whether its content
    // has been modified since the last setModified(false) and, crucially, clears
    // that flag again when an undo returns exactly to the checkpoint (and
    // re-marks it if a redo moves past it). MainWindow calls markClean() after
    // every programmatic load (open / new) and after every successful save, so
    // "undo my change" can drop the '*' from the title.
    void markClean();
    bool isModified() const;

    // -- Markdown highlighting. ---------------------------------------------
    // The MarkdownHighlighter owned by this pane (installed on the document).
    MarkdownHighlighter *highlighter() const { return m_highlighter; }
    // Re-theme the highlighter for light/dark and repaint (the subtask-7 theme
    // toggle calls this so the editor matches the preview).
    void setHighlighterDark(bool dark);
    // Apply a FULL theme (mode **and accent**) plus the UI scale: the markdown
    // highlighter is re-themed and the monospace editor font is re-scaled (the
    // line-number gutter follows the font automatically). The MainWindow theme
    // pipeline calls this.
    void setTheme(const Theme &theme, int uiScalePercent);

signals:
    // Emitted whenever the pane's own contents change — the explicit
    // "textChanged passthrough" this subtask asks for. It re-broadcasts the
    // base QPlainTextEdit::textChanged change so callers (e.g. the subtask-4
    // debounced preview) can hook counts/margin off a stable signal on the
    // concrete type.
    void editorTextChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

    // Line-number margin machinery.
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void lineNumberAreaPaintEvent(QPaintEvent *event);

private:
    LineNumberArea *m_lineNumberArea = nullptr;
    MarkdownHighlighter *m_highlighter = nullptr;
};
