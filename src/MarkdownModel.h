// MarkdownModel — pure, GUI-independent markdown text logic (subtask 4.1).
//
// Holds the non-widget text analysis the editor needs:
//   * heading extraction (ATX `#`/`##`/... → level + text) for an outline and
//     for content tests — headings inside fenced code blocks are NOT counted;
//   * word / character counting (the canonical implementation; EditorPane
//     delegates to these so the status-bar counts and the model agree);
//   * the PreviewDebouncer policy — a pure "render now, or wait N ms" decision
//     from an elapsed-since-last-edit time, so the subtask-4.2 live-preview
//     wiring (a GUI-thread QTimer + QElapsedTimer) stays a dumb timer around
//     this testable policy.
//
// No QWidget, no QTextDocument — only QString/QVector, so it is exercised
// headlessly by test_markdownmodel with no QApplication at all.
#pragma once

#include <QString>
#include <QVector>

// One extracted heading: its level (1..6) and its text (closing hash
// sequences stripped, surrounding whitespace trimmed; inline markdown such as
// `**bold**` is kept verbatim — this is an outline, not a renderer).
struct MarkdownHeading
{
    int level = 0;
    QString text;
};

class MarkdownModel
{
public:
    // Extract the ATX headings of `text` in document order.
    //
    // A heading is a line whose first non-space content is 1..6 `#` characters
    // followed by a space or tab and non-empty text (CommonMark: `#noSpace`
    // and `#######` are NOT headings; `   # x` with ≤3 leading spaces is). An
    // optional closing sequence of `#`s — preceded by a space/tab and followed
    // only by whitespace — is stripped from the text, as is surrounding
    // whitespace (`## Title ##` → level 2, text "Title").
    //
    // Lines inside fenced code blocks (``` … ``` or ~~~ … ~~~, the CommonMark
    // fence rule) are NOT headings, even if they look like one. Empty headings (`#` alone, or only spaces after the hashes)
    // are skipped — they carry no outline text.
    static QVector<MarkdownHeading> headings(const QString &text);

    // Number of whitespace-separated words (leading/trailing blanks ignored);
    // an empty string is 0 words.
    static int wordCount(const QString &text);
    // Number of Unicode characters (QChar count — includes spaces and line
    // breaks); an empty string is 0.
    static int charCount(const QString &text);
};

// The pure debounce policy behind the live preview (subtask 4.2 schedules the
// actual GUI-thread QTimer around this): given how long has elapsed since the
// last editor change, decide whether the preview must render now or how long
// the timer should wait.
class PreviewDebouncer
{
public:
    struct Decision
    {
        bool renderNeeded = false; // true  → render the preview now
        int waitMs = 0;            // false → wait this long, then re-evaluate
    };

    // The default debounce interval: 150 ms, the middle of the spec's
    // "~120–200 ms" window.
    static int defaultIntervalMs() { return 150; }

    explicit PreviewDebouncer(int intervalMs = defaultIntervalMs());

    int intervalMs() const { return m_intervalMs; }
    void setIntervalMs(int ms) { m_intervalMs = ms; }

    // Evaluate with this instance's interval (see the static overload).
    Decision evaluate(int elapsedMs) const;

    // The pure policy: elapsedMs = time since the last editor change,
    // intervalMs = the debounce window. Negative inputs are clamped to 0.
    //   * elapsedMs >= intervalMs → {renderNeeded = true,  waitMs = 0}
    //   * else                   → {renderNeeded = false, waitMs = intervalMs - elapsedMs}
    // An interval of 0 means "render on every change" (waitMs is always 0).
    static Decision evaluate(int elapsedMs, int intervalMs);

private:
    int m_intervalMs;
};
