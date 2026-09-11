// MarkdownHighlighter — pure-cosmetic markdown syntax highlighting for the
// editor pane (subtask 3).
//
// A QSyntaxHighlighter subclass that formats, per line:
//   * headings h1–h6 (bold, level-colored, hashes in a darker shade)
//   * strong / emphasis (bold / italic)
//   * inline code (`…`) and fenced code blocks (``` … ```)
//   * links (and image links) [text](url)
//   * blockquotes (lines starting with `>`)
//   * horizontal rules (--- / *** / ___)
//   * list markers (-, *, +, 1.)
//
// All formatting is applied with plain QTextCharFormat (no HTML, no resources)
// and every rule color comes from a theme-aware color map: the highlighter is
// constructed with a dark flag (light palette when false) and can re-theme
// itself with setDark() + re-highlight (subtask 7 re-runs this on toggle).
//
// Rule engine is a pure function: `formatsForLine(line, inFence)` returns the
// list of (start, count, QTextCharFormat) ranges for one source line plus the
// fence state carried out. `highlightBlock()` applies exactly those ranges
// with QSyntaxHighlighter::setFormat(). Keeping the decision pure makes the
// highlighter fully testable without relying on QTextDocument's per-char
// format storage (which this Qt build does not expose — see AGENTS.md).
//
// Fence tracking: block state (previousBlockState/setCurrentBlockState) holds
// the in-fence flag; a line inside a fence gets the fence-content format and
// no inline rules run on it, so `#`/`**` inside code cannot leak a fake
// highlight.
#pragma once

#include <QColor>
#include <QList>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class Theme;

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument *document, bool dark = false,
                                 QObject *parent = nullptr);

    // A run of characters to format on the current line (line-relative coords).
    struct RangeFormat {
        int start = 0;
        int count = 0;
        QTextCharFormat format;
    };

    // The result of formatting one source line.
    struct LineFormats {
        QList<RangeFormat> ranges;
        bool inFenceAfter = false; // fence state to carry into the next line
    };

    // Pure rule engine: compute the formats for one line given the fence state
    // carried in from the previous line. `highlightBlock()` applies exactly
    // these ranges with setFormat(), so this is also the surface tests assert
    // on (later ranges override earlier overlapping ones, in application
    // order).
    LineFormats formatsForLine(const QString &line, bool inFence = false) const;

    // Re-theme the color map for light/dark and repaint. Used by the theme
    // toggle (subtask 7) so the editor stays in sync with the preview.
    void setDark(bool dark);
    // Re-theme the whole colour map from a full Theme (mode + accent) and
    // repaint. The Default accent keeps the built-in light/dark table; any other
    // accent tints the headings (h1 = the accent, h2 = its hover tone, h3-h6
    // blended toward the text colour), the links and the heading hashes.
    void applyTheme(const Theme &theme);
    bool dark() const { return m_dark; }

protected:
    void highlightBlock(const QString &text) override;

private:
    // One cosmetic rule: regex + char format. Rules are applied in
    // construction order; later rules override overlapping ranges, so order
    // matters (block-level rules first, inline rules last).
    struct Rule {
        QRegularExpression regex;
        QTextCharFormat format;
    };

    void buildRules(); // (re)builds m_rules from the colour map

    bool m_dark = false;
    // Accent-derived colours (only meaningful while m_themed is true).
    bool m_themed = false;
    QColor m_accentHeadings[6];
    QColor m_accentLink;
    QColor m_accentHash;
    QList<Rule> m_rules;
    QTextCharFormat m_fenceContentFormat; // whole-line, in-fence content
    QTextCharFormat m_fenceMarkerFormat;  // the ``` delimiter line
};
