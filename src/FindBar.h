// FindBar — the Ctrl+F find dialog (subtask 6, find half; popup since the
// post-loop polish).
//
// A small **non-modal popup window** — a `QDialog` centered over the host
// window every time it is shown (it used to be an overlay glued to the
// top-left of the window, which sat under the menu bar and looked wrong). It
// drives the `EditorPane`'s existing search helpers (`find()`,
// `findPrevious()`, `matchCount()`) — so the *search logic* is the single,
// already-tested implementation in EditorPane, and this widget is only the
// interactive chrome around it:
//   * a **search field** (QLineEdit) — typing re-runs the match count and, when
//     a query is present, highlights the match at/after the cursor;
//   * **next / previous** buttons — move to the following / preceding match,
//     **wrapping** around at the ends (past the bottom -> top, before the top
//     -> bottom), and clearing the selection when there is no match at all;
//   * a **match-count label** — `"<i> of <n>"` for the current match index out
//     of the total `<n>` (or `"<n> matches"` / `"no matches"` when nothing is
//     currently selected);
//   * a **case-sensitive toggle**;
//   * a **close** button (Esc) and Enter/Return = next, Shift+Enter = previous;
//   * a **replace row** (shown by Ctrl+H / activateReplace()): a replace
//     field plus **Replace** (replace the current match, then advance to the
//     next — like Next) and **Replace All** (delegates to the single
//     EditorPane::replaceAll() implementation). Enter in the replace field =
//     Replace. The row is hidden while the dialog is find-only, and the window
//     title switches ("Find" / "Find and Replace") so the mode is obvious from
//     the title bar.
//
// The current match is highlighted simply by SELECTING it in the editor
// (QPlainTextEdit paints its selection); `EditorPane::find()` already selects +
// scrolls the hit into view, so the dialog just reports which of the `n` total
// matches the current selection is (a 1-based "i of n", never overflowing the
// count because wrap always re-selects a real occurrence).
//
// Closing it (Esc / the close button / the title-bar X) hides the window and
// returns the keyboard focus to the editor.
//
// Headless-safe: everything is a plain QDialog/QLineEdit/QPushButton/QCheckBox/
// QLabel and is exercised under `QT_QPA_PLATFORM=offscreen` with a QApplication.
#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class EditorPane;
class QCloseEvent;
class QShowEvent;

class FindBar : public QDialog
{
    Q_OBJECT
public:
    explicit FindBar(EditorPane *editor, QWidget *parent = nullptr);

    // Bring the dialog to the front (centered over the host window), give focus
    // to the search field (selecting any existing text for a quick retype), show
    // it if hidden, and — if a query is already present — highlight the next
    // match. Called on Ctrl+F.
    void activate();

    // Center the dialog's frame over the host window (parentWidget()->window(),
    // i.e. the main window in the app; the primary screen when there is no
    // parent). Called on every show and whenever the dialog grows/shrinks. Public
    // so a test can assert the centering.
    void centerOnParent();

    // Search forward to the next match, wrapping from the bottom to the top.
    // When the query has no matches at all, the selection is cleared.
    void findNext();
    // Search backward to the previous match, wrapping from the top to the
    // bottom. Same no-match behavior as findNext().
    void findPrevious();

    // The number of (non-overlapping) matches of the current query in the
    // editor, using the bar's current case-sensitivity.
    int matchCount() const;
    // The query text currently in the search field.
    QString currentQuery() const { return m_queryEdit->text(); }
    // The bar's current case-sensitivity (from the case toggle).
    Qt::CaseSensitivity caseSensitivity() const;
    // The match-count label's current text (e.g. "3 of 5", "5 matches",
    // "no matches", or "" when the query is empty). Exposed for tests.
    QString matchLabel() const { return m_countLabel->text(); }

    // Set the search field's text (fires textChanged -> an immediate search,
    // exactly like typing). A test seam mirroring the real interaction.
    void setQuery(const QString &q) { m_queryEdit->setText(q); }
    // Toggle the case-sensitive checkbox (fires toggled -> re-search).
    void setCaseSensitive(bool on) { m_caseBox->setChecked(on); }

    // Close/hide the bar (the Ctrl+F close affordance). Emits closed().
    void closeFindBar();

    // -- Replace (subtask 6, replace half; shown by Ctrl+H). -----------------
    // Show the bar AND the replace row (Ctrl+H). Focuses the search field when
    // the query is empty, otherwise the replace field.
    void activateReplace();
    // Show/hide the replace row (search-only vs find+replace mode).
    void setReplaceRowVisible(bool on);
    bool replaceRowVisible() const { return m_replaceRowVisible; }

    // The replace text currently in the replace field.
    QString currentReplaceText() const { return m_replaceEdit->text(); }
    // Set the replace field's text (a test seam mirroring the real interaction;
    // typing in the field does not re-run the search, so this is side-effect
    // free).
    void setReplaceText(const QString &t) { m_replaceEdit->setText(t); }

    // Replace the **current match** (the editor's selected occurrence of the
    // query, found next if none is currently selected) with the replace text,
    // then advance to the following match (wrapping like findNext). Returns 1
    // on a replacement, 0 when the query is empty or has no matches at all.
    int replaceCurrent();
    // Replace **every** occurrence of the query with the replace text (via
    // EditorPane::replaceAll with the bar's case-sensitivity), update the
    // count label, and return the number of replacements made (0 when the
    // query is empty).
    int replaceAll();

    // Recompute the match-count label from the current query + selection
    // WITHOUT moving the selection. The host window calls this on the editor's
    // textChanged so the count stays current while the user edits with the bar
    // open.
    void refreshCount();

signals:
    // Emitted when the bar is closed (Esc / close button).
    void closed();

protected:
    // Esc closes the dialog.
    void keyPressEvent(QKeyEvent *event) override;
    // A widget-close event (the title-bar X) also counts as "closed".
    void closeEvent(QCloseEvent *event) override;
    // (Re)center over the host window on every show.
    void showEvent(QShowEvent *event) override;

private slots:
    void onQueryChanged();
    void onCaseToggled(bool on);

private:
    // The Qt::MatchFlags set to pass to EditorPane::find/findPrevious (case
    // sensitivity only — no regex/whole-word in v1).
    int findFlags() const;
    // Recompute the match-count label from the current query + selection.
    void updateMatchLabel();
    // Re-size the dialog to its current (one- or two-row) size hint and keep it
    // centered over the host window.
    void updateBarGeometry();

    EditorPane *m_editor;
    QLineEdit *m_queryEdit = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QCheckBox *m_caseBox = nullptr;
    QLabel *m_countLabel = nullptr;
    // The replace row (hidden until activateReplace()/setReplaceRowVisible(true)).
    QLineEdit *m_replaceEdit = nullptr;
    QPushButton *m_replaceBtn = nullptr;
    QPushButton *m_replaceAllBtn = nullptr;
    bool m_replaceRowVisible = false;
};
