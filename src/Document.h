// Document — mdit's non-widget document model.
//
// Owns the plain-text content, the current file path, the dirty flag, and the
// line-ending convention. It has NO GUI dependency (no QWidget, no
// QTextDocument) so it can be exercised headlessly in `test_document`. It does
// NOT own the editor widget — the MainWindow / EditorPane hold the text in the
// widget and push it into this model; here we only model the file/dirty/title
// concerns.
#pragma once

#include <QString>

class Document
{
public:
    Document() = default;

    // -- content ----------------------------------------------------------
    QString text() const { return m_text; }
    // Replace the whole content. Marks the document dirty if it actually
    // changed.
    void setText(const QString &text);

    // -- file path / dirty ------------------------------------------------
    QString currentFilePath() const { return m_path; }
    void setCurrentFilePath(const QString &path);
    bool dirty() const { return m_dirty; }
    void setDirty(bool dirty) { m_dirty = dirty; }

    // -- I/O --------------------------------------------------------------
    // Streaming UTF-8 read of `path`. Normalizes CRLF (and lone CR) to LF in
    // memory for consistent behavior, remembering the original line ending for
    // a round-trip save. On success sets currentFilePath and clears dirty.
    // Returns false (leaving prior state untouched) if the file cannot be
    // opened.
    bool load(const QString &path);

    // UTF-8 write (no BOM). With no argument saves to currentFilePath()
    // (returns false if that is empty). The remembered line ending is applied
    // on write (LF by default). On success clears dirty and, when an explicit
    // path is given, updates currentFilePath.
    bool save(const QString &path = QString());

    // -- title ------------------------------------------------------------
    // The base file name when saved, otherwise the current untitled name.
    // (No `*` here — the dirty marker is a title-bar concern for subtask 5.)
    QString title() const;

    // The name the next new untitled document should take: "untitled", then
    // "untitled 1", "untitled 2", ... Advances the untitled counter and returns
    // the name.
    QString nextUntitled();
    // Set the untitled name shown by title() while the document is unsaved
    // (e.g. to the result of nextUntitled() after a New).
    void setUntitledName(const QString &name);

private:
    QString m_text;
    QString m_path;
    QString m_untitledName = QStringLiteral("untitled");
    QString m_lineEnding = QStringLiteral("\n");
    bool m_dirty = false;
    int m_untitledCounter = 0;
};
