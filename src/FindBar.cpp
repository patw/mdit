#include "FindBar.h"

#include "EditorPane.h"

#include <QCloseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTextCursor>
#include <QTextDocument>

FindBar::FindBar(EditorPane *editor, QWidget *parent)
    : QDialog(parent), m_editor(editor)
{
    setObjectName(QStringLiteral("FindBar"));
    // A non-modal popup window with a title bar (movable, closable) — NOT an
    // overlay child: it is centered over the main window by activate()/
    // showEvent() instead of being glued under the menu bar. The title follows
    // the mode (find-only vs find+replace).
    setWindowTitle(tr("Find"));
    setModal(false);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    // A compact two-row grid (row 2 — the replace row — is hidden until
    // Ctrl+H / activateReplace()). The count label is given a stretch (1) so
    // it absorbs the spare width and the controls stay packed to the left.
    auto *lay = new QGridLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_queryEdit = new QLineEdit(this);
    m_queryEdit->setObjectName(QStringLiteral("FindQuery"));
    m_queryEdit->setClearButtonEnabled(true);
    m_queryEdit->setPlaceholderText(tr("Find"));
    m_queryEdit->setMinimumWidth(200);

    m_nextBtn = new QPushButton(tr("Next"), this);
    m_nextBtn->setObjectName(QStringLiteral("FindNext"));
    m_nextBtn->setDefault(true); // so pressing Enter in the field triggers it
    m_prevBtn = new QPushButton(tr("Previous"), this);
    m_prevBtn->setObjectName(QStringLiteral("FindPrevious"));

    m_caseBox = new QCheckBox(tr("Match case"), this);
    m_caseBox->setObjectName(QStringLiteral("FindMatchCase"));

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setObjectName(QStringLiteral("FindClose"));
    m_closeBtn->setAutoDefault(false);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("FindCount"));

    // The replace row: replace field + Replace (current match) + Replace All.
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setObjectName(QStringLiteral("ReplaceQuery"));
    m_replaceEdit->setPlaceholderText(tr("Replace with"));
    m_replaceEdit->setMinimumWidth(200);

    m_replaceBtn = new QPushButton(tr("Replace"), this);
    m_replaceBtn->setObjectName(QStringLiteral("ReplaceOne"));
    m_replaceBtn->setAutoDefault(false);

    m_replaceAllBtn = new QPushButton(tr("Replace All"), this);
    m_replaceAllBtn->setObjectName(QStringLiteral("ReplaceAll"));
    m_replaceAllBtn->setAutoDefault(false);

    lay->addWidget(m_queryEdit, 0, 0);
    lay->addWidget(m_nextBtn, 0, 1);
    lay->addWidget(m_prevBtn, 0, 2);
    lay->addWidget(m_caseBox, 0, 3);
    lay->addWidget(m_closeBtn, 0, 4);
    lay->addWidget(m_countLabel, 0, 5, 1, 1);
    lay->addWidget(m_replaceEdit, 1, 0);
    lay->addWidget(m_replaceBtn, 1, 1);
    lay->addWidget(m_replaceAllBtn, 1, 2);
    lay->setColumnStretch(5, 1);
    lay->setRowStretch(0, 1);
    lay->setRowStretch(1, 1);

    // Enter/Return in the field = next; Shift+Enter = previous; otherwise fall
    // through (so Esc and Tab still work).
    connect(m_queryEdit, &QLineEdit::returnPressed, this, [this]() {
        if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
            findPrevious();
        else
            findNext();
    });
    connect(m_queryEdit, &QLineEdit::textChanged, this, &FindBar::onQueryChanged);
    connect(m_nextBtn, &QPushButton::clicked, this, &FindBar::findNext);
    connect(m_prevBtn, &QPushButton::clicked, this, &FindBar::findPrevious);
    connect(m_caseBox, &QCheckBox::toggled, this, &FindBar::onCaseToggled);
    connect(m_closeBtn, &QPushButton::clicked, this, &FindBar::closeFindBar);

    // Replace row: Replace acts on the current match, Replace All on every
    // match — both delegate to the EditorPane helpers (single search/replace
    // implementation). Enter in the replace field = Replace.
    connect(m_replaceBtn, &QPushButton::clicked, this, &FindBar::replaceCurrent);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindBar::replaceAll);
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindBar::replaceCurrent);

    // Start hidden with the replace row folded (find-only); the first
    // Ctrl+F / activate() shows it, Ctrl+H / activateReplace() unfolds the
    // row. (The widgets start VISIBLE, so fold them explicitly — the
    // setReplaceRowVisible(false) guard would treat this as a no-op.)
    m_replaceEdit->hide();
    m_replaceBtn->hide();
    m_replaceAllBtn->hide();
    hide();
    updateBarGeometry();
}

void FindBar::activate()
{
    centerOnParent(); // place it over the main window before it appears
    show();
    raise();
    activateWindow();
    m_queryEdit->setFocus();
    m_queryEdit->selectAll();
    if (!m_queryEdit->text().isEmpty())
        findNext();
    else
        updateMatchLabel();
}

void FindBar::centerOnParent()
{
    // Center the dialog's frame on the host window (the app's main window); fall
    // back to the primary screen's available area when there is no visible
    // parent window (e.g. a FindBar built bare in a test).
    const QSize sz = frameGeometry().size();
    const QPoint offset(sz.width() / 2, sz.height() / 2);
    QWidget *host = parentWidget() ? parentWidget()->window() : nullptr;
    if (host && host->isVisible()) {
        move(host->frameGeometry().center() - offset);
        return;
    }
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        move(screen->availableGeometry().center() - offset);
}

void FindBar::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // A visible top-level window does not SHRINK through adjustSize(), so size
    // the popup to the current (one- or two-row) hint explicitly — otherwise the
    // platform hands it a taller default and the find-only dialog shows an empty
    // second row. Then center it (the size affects the centering offset).
    resize(sizeHint());
    centerOnParent(); // re-center on every show (Ctrl+F / Ctrl+H)
}

int FindBar::findFlags() const
{
    int flags = 0;
    if (m_caseBox->isChecked())
        flags |= Qt::MatchCaseSensitive;
    return flags;
}

Qt::CaseSensitivity FindBar::caseSensitivity() const
{
    return m_caseBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
}

int FindBar::matchCount() const
{
    if (!m_editor)
        return 0;
    return m_editor->matchCount(m_queryEdit->text(), caseSensitivity());
}

void FindBar::findNext()
{
    if (!m_editor)
        return;
    const QString q = m_queryEdit->text();
    if (q.isEmpty()) {
        updateMatchLabel();
        return;
    }
    const int flags = findFlags();
    if (!m_editor->find(q, flags)) {
        // No match after the cursor: wrap to the very top and search again.
        QTextCursor c = m_editor->textCursor();
        c.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(c);
        m_editor->find(q, flags);
    }
    updateMatchLabel();
}

void FindBar::findPrevious()
{
    if (!m_editor)
        return;
    const QString q = m_queryEdit->text();
    if (q.isEmpty()) {
        updateMatchLabel();
        return;
    }
    const int flags = findFlags();
    if (!m_editor->findPrevious(q, flags)) {
        // No match before the cursor: wrap to the very bottom and search again.
        QTextCursor c = m_editor->textCursor();
        c.movePosition(QTextCursor::End);
        m_editor->setTextCursor(c);
        m_editor->findPrevious(q, flags);
    }
    updateMatchLabel();
}

void FindBar::onQueryChanged()
{
    // Recompute the count, and — when a query is present — highlight the match
    // at/after the current cursor (wrap handles the case where the cursor is
    // already past the last match).
    if (!m_queryEdit->text().isEmpty())
        findNext();
    else
        updateMatchLabel();
}

void FindBar::onCaseToggled(bool)
{
    // A sensitivity change can change both the total and the current match.
    if (!m_queryEdit->text().isEmpty())
        findNext();
    else
        updateMatchLabel();
}

void FindBar::closeFindBar()
{
    hide();
    // Return keyboard focus to the editor so typing resumes there.
    if (m_editor)
        m_editor->setFocus();
    emit closed();
}

// -- Replace (Ctrl+H). ---------------------------------------------------------

void FindBar::activateReplace()
{
    activate(); // show + focus the query + jump to the next match (if any)
    setReplaceRowVisible(true);
    // With a query already present the user most likely wants to type the
    // replacement; with none, they want to type the search term first.
    if (m_queryEdit->text().isEmpty())
        m_queryEdit->setFocus();
    else
        m_replaceEdit->setFocus();
}

void FindBar::setReplaceRowVisible(bool on)
{
    if (m_replaceRowVisible == on)
        return;
    m_replaceRowVisible = on;
    m_replaceEdit->setVisible(on);
    m_replaceBtn->setVisible(on);
    m_replaceAllBtn->setVisible(on);
    // The title bar names the mode, so a find+replace window is obvious.
    setWindowTitle(on ? tr("Find and Replace") : tr("Find"));
    updateBarGeometry();
}

void FindBar::updateBarGeometry()
{
    // A popup window: its size follows the number of visible rows (one vs two)
    // through the layout's size hint — resize() to the hint explicitly, because
    // adjustSize() will not shrink a visible top-level window — and it stays
    // centered over the host window as it grows/shrinks.
    resize(sizeHint());
    if (isVisible())
        centerOnParent();
}

int FindBar::replaceCurrent()
{
    if (!m_editor)
        return 0;
    const QString q = m_queryEdit->text();
    if (q.isEmpty())
        return 0;

    // "The current match" = a selection that IS an occurrence of the query
    // (compared with the bar's case-sensitivity). Otherwise find the next one
    // first; if the query has no matches at all there is nothing to replace.
    const QTextCursor cur = m_editor->textCursor();
    const bool onMatch =
        cur.hasSelection() && cur.selectedText().compare(q, caseSensitivity()) == 0;
    if (!onMatch && !m_editor->find(q, findFlags()))
        return 0;

    // Inserting at a cursor with a selection replaces the selection.
    QTextCursor c = m_editor->textCursor();
    c.insertText(m_replaceEdit->text());

    // Advance to the following occurrence (wrapping like Next) so the next
    // Replace hits the next match, and refresh the count label.
    findNext();
    return 1;
}

int FindBar::replaceAll()
{
    if (!m_editor)
        return 0;
    const QString q = m_queryEdit->text();
    if (q.isEmpty())
        return 0;

    // The single replace-all implementation (case-sensitivity from the bar),
    // then refresh the count label: the query now occurs n fewer times (or
    // not at all).
    const int n = m_editor->replaceAll(q, m_replaceEdit->text(), caseSensitivity());
    updateMatchLabel();
    return n;
}

void FindBar::refreshCount()
{
    // Count-only refresh (no selection move) — used by the host window on
    // editor textChanged so the count stays current while editing with the bar
    // open.
    updateMatchLabel();
}

void FindBar::updateMatchLabel()
{
    const int n = matchCount();
    const QString q = m_queryEdit->text();
    if (q.isEmpty()) {
        m_countLabel->setText(QString());
        return;
    }
    if (n == 0) {
        m_countLabel->setText(tr("no matches"));
        return;
    }
    // Is a match currently selected?
    const QTextCursor cur = m_editor ? m_editor->textCursor() : QTextCursor();
    if (m_editor && cur.hasSelection()) {
        // 1-based index of the selected match: count of matches that begin at
        // or before the selection start.
        const QString hay = m_editor->toPlainText();
        const Qt::CaseSensitivity cs = caseSensitivity();
        int idx = 1;
        int pos = 0;
        const int selStart = cur.selectionStart();
        while ((pos = hay.indexOf(q, pos, cs)) != -1) {
            if (pos >= selStart)
                break;
            ++idx;
            pos += q.size();
        }
        m_countLabel->setText(tr("%1 of %2").arg(idx).arg(n));
    } else {
        m_countLabel->setText(tr("%1 matches").arg(n));
    }
}

void FindBar::keyPressEvent(QKeyEvent *event)
{
    // Esc anywhere in the dialog closes it.
    if (event->key() == Qt::Key_Escape) {
        closeFindBar();
        return;
    }
    QDialog::keyPressEvent(event);
}

void FindBar::closeEvent(QCloseEvent *event)
{
    // The title-bar X / a programmatic close: hide + hand focus back to the
    // editor + announce it (the same path as Esc / the Close button).
    event->accept();
    closeFindBar();
}
