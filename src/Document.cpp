// Document — implementation. See Document.h for the design rationale.
#include "Document.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

void Document::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    m_dirty = true;
}

void Document::setCurrentFilePath(const QString &path)
{
    m_path = path;
}

bool Document::load(const QString &path)
{
    // Open without QIODevice::Text so NO platform line-ending translation
    // happens while reading: we do the CRLF->LF normalization ourselves (and
    // remember the source convention for the round-trip save).
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    // Streaming read (QFile reads in chunks; readAll assembles a QString). In
    // Qt6 QTextStream defaults to UTF-8, so no codec setup is needed and BOM'd /
    // non-ASCII content round-trips correctly.
    QTextStream in(&file);
    const QString raw = in.readAll();

    // Detect the dominant line ending from the raw text BEFORE normalizing, so
    // a save can restore the file's original convention (\r\n) or default to \n.
    m_lineEnding = QStringLiteral("\n");
    if (raw.contains(QStringLiteral("\r\n")))
        m_lineEnding = QStringLiteral("\r\n");

    // Normalize every CRLF and lone CR to a single LF for in-memory consistency.
    m_text = raw;
    m_text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    m_text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    m_path = path;
    m_dirty = false;
    return true;
}

bool Document::save(const QString &path)
{
    const QString target = path.isEmpty() ? m_path : path;
    if (target.isEmpty())
        return false;

    // No QIODevice::Text on write either: the bytes we emit (LF, or CRLF when
    // that is the remembered convention) are exactly what lands on disk.
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // Write UTF-8 without a BOM (QTextStream in Qt6 defaults to UTF-8 and never
    // emits a BOM). Restore the remembered line ending on write.
    QString out = m_text;
    if (m_lineEnding == QStringLiteral("\r\n"))
        out.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));

    QTextStream outStream(&file);
    outStream << out;
    outStream.flush();
    const bool ok = !file.error() && file.flush();

    if (ok) {
        m_dirty = false;
        if (!path.isEmpty())
            m_path = path;
    }
    return ok;
}

QString Document::title() const
{
    if (!m_path.isEmpty()) {
        QFileInfo info(m_path);
        if (!info.fileName().isEmpty())
            return info.fileName();
    }
    return m_untitledName;
}

QString Document::nextUntitled()
{
    const QString name = (m_untitledCounter == 0)
        ? QStringLiteral("untitled")
        : QStringLiteral("untitled %1").arg(m_untitledCounter);
    ++m_untitledCounter;
    return name;
}

void Document::setUntitledName(const QString &name)
{
    m_untitledName = name;
}
