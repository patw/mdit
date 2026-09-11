#include "MarkdownModel.h"

#include <QRegularExpression>

QVector<MarkdownHeading> MarkdownModel::headings(const QString &text)
{
    QVector<MarkdownHeading> out;

    // Fenced-code delimiter: up to 3 leading spaces, then 3+ backticks or
    // 3+ tildes (a trailing language tag is allowed), which toggles
    // fenced-code state line by line. (The cosmetic MarkdownHighlighter only
    // tracks backtick fences; CommonMark — which the renderer and this
    // extractor both follow for content — also treats ~~~ as a fence.)
    const QRegularExpression fence(QStringLiteral("^[ ]{0,3}(?:`{3,}|~{3,}).*$"));
    // 1..6 hashes at the start (up to 3 leading spaces), a space/tab, then the
    // (possibly empty) remainder. `#noSpace` and `#######…` do not match.
    const QRegularExpression atx(QStringLiteral("^[ ]{0,3}(#{1,6})[ \\t]+(.*)$"));
    // Optional closing sequence: a space/tab, then #s, then only whitespace.
    const QRegularExpression closing(QStringLiteral("[ \\t]+#+[ \\t]*$"));

    bool inFence = false;
    const QStringList lines = text.split(QChar('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.endsWith(QChar('\r')) ? raw.chopped(1) : raw;
        if (inFence) {
            if (fence.match(line).hasMatch())
                inFence = false; // closing delimiter
            continue;
        }
        if (fence.match(line).hasMatch()) {
            inFence = true; // opening delimiter
            continue;
        }
        const QRegularExpressionMatch m = atx.match(line);
        if (!m.hasMatch())
            continue;
        QString t = m.captured(2);
        const QRegularExpressionMatch c = closing.match(t);
        if (c.hasMatch())
            t = t.left(t.size() - c.capturedLength(0));
        t = t.trimmed();
        if (t.isEmpty())
            continue; // empty headings carry no outline text
        out.append(MarkdownHeading{int(m.captured(1).size()), t});
    }
    return out;
}

int MarkdownModel::wordCount(const QString &text)
{
    // Count whitespace-separated words (leading/trailing blanks ignored); an
    // empty string is 0 words.
    int count = 0;
    bool inWord = false;
    for (const QChar &c : text) {
        if (c.isSpace()) {
            inWord = false;
        } else {
            if (!inWord) {
                ++count;
                inWord = true;
            }
        }
    }
    return count;
}

int MarkdownModel::charCount(const QString &text)
{
    // QChar count — includes spaces and line breaks.
    return text.size();
}

PreviewDebouncer::PreviewDebouncer(int intervalMs)
    : m_intervalMs(intervalMs)
{
}

PreviewDebouncer::Decision PreviewDebouncer::evaluate(int elapsedMs) const
{
    return evaluate(elapsedMs, m_intervalMs);
}

PreviewDebouncer::Decision PreviewDebouncer::evaluate(int elapsedMs, int intervalMs)
{
    if (elapsedMs < 0)
        elapsedMs = 0;
    if (intervalMs < 0)
        intervalMs = 0;
    if (elapsedMs >= intervalMs)
        return Decision{true, 0};
    return Decision{false, intervalMs - elapsedMs};
}
