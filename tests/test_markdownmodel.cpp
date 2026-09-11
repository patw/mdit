// test_markdownmodel — subtask 4.1: the pure MarkdownModel text logic.
//
// Covers heading extraction (levels + text, closing-hash stripping,
// non-headings, fenced-code exclusion, indentation rules), word/char
// counting, and the PreviewDebouncer policy (render-now vs wait-N-ms).
//
// Pure-logic test: QTEST_GUILESS_MAIN (no QApplication at all).
#include "MarkdownModel.h"

#include <QtTest>

class TestMarkdownModel : public QObject
{
    Q_OBJECT

private slots:
    // -- heading extraction -------------------------------------------------

    void headingsLevelsAndText()
    {
        const QVector<MarkdownHeading> h =
            MarkdownModel::headings(QStringLiteral("# Alpha\n## Beta\n### Gamma\n"
                                                   "###### Omega"));
        QCOMPARE(h.size(), 4);
        QCOMPARE(h[0].level, 1);
        QCOMPARE(h[0].text, QStringLiteral("Alpha"));
        QCOMPARE(h[1].level, 2);
        QCOMPARE(h[1].text, QStringLiteral("Beta"));
        QCOMPARE(h[2].level, 3);
        QCOMPARE(h[2].text, QStringLiteral("Gamma"));
        QCOMPARE(h[3].level, 6);
        QCOMPARE(h[3].text, QStringLiteral("Omega"));
    }

    void headingsStripClosingHashesAndWhitespace()
    {
        const QVector<MarkdownHeading> h =
            MarkdownModel::headings(QStringLiteral("## Title ##\n### Spaced   ###   \n"
                                                   "# C#\n# C# #"));
        QCOMPARE(h.size(), 4);
        QCOMPARE(h[0].text, QStringLiteral("Title"));
        QCOMPARE(h[1].text, QStringLiteral("Spaced"));
        // No space before the trailing `#` in "C#" -> it is content, not a
        // closing sequence.
        QCOMPARE(h[2].text, QStringLiteral("C#"));
        QCOMPARE(h[3].text, QStringLiteral("C#"));
    }

    void headingsSkipNonHeadings()
    {
        const QVector<MarkdownHeading> h = MarkdownModel::headings(
            QStringLiteral("#noSpace\n####### seven hashes\n#\n#   \ntext line"));
        // `#noSpace` (no space after the hashes), `#######` (7 hashes), a bare
        // `#`, and `#` + only spaces (empty heading) are all excluded.
        QCOMPARE(h.size(), 0);
    }

    void headingsHonorIndentation()
    {
        const QVector<MarkdownHeading> h =
            MarkdownModel::headings(QStringLiteral("  # two spaces\n"
                                                   "     # four spaces"));
        // ≤3 leading spaces is a CommonMark heading; 4 spaces is an indented
        // code block, not a heading.
        QCOMPARE(h.size(), 1);
        QCOMPARE(h[0].level, 1);
        QCOMPARE(h[0].text, QStringLiteral("two spaces"));
    }

    void headingsIgnoreFencedCode()
    {
        const QString md = QStringLiteral(
            "# Real\n"
            "```c\n"
            "# not a heading\n"
            "# also not\n"
            "```\n"
            "~~~\n"
            "# not either\n"
            "~~~\n"
            "## Real two");
        const QVector<MarkdownHeading> h = MarkdownModel::headings(md);
        QCOMPARE(h.size(), 2);
        QCOMPARE(h[0].level, 1);
        QCOMPARE(h[0].text, QStringLiteral("Real"));
        QCOMPARE(h[1].level, 2);
        QCOMPARE(h[1].text, QStringLiteral("Real two"));
    }

    // -- counts ---------------------------------------------------------------

    void wordCount()
    {
        QCOMPARE(MarkdownModel::wordCount(QString()), 0);
        QCOMPARE(MarkdownModel::wordCount(QStringLiteral("   \n\t  ")), 0);
        QCOMPARE(MarkdownModel::wordCount(QStringLiteral("hello world")), 2);
        QCOMPARE(MarkdownModel::wordCount(QStringLiteral("  a  b  ")), 2);
        QCOMPARE(MarkdownModel::wordCount(QStringLiteral("one\ntwo\nthree")), 3);
        QCOMPARE(MarkdownModel::wordCount(QStringLiteral("a\tb\nc")), 3);
        // Non-ASCII words count as words too.
        QCOMPARE(MarkdownModel::wordCount(QStringLiteral("héllo wörld 中文")), 3);
    }

    void charCount()
    {
        QCOMPARE(MarkdownModel::charCount(QString()), 0);
        QCOMPARE(MarkdownModel::charCount(QStringLiteral("a b\n")), 4);
        QCOMPARE(MarkdownModel::charCount(QStringLiteral("héllo")), 5);
    }

    // -- debounce policy -------------------------------------------------------

    void debounceDecision()
    {
        // Not yet quiet enough → wait the remainder of the window.
        PreviewDebouncer::Decision d = PreviewDebouncer::evaluate(0, 150);
        QVERIFY(!d.renderNeeded);
        QCOMPARE(d.waitMs, 150);
        d = PreviewDebouncer::evaluate(149, 150);
        QVERIFY(!d.renderNeeded);
        QCOMPARE(d.waitMs, 1);

        // The interval edge and anything past it → render now.
        d = PreviewDebouncer::evaluate(150, 150);
        QVERIFY(d.renderNeeded);
        QCOMPARE(d.waitMs, 0);
        d = PreviewDebouncer::evaluate(5000, 150);
        QVERIFY(d.renderNeeded);
        QCOMPARE(d.waitMs, 0);
    }

    void debounceClampsAndZeroInterval()
    {
        // Negative elapsed is clamped to the full wait.
        PreviewDebouncer::Decision d = PreviewDebouncer::evaluate(-30, 150);
        QVERIFY(!d.renderNeeded);
        QCOMPARE(d.waitMs, 150);

        // A zero interval means "render on every change".
        d = PreviewDebouncer::evaluate(0, 0);
        QVERIFY(d.renderNeeded);
        QCOMPARE(d.waitMs, 0);
    }

    void debouncerInstance()
    {
        // The default interval is the middle of the spec's ~120–200 ms window.
        QCOMPARE(PreviewDebouncer::defaultIntervalMs(), 150);
        PreviewDebouncer deb;
        QCOMPARE(deb.intervalMs(), 150);
        QVERIFY(!deb.evaluate(100).renderNeeded);
        QCOMPARE(deb.evaluate(100).waitMs, 50);
        QVERIFY(deb.evaluate(150).renderNeeded);

        // A custom interval is honored and settable.
        PreviewDebouncer fast(25);
        QCOMPARE(fast.intervalMs(), 25);
        QVERIFY(fast.evaluate(25).renderNeeded);
        fast.setIntervalMs(80);
        QCOMPARE(fast.intervalMs(), 80);
        QVERIFY(!fast.evaluate(79).renderNeeded);
        QCOMPARE(fast.evaluate(79).waitMs, 1);
        QVERIFY(fast.evaluate(80).renderNeeded);
    }
};

QTEST_GUILESS_MAIN(TestMarkdownModel)
#include "test_markdownmodel.moc"
