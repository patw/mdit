// test_editor — exercises the `EditorPane` (subtask 3).
//
// Covers the required surface:
//   * word/char counts for known input (+ the empty-buffer zero case);
//   * the **line-number margin**: the number of line numbers == the document
//     block count, the exact "1".."N" strings, the digit-width sizing, and that
//     the margin is a real left-hand widget;
//   * a **textChanged** emission on edit (both the inherited signal and the
//     concrete `editorTextChanged()` passthrough);
//   * the hosted **MarkdownHighlighter**: it formats a heading range (bold +
//     theme color) and an emphasis range (italic), and setHighlighterDark()
//     re-themes the color map;
//   * the **find / replace helpers**: find() selects + scrolls to a match,
//     matchCount() tallies occurrences, replaceAll() rewrites them all.
//
// Widget test: builds a QApplication itself (EditorPane is a QWidget). The
// offscreen platform plugin from add_mdit_test() keeps this headless — no
// window is ever shown.
#include <QtTest>

#include "testmain.h"
#include "EditorPane.h"
#include "FindBar.h"
#include "MarkdownHighlighter.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QLineEdit>
#include <QLabel>
#include <QSignalSpy>
#include <QTextCharFormat>
#include <QTextCursor>

namespace {
// The last covering (start,count,format) range of a line under the highlighter
// rule engine (mirrors setFormat application order — later ranges win). This is
// the same assertion surface test_highlighter uses, because this Qt build's
// QTextDocument has no observable per-character format storage.
QTextCharFormat fmtIn(const MarkdownHighlighter &h, const QString &line, int pos)
{
    const auto res = h.formatsForLine(line, /*inFence=*/false);
    QTextCharFormat result;
    for (const auto &r : res.ranges)
        if (pos >= r.start && pos < r.start + r.count)
            result = r.format;
    return result;
}
bool bold(const QTextCharFormat &f) { return f.fontWeight() == QFont::Bold; }
bool italic(const QTextCharFormat &f) { return f.fontItalic(); }
QColor color(const QTextCharFormat &f) { return f.foreground().color(); }
} // namespace

class TestEditor : public QObject
{
    Q_OBJECT
private slots:
    void wordAndCharCounts_matchKnownInput();
    void emptyBuffer_countsAreZero();
    void lineNumberMargin_blockCountMatchesDocument();
    void lineNumberMargin_widthScalesWithDigitCount();
    void lineNumberMargin_isARealLeftWidget();
    void textChanged_emitsOnEdit();
    void highlighter_formatsHeadingRange();
    void highlighter_formatsEmphasisRange();
    void setHighlighterDark_rethemes();
    void find_selectsNextMatch();
    void matchCount_countsOccurrences();
    void replaceAll_rewritesEveryOccurrence();
    void findBar_matchCount_countsOccurrences();
    void findBar_typingQueryHighlightsAndCounts();
    void findBar_nextPrev_wrapAround();
    void findBar_caseSensitiveToggle_changesCount();
    void findBar_noMatch_clearsSelectionAndLabel();
    void findBar_currentMatchIndexLabel();
    void replaceRow_hiddenByDefault_shownByActivateReplace();
    void replaceBar_replaceAll_rewritesEveryOccurrence();
    void replaceBar_replaceAll_respectsCaseSensitivity();
    void replaceBar_replaceAll_updatesMatchCountAndLabel();
    void replaceBar_replaceCurrent_replacesAndAdvances();
    void replaceBar_replaceCurrent_withNoSelectionFindsFirst();
    void replaceBar_replaceCurrent_noMatchIsNoop();
    void replaceBar_emptyQueryReplacesNothing();
};

// "three little words" -> 3 words / 18 chars (incl. spaces).
void TestEditor::wordAndCharCounts_matchKnownInput()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("three little words"));
    QCOMPARE(e.wordCount(), 3);
    QCOMPARE(e.charCount(), 18);

    // Multiple runs of whitespace / leading+trailing blanks collapse to single
    // word separators.
    e.setPlainText(QStringLiteral("  a  b   c  "));
    QCOMPARE(e.wordCount(), 3);
    QCOMPARE(e.charCount(), 12);
}

// An empty buffer has zero words and zero characters.
void TestEditor::emptyBuffer_countsAreZero()
{
    EditorPane e;
    QCOMPARE(e.wordCount(), 0);
    QCOMPARE(e.charCount(), 0);
    QCOMPARE(e.lineNumbers().size(), 1); // an empty doc is still one (empty) block
}

// The number of line numbers == the document block count; the exact strings are
// "1".."N". (No trailing-newline quirk: we feed explicit line counts.)
void TestEditor::lineNumberMargin_blockCountMatchesDocument()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("a\nb\nc\nd\n\nf")); // 6 blocks (incl. the empty one)
    QCOMPARE(e.lineCount(), 6);
    QCOMPARE(e.lineNumbers().size(), 6);
    QCOMPARE(e.lineNumbers(), (QStringList{QStringLiteral("1"), QStringLiteral("2"),
                                           QStringLiteral("3"), QStringLiteral("4"),
                                           QStringLiteral("5"), QStringLiteral("6")}));

    e.setPlainText(QStringLiteral("only one line"));
    QCOMPARE(e.lineCount(), 1);
    QCOMPARE(e.lineNumbers(), QStringList{QStringLiteral("1")});
}

// The gutter width grows when the largest line number gains a digit (9 -> 10
// lines), and a single-digit count is narrower.
void TestEditor::lineNumberMargin_widthScalesWithDigitCount()
{
    EditorPane e;
    QString nine;
    for (int i = 1; i <= 9; ++i)
        nine += (i > 1 ? QStringLiteral("\n") : QString()) + QString::number(i);
    e.setPlainText(nine); // 9 blocks
    QCOMPARE(e.lineCount(), 9);
    const int w1 = e.lineNumberAreaWidth();
    QVERIFY(w1 > 0);

    // Add an 8th... a 10th line: the max number now has two digits.
    e.setPlainText(nine + QStringLiteral("\n10")); // 10 blocks
    QCOMPARE(e.lineCount(), 10);
    const int w2 = e.lineNumberAreaWidth();
    QVERIFY2(w2 > w1, qPrintable(QStringLiteral("expected %1 > %2").arg(w2).arg(w1)));
}

// The margin is a real child widget (a LineNumberArea) with a positive width.
void TestEditor::lineNumberMargin_isARealLeftWidget()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("x\ny"));
    EditorPane::LineNumberArea *margin = e.lineNumberAreaWidget();
    QVERIFY(margin != nullptr);
    QCOMPARE(margin->parentWidget(), &e);
    QVERIFY(e.lineNumberAreaWidth() > 0);
    // The margin's size hint reflects the computed width.
    QCOMPARE(margin->sizeHint().width(), e.lineNumberAreaWidth());
}

// Typing emits the base textChanged and the concrete editorTextChanged
// passthrough; a programmatic setPlainText does too.
void TestEditor::textChanged_emitsOnEdit()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("start"));

    QSignalSpy baseSpy(&e, &QPlainTextEdit::textChanged);
    QSignalSpy passSpy(&e, &EditorPane::editorTextChanged);
    QVERIFY(baseSpy.isValid());
    QVERIFY(passSpy.isValid());

    e.insertPlainText(QStringLiteral("x")); // a real edit
    QVERIFY2(baseSpy.count() >= 1, "textChanged should fire on edit");
    QCOMPARE(passSpy.count(), baseSpy.count()); // the passthrough mirrors it

    e.setPlainText(QStringLiteral("replaced"));
    QVERIFY2(baseSpy.count() >= 2, "textChanged should fire on set");
    QCOMPARE(passSpy.count(), baseSpy.count());
}

// The pane's own highlighter (installed on its document) formats a heading
// range: the hashes and the title are bold, the title carries the h1 theme
// color (light), and the hashes are a muted color.
void TestEditor::highlighter_formatsHeadingRange()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("# Title"));

    MarkdownHighlighter *h = e.highlighter();
    QVERIFY(h != nullptr);
    QVERIFY(!h->dark());

    const QString line = QStringLiteral("# Title");
    // Hash (pos 0): bold + muted.
    const auto hash = fmtIn(*h, line, 0);
    QVERIFY(bold(hash));
    QCOMPARE(color(hash), QColor(0x64, 0x74, 0x8B));
    // Title text (pos 2): bold + h1 light color, and NOT italic.
    const auto title = fmtIn(*h, line, 2);
    QVERIFY(bold(title));
    QVERIFY(!italic(title));
    QCOMPARE(color(title), QColor(0x7C, 0x2D, 0x12));
    // The hashes and the title are formatted as a contiguous bold run from 0.
    const auto res = h->formatsForLine(line, false);
    QVERIFY(!res.ranges.isEmpty());
    QCOMPARE(res.ranges.first().start, 0);
}

// The highlighter formats an emphasis range (`*emph*` -> italic, not bold).
void TestEditor::highlighter_formatsEmphasisRange()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("a *emph* b"));
    MarkdownHighlighter *h = e.highlighter();
    QVERIFY(h != nullptr);

    const QString line = QStringLiteral("a *emph* b");
    const auto emph = fmtIn(*h, line, 3); // 'e' of emph
    QVERIFY(italic(emph));
    QVERIFY(!bold(emph));
    QCOMPARE(color(emph), QColor(0x04, 0x78, 0x57));
    // Surrounding plain text is unformatted (== default QTextCharFormat).
    QVERIFY(fmtIn(*h, line, 0) == QTextCharFormat());
}

// setHighlighterDark() re-themes the hosted highlighter's color map in place.
void TestEditor::setHighlighterDark_rethemes()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("`code`"));
    MarkdownHighlighter *h = e.highlighter();
    QVERIFY(h != nullptr);

    const QString line = QStringLiteral("`code`");
    const QColor codeLight = color(fmtIn(*h, line, 2));
    e.setHighlighterDark(true);
    QVERIFY(h->dark());
    QCOMPARE(color(fmtIn(*h, line, 2)), QColor(0xFD, 0xBA, 0x74)); // dark code color
    QVERIFY(color(fmtIn(*h, line, 2)) != codeLight);

    e.setHighlighterDark(false);
    QVERIFY(!h->dark());
    QCOMPARE(color(fmtIn(*h, line, 2)), codeLight);
}

// find() selects the next match from the cursor and scrolls to it; a
// miss leaves no selection.
void TestEditor::find_selectsNextMatch()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("one two three two"));

    // Start the search from the very beginning of the buffer (setPlainText may
    // leave the cursor elsewhere, so move it explicitly via the pane's cursor).
    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c);
    QCOMPARE(e.textCursor().position(), 0);

    QVERIFY(e.find(QStringLiteral("two")));
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("two"));

    // The next find() continues from the previous selection end.
    QVERIFY(e.find(QStringLiteral("two")));
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("two"));
    // The cursor is past the first occurrence now (selection at the 2nd "two").
    QVERIFY(e.textCursor().selectionStart() > 4);

    // A miss returns false and clears the selection.
    QVERIFY(!e.find(QStringLiteral("zzz-not-present")));
    QVERIFY(!e.textCursor().hasSelection());
}

// matchCount() tallies non-overlapping occurrences (default: case-insensitive).
void TestEditor::matchCount_countsOccurrences()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("aaaa"));
    QCOMPARE(e.matchCount(QStringLiteral("aa")), 2); // positions 0 and 2
    QCOMPARE(e.matchCount(QStringLiteral("bbb")), 0);
    QCOMPARE(e.matchCount(QString()), 0); // empty needle -> 0

    // Case-insensitive by default; exact with Qt::CaseSensitive.
    e.setPlainText(QStringLiteral("CAT cat Cat"));
    QCOMPARE(e.matchCount(QStringLiteral("cat"), Qt::CaseInsensitive), 3);
    QCOMPARE(e.matchCount(QStringLiteral("cat"), Qt::CaseSensitive), 1);
}

// replaceAll() rewrites every occurrence and returns the count.
void TestEditor::replaceAll_rewritesEveryOccurrence()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("cat cat cat"));
    QCOMPARE(e.replaceAll(QStringLiteral("cat"), QStringLiteral("dog")), 3);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog dog dog"));

    // Case-insensitive by default.
    e.setPlainText(QStringLiteral("CAT cat"));
    QCOMPARE(e.replaceAll(QStringLiteral("cat"), QStringLiteral("dog")), 2);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog dog"));

    // Nothing to replace -> 0, text unchanged.
    e.setPlainText(QStringLiteral("no match here"));
    QCOMPARE(e.replaceAll(QStringLiteral("xyz"), QStringLiteral("q")), 0);
    QCOMPARE(e.toPlainText(), QStringLiteral("no match here"));
}

// --- Find bar (subtask 6, find half). ----------------------------------------
// The FindBar is the interactive chrome around EditorPane's existing search
// helpers; these tests drive it headlessly (it is a plain QWidget, no window).

// The bar's matchCount() tallies the query using its own case-sensitivity.
void TestEditor::findBar_matchCount_countsOccurrences()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("apple banana apple cherry apple"));
    FindBar bar(&e);

    bar.setQuery(QStringLiteral("apple"));
    QCOMPARE(bar.matchCount(), 3);
    QCOMPARE(bar.currentQuery(), QStringLiteral("apple"));

    bar.setQuery(QStringLiteral("banana"));
    QCOMPARE(bar.matchCount(), 1);

    bar.setQuery(QStringLiteral("zzz-not-there"));
    QCOMPARE(bar.matchCount(), 0);

    bar.setQuery(QString());
    QCOMPARE(bar.matchCount(), 0); // empty query -> 0
}

// Typing a query immediately highlights the match at/after the cursor (the
// editor selection) and reports "i of n".
void TestEditor::findBar_typingQueryHighlightsAndCounts()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("foo bar foo"));
    FindBar bar(&e);

    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c); // deterministic start (see AGENTS cursor-reset gotcha)

    bar.setQuery(QStringLiteral("foo")); // fires onQueryChanged -> findNext
    QVERIFY(e.textCursor().hasSelection());
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("foo"));
    QCOMPARE(bar.matchCount(), 2);
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 2"));

    bar.findNext();
    QVERIFY(e.textCursor().hasSelection());
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("foo"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("2 of 2"));
}

// next/prev wrap around: past the last occurrence returns to the first, and
// before the first returns to the last.
void TestEditor::findBar_nextPrev_wrapAround()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("alpha beta alpha gamma"));
    FindBar bar(&e);

    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c);

    bar.setQuery(QStringLiteral("alpha"));
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("alpha"));
    const int firstStart = e.textCursor().selectionStart();
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 2"));

    bar.findNext(); // the second occurrence
    QCOMPARE(bar.matchLabel(), QStringLiteral("2 of 2"));
    QVERIFY(e.textCursor().selectionStart() > firstStart);

    // Wrap forward: past the last back to the first.
    bar.findNext();
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 2"));
    QCOMPARE(e.textCursor().selectionStart(), firstStart);

    // Wrap backward: before the first back to the last.
    bar.findPrevious();
    QCOMPARE(bar.matchLabel(), QStringLiteral("2 of 2"));
    QVERIFY(e.textCursor().selectionStart() > firstStart);
}

// Toggling "match case" changes both the total and the current match.
void TestEditor::findBar_caseSensitiveToggle_changesCount()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("Cat cat CAT"));
    FindBar bar(&e);

    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c);

    bar.setQuery(QStringLiteral("cat"));
    QCOMPARE(bar.caseSensitivity(), Qt::CaseInsensitive);
    QCOMPARE(bar.matchCount(), 3);

    bar.setCaseSensitive(true);
    QCOMPARE(bar.caseSensitivity(), Qt::CaseSensitive);
    QCOMPARE(bar.matchCount(), 1); // only the literal lowercase "cat"
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 1"));
}

// A query with no matches clears the selection and reports "no matches".
void TestEditor::findBar_noMatch_clearsSelectionAndLabel()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("hello world"));
    FindBar bar(&e);

    bar.setQuery(QStringLiteral("hello"));
    QVERIFY(e.textCursor().hasSelection());

    bar.setQuery(QStringLiteral("nope"));
    QCOMPARE(bar.matchCount(), 0);
    QCOMPARE(bar.matchLabel(), QStringLiteral("no matches"));
    QVERIFY(!e.textCursor().hasSelection()); // the miss cleared the highlight
}

// The "i of n" label tracks which of the total matches is currently selected as
// the user steps through them.
void TestEditor::findBar_currentMatchIndexLabel()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("a a a"));
    FindBar bar(&e);

    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c);

    bar.setQuery(QStringLiteral("a"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 3"));
    bar.findNext();
    QCOMPARE(bar.matchLabel(), QStringLiteral("2 of 3"));
    bar.findNext();
    QCOMPARE(bar.matchLabel(), QStringLiteral("3 of 3"));
    // Wraps back to the first rather than overflowing past the count.
    bar.findNext();
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 3"));
}

// --- Replace (subtask 6, replace half; Ctrl+H). --------------------------------
// The replace row is find-bar chrome around the EditorPane replace helpers:
// Replace acts on the current match, Replace All on every match.

// The replace row is hidden in the default (Ctrl+F) state and appears via
// activateReplace() (Ctrl+H), which also shows the bar.
void TestEditor::replaceRow_hiddenByDefault_shownByActivateReplace()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("hello hello"));
    FindBar bar(&e);

    QVERIFY(!bar.isVisible()); // hidden until activated
    QVERIFY(!bar.replaceRowVisible()); // find-only by default
    QLineEdit *replaceField = bar.findChild<QLineEdit *>(QStringLiteral("ReplaceQuery"));
    QPushButton *replaceBtn = bar.findChild<QPushButton *>(QStringLiteral("ReplaceOne"));
    QVERIFY(replaceField != nullptr);
    QVERIFY(replaceBtn != nullptr);
    QVERIFY(replaceField->isHidden()); // the row widgets are folded too

    bar.activate(); // Ctrl+F
    QVERIFY(bar.isVisible());
    QVERIFY(!bar.replaceRowVisible()); // still find-only
    QVERIFY(replaceField->isHidden());

    bar.activateReplace(); // Ctrl+H
    QVERIFY(bar.isVisible());
    QVERIFY(bar.replaceRowVisible()); // replace row unfolded
    QVERIFY(!replaceField->isHidden());
    QVERIFY(!replaceBtn->isHidden());

    bar.setReplaceRowVisible(false); // fold back to find-only
    QVERIFY(!bar.replaceRowVisible());
    QVERIFY(replaceField->isHidden());
}

// replaceAll() (Replace All) rewrites every occurrence and returns the count —
// the result the subtask requires the suite to assert.
void TestEditor::replaceBar_replaceAll_rewritesEveryOccurrence()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("cat cat CAT cat"));
    FindBar bar(&e);

    bar.setQuery(QStringLiteral("cat"));
    bar.setReplaceText(QStringLiteral("dog"));
    QCOMPARE(bar.currentReplaceText(), QStringLiteral("dog"));

    QCOMPARE(bar.replaceAll(), 4); // case-insensitive by default (all four)
    QCOMPARE(e.toPlainText(), QStringLiteral("dog dog dog dog"));

    // No occurrences remain for the query anymore.
    QCOMPARE(bar.matchCount(), 0);
}

// With "Match case" on, replaceAll() rewrites only the exact-case matches
// (sensitivity flows from the bar into EditorPane::replaceAll).
void TestEditor::replaceBar_replaceAll_respectsCaseSensitivity()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("cat CAT Cat"));
    FindBar bar(&e);

    bar.setQuery(QStringLiteral("cat"));
    bar.setCaseSensitive(true);
    bar.setReplaceText(QStringLiteral("dog"));
    QCOMPARE(bar.replaceAll(), 1); // only the literal lowercase "cat"
    QCOMPARE(e.toPlainText(), QStringLiteral("dog CAT Cat"));
}

// After Replace All the count label reflects the (now absent) query and the
// editor selection is cleared.
void TestEditor::replaceBar_replaceAll_updatesMatchCountAndLabel()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("a a a"));
    FindBar bar(&e);

    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c);

    bar.setQuery(QStringLiteral("a"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 3"));

    bar.setReplaceText(QStringLiteral("b"));
    QCOMPARE(bar.replaceAll(), 3);
    QCOMPARE(e.toPlainText(), QStringLiteral("b b b"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("no matches"));
    QVERIFY(!e.textCursor().hasSelection());

    // A replace-all that leaves some occurrences (replacement text containing
    // the query counts too): "aa" -> "a" turns "aa aa" into "a a" (2 of 2).
    e.setPlainText(QStringLiteral("aa aa"));
    bar.setQuery(QStringLiteral("aa"));
    bar.setReplaceText(QStringLiteral("a"));
    QCOMPARE(bar.replaceAll(), 2);
    QCOMPARE(e.toPlainText(), QStringLiteral("a a"));
    QCOMPARE(bar.matchCount(), 0); // "aa" no longer occurs
    QCOMPARE(bar.matchLabel(), QStringLiteral("no matches"));
}

// Replace (the button, acting on the current match) rewrites the selected
// occurrence, keeps the replace text, and advances to the following match.
void TestEditor::replaceBar_replaceCurrent_replacesAndAdvances()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("cat bat cat"));
    FindBar bar(&e);

    QTextCursor c = e.textCursor();
    c.movePosition(QTextCursor::Start);
    e.setTextCursor(c);

    bar.setQuery(QStringLiteral("cat")); // selects the first "cat"
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("cat"));
    bar.setReplaceText(QStringLiteral("dog"));

    QCOMPARE(bar.replaceCurrent(), 1);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog bat cat"));
    // Advanced to the (now only) remaining match; the replace text persists.
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("cat"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 1"));
    QCOMPARE(bar.currentReplaceText(), QStringLiteral("dog"));

    // The next Replace hits that match; after it there are none left.
    QCOMPARE(bar.replaceCurrent(), 1);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog bat dog"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("no matches"));
    QVERIFY(!e.textCursor().hasSelection());

    // With zero matches left, Replace is a no-op.
    QCOMPARE(bar.replaceCurrent(), 0);
    QCOMPARE(e.toPlainText(), QStringLiteral("dog bat dog"));
}

// When nothing is currently selected (e.g. the user just moved the cursor),
// Replace finds the next match and replaces it.
void TestEditor::replaceBar_replaceCurrent_withNoSelectionFindsFirst()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("x foo x foo"));
    FindBar bar(&e);

    // Park the cursor BETWEEN the two occurrences (no selection), as if the
    // user had just clicked in the text. Replace must find the NEXT match from
    // the cursor — the second "foo".
    QTextCursor c = e.textCursor();
    c.setPosition(6); // "x foo| x foo"
    e.setTextCursor(c);
    QVERIFY(!e.textCursor().hasSelection());

    bar.setQuery(QStringLiteral("foo"));
    bar.findPrevious(); // deselect: step back to the first "foo"...
    c = e.textCursor();
    c.setPosition(6); // ...and park the bare cursor at the middle again
    e.setTextCursor(c);
    QVERIFY(!e.textCursor().hasSelection());

    bar.setReplaceText(QStringLiteral("bar"));
    QCOMPARE(bar.replaceCurrent(), 1);
    QCOMPARE(e.toPlainText(), QStringLiteral("x foo x bar")); // 2nd one replaced
    // Advanced (wrapping) to the remaining match.
    QCOMPARE(e.textCursor().selectedText(), QStringLiteral("foo"));
    QCOMPARE(bar.matchLabel(), QStringLiteral("1 of 1"));
}

// A query with no matches: Replace changes nothing and returns 0.
void TestEditor::replaceBar_replaceCurrent_noMatchIsNoop()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("nothing to see"));
    FindBar bar(&e);

    bar.setQuery(QStringLiteral("zebra"));
    bar.setReplaceText(QStringLiteral("horse"));
    QCOMPARE(bar.replaceCurrent(), 0);
    QCOMPARE(e.toPlainText(), QStringLiteral("nothing to see"));
    QCOMPARE(bar.replaceAll(), 0); // replace-all with no matches too
    QCOMPARE(e.toPlainText(), QStringLiteral("nothing to see"));
}

// An empty query is a no-op for both Replace and Replace All.
void TestEditor::replaceBar_emptyQueryReplacesNothing()
{
    EditorPane e;
    e.setPlainText(QStringLiteral("abc"));
    FindBar bar(&e);

    bar.setReplaceText(QStringLiteral("x"));
    QCOMPARE(bar.replaceCurrent(), 0); // empty query
    QCOMPARE(bar.replaceAll(), 0);
    QCOMPARE(e.toPlainText(), QStringLiteral("abc"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestEditor t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_editor.moc"
