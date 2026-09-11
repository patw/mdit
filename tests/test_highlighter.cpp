// test_highlighter — exercises the `MarkdownHighlighter` (subtask 3).
//
// Covers every required rule family:
//   * headings h1–h6: bold + per-level theme color, muted hashes
//   * strong (`**…**`) / emphasis (`*…*`, `_…_`)
//   * inline code (`…`)
//   * fenced code blocks (``` … ```) with carried fence state: content lines
//     get the fence format and inline rules do NOT leak into them
//   * links (and images) — underline + link color
//   * blockquotes, horizontal rules, list markers
//   * theme-awareness: setDark() re-themes the whole color map
//
// HOW FORMATS ARE ASSERTED (environment constraint, see AGENTS.md): this Qt
// build's `QTextDocument` has NO observable per-character format storage —
// `QSyntaxHighlighter::setFormat()` is a functional no-op and
// `QTextCursor::charFormat()`/`toHtml()` never reflect it (probed directly).
// The highlighter therefore exposes its pure rule engine,
// `MarkdownHighlighter::formatsForLine(line, inFence)` — the exact
// (start, count, QTextCharFormat) ranges `highlightBlock()` applies with
// `setFormat()` — and this suite asserts on those ranges.
//
// Pure-logic test: runs under QTEST_GUILESS_MAIN (no QApplication at all).
#include <QtTest>

#include "MarkdownHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextCharFormat>
#include <QTextDocument>

namespace {

// Format in effect at `pos` of `line` under the given carried fence state:
// the LAST covering range wins (mirrors setFormat application order).
QTextCharFormat fmtIn(const MarkdownHighlighter &h, const QString &line, int pos,
                      bool inFence = false)
{
    const auto res = h.formatsForLine(line, inFence);
    QTextCharFormat result;
    for (const auto &r : res.ranges)
        if (pos >= r.start && pos < r.start + r.count)
            result = r.format; // later range overrides earlier
    return result;
}

bool bold(const QTextCharFormat &f) { return f.fontWeight() == QFont::Bold; }
bool italic(const QTextCharFormat &f) { return f.fontItalic(); }
bool underline(const QTextCharFormat &f) { return f.fontUnderline(); }
QColor color(const QTextCharFormat &f) { return f.foreground().color(); }

// An unformatted position yields exactly the default format. (Cannot test
// `foreground().color().isValid()` — on this Qt build a default-constructed
// QTextCharFormat reports a valid black foreground.)
bool isPlain(const QTextCharFormat &f)
{
    return f == QTextCharFormat();
}

} // namespace

class TestHighlighter : public QObject
{
    Q_OBJECT
private slots:
    void headings_getBoldPerLevelFormat();
    void headingWithoutSpace_isNotHeading();
    void strongIsBold_emphasisIsItalic();
    void emphasis_doesNotLeakIntoStrong();
    void inlineCode_getsCodeColor();
    void linksAndImages_areUnderlined();
    void blockquote_isItalicTinted();
    void horizontalRule_isFormattedWholeLine();
    void listMarkers_areFormatted();
    void fencedCode_isTrackedAcrossLines();
    void setDark_rethemesColorMap();
};

// h1..h6 are all bold; the heading text carries a distinct color per level
// (the theme-aware map), and the `#` hashes are muted + bold.
void TestHighlighter::headings_getBoldPerLevelFormat()
{
    static const char *levels[] = {"#", "##", "###", "####", "#####", "######"};
    // Light-theme per-level colors (pin the map: orange → amber → yellow →
    // lime → slate for h1..h6).
    static const QColor expectedLight[] = {
        QColor(0x7C, 0x2D, 0x12), QColor(0x9A, 0x34, 0x12), QColor(0xB4, 0x53, 0x09),
        QColor(0xA1, 0x62, 0x07), QColor(0x4D, 0x7C, 0x0F), QColor(0x47, 0x55, 0x69)};
    static const QColor expectedDark[] = {
        QColor(0xFC, 0xD3, 0x4D), QColor(0xFD, 0xBA, 0x74), QColor(0xFD, 0xE6, 0x8A),
        QColor(0x86, 0xEF, 0xAC), QColor(0x93, 0xC5, 0xFD), QColor(0xC4, 0xB5, 0xFD)};
    const QColor hashLight(0x64, 0x74, 0x8B);
    const QColor hashDark(0x94, 0xA3, 0xB8);
    const MarkdownHighlighter light(nullptr, false);
    const MarkdownHighlighter dark(nullptr, true);

    for (int lvl = 0; lvl < 6; ++lvl) {
        const int n = lvl + 1;
        const QString line = QString(levels[lvl]) + QStringLiteral(" Title");
        const int textPos = n + 1; // skip hashes + the space
        for (int i = 0; i < n; ++i) { // hashes
            const auto lf = fmtIn(light, line, i);
            const auto df = fmtIn(dark, line, i);
            QVERIFY2(bold(lf), qPrintable(QStringLiteral("h%1 light hash not bold").arg(n)));
            QCOMPARE(color(lf), hashLight);
            QVERIFY2(bold(df), qPrintable(QStringLiteral("h%1 dark hash not bold").arg(n)));
            QCOMPARE(color(df), hashDark);
        }
        QCOMPARE(color(fmtIn(light, line, textPos)), expectedLight[lvl]);
        QCOMPARE(color(fmtIn(dark, line, textPos)), expectedDark[lvl]);
        QVERIFY(bold(fmtIn(light, line, textPos)));
        QVERIFY(bold(fmtIn(dark, line, textPos)));
        QVERIFY(color(fmtIn(light, line, textPos)) != color(fmtIn(dark, line, textPos)));
    }
}

// CommonMark: `#noSpace` is not a heading — nothing on the line is formatted.
void TestHighlighter::headingWithoutSpace_isNotHeading()
{
    const MarkdownHighlighter h(nullptr, false);
    const QString line = QStringLiteral("#noSpace");
    QVERIFY(isPlain(fmtIn(h, line, 0)));
    QVERIFY(isPlain(fmtIn(h, line, 1)));
}

// `**strong**` is bold; `*emph*` and `_emph_` are italic, not bold.
void TestHighlighter::strongIsBold_emphasisIsItalic()
{
    const QString line = QStringLiteral("x **strong** *emph* _un_ y");
    const MarkdownHighlighter light(nullptr, false);

    // Positions: x(0) sp(1) **strong**(2..11) sp(12) *emph*(13..18) sp(19)
    // _un_(20..23) sp(24) y(25).
    const auto strong = fmtIn(light, line, 4); // 's' of strong
    QVERIFY(bold(strong));
    QCOMPARE(color(strong), QColor(0x1D, 0x4E, 0xD8));
    QVERIFY(!italic(strong));

    const auto emph = fmtIn(light, line, 15); // 'm' of emph
    QVERIFY(italic(emph));
    QVERIFY(!bold(emph));
    QCOMPARE(color(emph), QColor(0x04, 0x78, 0x57));

    const auto un = fmtIn(light, line, 21); // 'u' of un
    QVERIFY(italic(un));
    QVERIFY(!bold(un));

    QVERIFY(isPlain(fmtIn(light, line, 0))); // leading "x"
    QVERIFY(isPlain(fmtIn(light, line, 25))); // trailing "y"
}

// The `*`-emphasis rule must NOT match the inner span of `**strong**`
// (look-around guard): the inner text is bold-only, never italic.
void TestHighlighter::emphasis_doesNotLeakIntoStrong()
{
    const MarkdownHighlighter h(nullptr, false);
    const QString line = QStringLiteral("a **bold** b");
    for (int i = 4; i <= 7; ++i) { // "bold"
        const auto f = fmtIn(h, line, i);
        QVERIFY(bold(f));
        QVERIFY2(!italic(f), qPrintable(QStringLiteral("pos %1 should not be italic").arg(i)));
    }
}

// `…` spans get the theme code color; surrounding text stays plain.
void TestHighlighter::inlineCode_getsCodeColor()
{
    const QString line = QStringLiteral("run `make all` now");
    const MarkdownHighlighter light(nullptr, false);
    const MarkdownHighlighter dark(nullptr, true);

    QCOMPARE(color(fmtIn(light, line, 5)), QColor(0xC2, 0x41, 0x0C)); // 'm' of make
    QCOMPARE(color(fmtIn(light, line, 10)), QColor(0xC2, 0x41, 0x0C)); // 'a' of all
    QCOMPARE(color(fmtIn(dark, line, 5)), QColor(0xFD, 0xBA, 0x74));
    QVERIFY(isPlain(fmtIn(light, line, 2))); // "n" of run
    QVERIFY(isPlain(fmtIn(light, line, 15))); // "n" of now
}

// `[text](url)` and `![alt](url)` are underlined with the link color,
// including the URL portion.
void TestHighlighter::linksAndImages_areUnderlined()
{
    const MarkdownHighlighter h(nullptr, false);
    const QString line = QStringLiteral("see [docs](http://x.y/z) here");
    // "see " = 4; [docs](http://x.y/z) spans 4..24.
    QVERIFY(underline(fmtIn(h, line, 5))); // 'd' of docs
    QCOMPARE(color(fmtIn(h, line, 5)), QColor(0x1D, 0x4E, 0xD8));
    QVERIFY(underline(fmtIn(h, line, 13))); // inside the URL
    QVERIFY(isPlain(fmtIn(h, line, 3))); // " " before the link
    QVERIFY(isPlain(fmtIn(h, line, 24))); // " " after the link

    const QString img = QStringLiteral("![alt](img.png)");
    QVERIFY(underline(fmtIn(h, img, 2))); // 'a' of alt
    QVERIFY(underline(fmtIn(h, img, 13))); // inside (img.png)
}

// A `> ` line is italic + tinted across the whole line.
void TestHighlighter::blockquote_isItalicTinted()
{
    const MarkdownHighlighter h(nullptr, false);
    const QString line = QStringLiteral("> quoted line");
    for (int i = 0; i < 13; ++i) {
        const auto f = fmtIn(h, line, i);
        QVERIFY(italic(f));
        QCOMPARE(color(f), QColor(0x6D, 0x28, 0xD9));
    }
}

// `---`, `***`, `___` (and longer runs) format the entire line.
void TestHighlighter::horizontalRule_isFormattedWholeLine()
{
    const MarkdownHighlighter h(nullptr, false);
    for (const QString &hr : {QStringLiteral("---"), QStringLiteral("***"),
                              QStringLiteral("___"), QStringLiteral("----")}) {
        for (int i = 0; i < hr.size(); ++i) {
            const auto f = fmtIn(h, hr, i);
            QVERIFY2(bold(f), qPrintable(QStringLiteral("hr %1 pos %2 not bold").arg(hr).arg(i)));
            QCOMPARE(color(f), QColor(0x94, 0xA3, 0xB8));
        }
    }
    // A lone `--` is not a rule.
    QVERIFY(isPlain(fmtIn(h, QStringLiteral("--"), 0)));
}

// `- item`, `* item`, `+ item`, `1. item` — the marker is bold+tinted, the
// item text is not.
void TestHighlighter::listMarkers_areFormatted()
{
    const MarkdownHighlighter h(nullptr, false);
    for (const QString &line : {QStringLiteral("- item"), QStringLiteral("* item"),
                                QStringLiteral("+ item"), QStringLiteral("1. item")}) {
        const auto marker = fmtIn(h, line, 0);
        QVERIFY(bold(marker));
        QCOMPARE(color(marker), QColor(0xC2, 0x41, 0x0C));
        QVERIFY(isPlain(fmtIn(h, line, line.size() - 1))); // last letter of "item"
    }
}

// Fenced blocks with the carried state: the ``` delimiter lines get the
// marker format, the content line gets the fence-content format (and NO
// inline rules inside — `**x**` in code stays un-bolded), the state ends on
// the closing fence, and inline highlighting resumes on the next line.
void TestHighlighter::fencedCode_isTrackedAcrossLines()
{
    const QString open = QStringLiteral("```c");
    const QString code = QStringLiteral("code **x** #not-heading");
    const QString close = QStringLiteral("```");
    const QString after = QStringLiteral("back **to**");

    const MarkdownHighlighter light(nullptr, false);
    const MarkdownHighlighter dark(nullptr, true);

    // Line 1: opening delimiter — marker format, state turns ON.
    {
        const auto res = light.formatsForLine(open, false);
        QCOMPARE(res.ranges.size(), 1);
        QCOMPARE(res.ranges[0].start, 0);
        QCOMPARE(res.ranges[0].count, open.size());
        QVERIFY(bold(fmtIn(light, open, 0)));
        QCOMPARE(color(fmtIn(light, open, 0)), QColor(0xC2, 0x41, 0x0C));
        QVERIFY(res.inFenceAfter);
    }

    // Line 2: in-fence content — fence format on every char, NO inline rules.
    {
        const auto res = light.formatsForLine(code, true);
        QCOMPARE(res.ranges.size(), 1);
        QCOMPARE(res.ranges[0].count, code.size());
        QCOMPARE(color(fmtIn(light, code, 0, true)), QColor(0x9A, 0x34, 0x12));
        QCOMPARE(color(fmtIn(dark, code, 0, true)), QColor(0xFC, 0xA5, 0xA5));
        QVERIFY(!bold(fmtIn(light, code, 11, true))); // '*' of **x** inside code
        QVERIFY(!bold(fmtIn(light, code, 16, true))); // '#' inside code
        QVERIFY(res.inFenceAfter);
    }

    // Line 3: closing delimiter — marker format, state turns OFF.
    {
        const auto res = light.formatsForLine(close, true);
        QVERIFY(bold(fmtIn(light, close, 0)));
        QCOMPARE(color(fmtIn(light, close, 0)), QColor(0xC2, 0x41, 0x0C));
        QVERIFY(!res.inFenceAfter);
    }

    // Line 4: outside the fence again — inline rules active.
    const int to = 5; // "back " = 5
    QVERIFY(bold(fmtIn(light, after, to)));
    QCOMPARE(color(fmtIn(light, after, to)), QColor(0x1D, 0x4E, 0xD8));
}

// setDark() swaps the whole color map and repaints (re-highlight). A real
// document is passed here (not nullptr) because setDark() also re-highlights.
void TestHighlighter::setDark_rethemesColorMap()
{
    QTextDocument doc;
    MarkdownHighlighter h(&doc, false);
    const QString line = QStringLiteral("code `x` and **b**");
    const QColor codeLight = color(fmtIn(h, line, 5));
    const QColor strongLight = color(fmtIn(h, line, 15));
    QVERIFY(!h.dark());

    h.setDark(true);
    QVERIFY(h.dark());
    QCOMPARE(color(fmtIn(h, line, 5)), QColor(0xFD, 0xBA, 0x74));
    QCOMPARE(color(fmtIn(h, line, 15)), QColor(0x93, 0xC5, 0xFD));
    QVERIFY(color(fmtIn(h, line, 5)) != codeLight);
    QVERIFY(color(fmtIn(h, line, 15)) != strongLight);

    // Back to light restores the original map.
    h.setDark(false);
    QCOMPARE(color(fmtIn(h, line, 5)), codeLight);
    QCOMPARE(color(fmtIn(h, line, 15)), strongLight);
}

QTEST_GUILESS_MAIN(TestHighlighter)
#include "test_highlighter.moc"
