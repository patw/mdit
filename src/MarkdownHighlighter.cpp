#include "MarkdownHighlighter.h"

#include "Theme.h"

#include <QColor>
#include <QRegularExpression>

namespace {

// Build a char format from individual attribute bits (keeps the rule tables
// below readable: color + bold/italic/underline).
QTextCharFormat fmt(const QColor &color, bool bold = false, bool italic = false,
                    bool underline = false)
{
    QTextCharFormat f;
    f.setForeground(color);
    if (bold)
        f.setFontWeight(QFont::Bold);
    if (italic)
        f.setFontItalic(true);
    if (underline) {
        f.setFontUnderline(true);
        f.setUnderlineColor(color);
    }
    return f;
}

// The theme-aware heading palette, one entry per heading level 1..6.
QColor headingColor(int level, bool dark)
{
    if (dark) {
        switch (level) {
        case 1: return QColor(0xFC, 0xD3, 0x4D); // amber-300
        case 2: return QColor(0xFD, 0xBA, 0x74); // orange-300
        case 3: return QColor(0xFD, 0xE6, 0x8A); // yellow-200
        case 4: return QColor(0x86, 0xEF, 0xAC); // green-300
        case 5: return QColor(0x93, 0xC5, 0xFD); // blue-300
        default: return QColor(0xC4, 0xB5, 0xFD); // violet-300
        }
    }
    switch (level) {
    case 1: return QColor(0x7C, 0x2D, 0x12); // orange-900
    case 2: return QColor(0x9A, 0x34, 0x12); // orange-800
    case 3: return QColor(0xB4, 0x53, 0x09); // amber-700
    case 4: return QColor(0xA1, 0x62, 0x07); // yellow-700
    case 5: return QColor(0x4D, 0x7C, 0x0F); // lime-700
    default: return QColor(0x47, 0x55, 0x69); // slate-600
    }
}

} // namespace

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document, bool dark, QObject *parent)
    : QSyntaxHighlighter(document), m_dark(dark)
{
    buildRules();
}

void MarkdownHighlighter::setDark(bool dark)
{
    // The built-in light/dark tables == the Default-accent theme.
    applyTheme(Theme::make(dark));
}

void MarkdownHighlighter::applyTheme(const Theme &theme)
{
    const bool wasThemed = m_themed;
    m_themed = (theme.accent != Theme::defaultAccent());
    if (m_themed) {
        // Accent-tinted markdown palette: the accent drives the headings
        // (h1 = primary, h2 = primary_hover, h3..h6 blended toward the text
        // colour), the links and the heading hashes.
        const QColor primary = theme.color(QStringLiteral("primary"));
        const QColor primaryHover = theme.color(QStringLiteral("primary_hover"));
        const QColor text = theme.color(QStringLiteral("doc_fg"));
        m_accentHeadings[0] = primary;
        m_accentHeadings[1] = primaryHover;
        m_accentHeadings[2] = Theme::blend(primary, text, 0.25);
        m_accentHeadings[3] = Theme::blend(primary, text, 0.45);
        m_accentHeadings[4] = Theme::blend(primary, text, 0.62);
        m_accentHeadings[5] = Theme::blend(primary, text, 0.78);
        m_accentLink = theme.color(QStringLiteral("link"));
        m_accentHash = theme.color(QStringLiteral("muted"));
    }

    const bool modeChanged = (m_dark != theme.dark);
    m_dark = theme.dark;
    if (!modeChanged && wasThemed == m_themed)
        return; // the neutral table of the same mode did not change
    buildRules();
    rehighlight();
}

void MarkdownHighlighter::buildRules()
{
    // ---- theme-aware color map -------------------------------------------
    const QColor strong = m_dark ? QColor(0x93, 0xC5, 0xFD) : QColor(0x1D, 0x4E, 0xD8);
    const QColor emphasis = m_dark ? QColor(0x6E, 0xE7, 0xB7) : QColor(0x04, 0x78, 0x57);
    const QColor code = m_dark ? QColor(0xFD, 0xBA, 0x74) : QColor(0xC2, 0x41, 0x0C);
    const QColor fenceMarker = m_dark ? QColor(0xFD, 0xBA, 0x74) : QColor(0xC2, 0x41, 0x0C);
    const QColor fenceContent = m_dark ? QColor(0xFC, 0xA5, 0xA5) : QColor(0x9A, 0x34, 0x12);
    const QColor link = m_themed ? m_accentLink
                                 : (m_dark ? QColor(0x7D, 0xD3, 0xFC) : QColor(0x1D, 0x4E, 0xD8));
    const QColor blockquote = m_dark ? QColor(0xC4, 0xB5, 0xFD) : QColor(0x6D, 0x28, 0xD9);
    const QColor hr = m_dark ? QColor(0x64, 0x74, 0x8B) : QColor(0x94, 0xA3, 0xB8);
    const QColor listMarker = m_dark ? QColor(0xFD, 0xBA, 0x74) : QColor(0xC2, 0x41, 0x0C);

    m_fenceContentFormat = fmt(fenceContent);
    m_fenceMarkerFormat = fmt(fenceMarker, /*bold*/ true);

    m_rules.clear();

    // ---- block-level rules (applied first; whole-line bases) --------------
    m_rules.append(Rule{QRegularExpression(QStringLiteral("^>\\s?.*$")),
                        fmt(blockquote, false, true)});
    m_rules.append(Rule{QRegularExpression(QStringLiteral(
                            "^(?:(?:\\s*-){3,}|\\s*\\*{3,}|\\s*_{3,})\\s*$")),
                        fmt(hr, true)});
    m_rules.append(Rule{QRegularExpression(QStringLiteral("^\\s*(?:[-*+]|\\d+\\.)\\s")),
                        fmt(listMarker, true)});

    // ---- inline rules (applied later; override overlapping ranges) --------
    // Images first (`![alt](url)`), then plain links (`[text](url)`); both use
    // the underline link format.
    m_rules.append(Rule{QRegularExpression(QStringLiteral("!?\\[[^\\]]*\\]\\([^)]*\\)")),
                        fmt(link, false, false, true)});
    m_rules.append(Rule{QRegularExpression(QStringLiteral("`[^`]+`")), fmt(code)});
    m_rules.append(Rule{QRegularExpression(QStringLiteral("\\*\\*[^*]+\\*\\*|__[^_]+__")),
                        fmt(strong, true)});
    // Look-arounds keep the emphasis rule from matching inside `**strong**`
    // or `__strong__`.
    m_rules.append(Rule{QRegularExpression(QStringLiteral("(?<!\\*)\\*(?!\\*)[^*]+\\*(?!\\*)")),
                        fmt(emphasis, false, true)});
    m_rules.append(Rule{QRegularExpression(QStringLiteral("(?<![_\\w])_(?!_)[^_]+_(?!\\w)")),
                        fmt(emphasis, false, true)});
}

// Pure rule engine (see header): fence tracking first, then headings, then
// the ordered rule table. Later ranges override earlier overlapping ones.
MarkdownHighlighter::LineFormats
MarkdownHighlighter::formatsForLine(const QString &line, bool inFence) const
{
    LineFormats out;
    if (line.isEmpty())
        return out;

    // ---- fenced code blocks (take precedence over everything) -------------
    const QRegularExpression fence(QStringLiteral("^[ \\t]{0,3}`{3,}.*$"));
    const bool isFenceLine = fence.match(line).hasMatch();
    if (inFence) {
        if (isFenceLine) {
            // Closing delimiter.
            out.ranges.append({0, line.size(), m_fenceMarkerFormat});
            out.inFenceAfter = false;
            return out;
        }
        // Continuation of the fenced block (no inline rules on it).
        out.ranges.append({0, line.size(), m_fenceContentFormat});
        out.inFenceAfter = true;
        return out;
    }
    if (isFenceLine) {
        // Opening delimiter; trailing language tag (```c) is marker too.
        out.ranges.append({0, line.size(), m_fenceMarkerFormat});
        out.inFenceAfter = true;
        return out;
    }

    // ---- headings (whole line, before the inline rules) -------------------
    // `#...` hashes at column 0, optionally followed by a space + content.
    // `#noSpace` is NOT a heading (CommonMark requires the space).
    const QRegularExpression heading(QStringLiteral("^#{1,6}(?:\\s.*)?$"));
    const auto hm = heading.match(line);
    if (hm.hasMatch()) {
        int level = 0;
        while (level < line.size() && line.at(level) == QLatin1Char('#'))
            ++level;
        // The hashes get a muted color; the heading text is bold + level color.
        const QColor hash = m_themed ? m_accentHash
                                     : (m_dark ? QColor(0x94, 0xA3, 0xB8) : QColor(0x64, 0x74, 0x8B));
        out.ranges.append({0, level, fmt(hash, true)});
        if (line.size() > level) {
            const QColor colour = m_themed ? m_accentHeadings[qBound(1, level, 6) - 1]
                                           : headingColor(level, m_dark);
            out.ranges.append({level, line.size() - level, fmt(colour, true)});
        }
    }

    // ---- rule table (block rules first, inline rules override overlaps) ---
    for (const Rule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.regex.globalMatch(line);
        while (it.hasNext()) {
            const auto m = it.next();
            out.ranges.append({m.capturedStart(0), m.capturedLength(0), rule.format});
        }
    }
    return out;
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    // Apply exactly the pure rule engine's decision, in order (later ranges
    // override overlapping earlier ones — the same order setFormat uses).
    const LineFormats res = formatsForLine(text, /*inFence=*/previousBlockState() == 1);
    for (const RangeFormat &rf : res.ranges)
        setFormat(rf.start, rf.count, rf.format);
    setCurrentBlockState(res.inFenceAfter ? 1 : 0);
}
