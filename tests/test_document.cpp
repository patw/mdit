// test_document — exercises the pure `Document` model (no widget, no window).
//
// Covers: load/save UTF-8 round-trip, CRLF -> LF normalization (and CRLF
// round-trip back), dirty-flag transitions, title derivation (filename vs
// untitled), save-as path update + dirty clearing, and nextUntitled() naming.
#include <QtTest>

#include "Document.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

class TestDocument : public QObject
{
    Q_OBJECT
private slots:
    void defaults_untitledAndClean();
    void setTitle_marksDirty_whenChanged();
    void loadRoundTrip_preservesUtf8();
    void load_normalizesCRLFtoLF();
    void save_restoreCRLF_lineEnding();
    void save_clearsDirty();
    void saveAs_updatesPath_andClearsDirty();
    void load_missingPath_fails();
    void save_noPath_untitledFails();
    void title_derivedFromFilename();
    void nextUntitled_sequence();
};

void TestDocument::defaults_untitledAndClean()
{
    Document d;
    QVERIFY(!d.dirty());
    QVERIFY(d.currentFilePath().isEmpty());
    QVERIFY(d.text().isEmpty());
    QCOMPARE(d.title(), QString(QStringLiteral("untitled")));
}

void TestDocument::setTitle_marksDirty_whenChanged()
{
    Document d;
    d.setText(QStringLiteral("hello"));
    QVERIFY(d.dirty());
    QCOMPARE(d.text(), QString(QStringLiteral("hello")));

    // Setting the same text again must NOT flip dirty back on.
    d.setDirty(false);
    d.setText(QStringLiteral("hello"));
    QVERIFY(!d.dirty());
}

void TestDocument::loadRoundTrip_preservesUtf8()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("round.md"));

    // A file with a mix of ASCII and non-ASCII (Café / 中文) content to prove
    // the UTF-8 path round-trips without mojibake.
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        // Qt6 QTextStream defaults to UTF-8; no explicit codec needed.
        QTextStream ts(&f);
        ts << QStringLiteral("# Title\n\nCafé 中文\n");
        ts.flush();
    }

    Document d;
    QVERIFY(d.load(path));
    QVERIFY(!d.dirty());
    QCOMPARE(d.currentFilePath(), path);
    QCOMPARE(d.text(), QString(QStringLiteral("# Title\n\nCafé 中文\n")));
    QCOMPARE(d.title(), QString(QStringLiteral("round.md")));

    // Round-trip to a second file and reload: content identical.
    const QString path2 = tmp.filePath(QStringLiteral("round2.md"));
    QVERIFY(d.save(path2));
    Document d2;
    QVERIFY(d2.load(path2));
    QCOMPARE(d2.text(), d.text());
}

void TestDocument::load_normalizesCRLFtoLF()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("crlf.md"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("line one\r\nline two\r\nline three\n");
    }

    Document d;
    QVERIFY(d.load(path));
    // All line endings normalized to LF; no stray \r remains.
    QCOMPARE(d.text(), QString(QStringLiteral("line one\nline two\nline three\n")));
    QVERIFY(!d.text().contains(QLatin1Char('\r')));
}

void TestDocument::save_restoreCRLF_lineEnding()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString inPath = tmp.filePath(QStringLiteral("in.md"));
    {
        QFile f(inPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("a\r\nb\r\nc\r\n");
    }

    Document d;
    QVERIFY(d.load(inPath));
    QCOMPARE(d.text(), QString(QStringLiteral("a\nb\nc\n")));

    // Saving the (still-LF) document back must restore the original CRLF.
    const QString outPath = tmp.filePath(QStringLiteral("out.md"));
    QVERIFY(d.save(outPath));
    QFile rf(outPath);
    QVERIFY(rf.open(QIODevice::ReadOnly));
    const QByteArray bytes = rf.readAll();
    QCOMPARE(QString::fromUtf8(bytes), QString(QStringLiteral("a\r\nb\r\nc\r\n")));
}

void TestDocument::save_clearsDirty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("dirty.md"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("original\n");
    }

    Document d;
    QVERIFY(d.load(path));
    QVERIFY(!d.dirty());
    d.setText(QStringLiteral("edited\n"));
    QVERIFY(d.dirty());

    // Save to the current path (no argument) — clears dirty.
    QVERIFY(d.save());
    QVERIFY(!d.dirty());
    QCOMPARE(d.currentFilePath(), path);

    // Reload to confirm the edit actually hit disk.
    Document d2;
    QVERIFY(d2.load(path));
    QCOMPARE(d2.text(), QString(QStringLiteral("edited\n")));
}

void TestDocument::saveAs_updatesPath_andClearsDirty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("as.md"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("start\n");
    }

    Document d;
    QVERIFY(d.load(path));
    d.setText(QStringLiteral("changed\n"));
    QVERIFY(d.dirty());

    // Save As a different path: path updates, dirty clears, file exists there.
    const QString newPath = tmp.filePath(QStringLiteral("as2.md"));
    QVERIFY(d.save(newPath));
    QVERIFY(!d.dirty());
    QCOMPARE(d.currentFilePath(), newPath);
    QVERIFY(QFile::exists(newPath));

    Document d2;
    QVERIFY(d2.load(newPath));
    QCOMPARE(d2.text(), QString(QStringLiteral("changed\n")));
}

void TestDocument::load_missingPath_fails()
{
    Document d;
    d.setText(QStringLiteral("keep me\n"));
    QVERIFY(d.dirty());
    QVERIFY(!d.load(QStringLiteral("/nonexistent/definitely/missing.md")));
    // State untouched on failure.
    QCOMPARE(d.text(), QString(QStringLiteral("keep me\n")));
    QVERIFY(d.dirty());
    QVERIFY(d.currentFilePath().isEmpty());
}

void TestDocument::save_noPath_untitledFails()
{
    Document d;
    d.setText(QStringLiteral("no path\n"));
    // No current path and no argument -> save fails.
    QVERIFY(!d.save());
    QVERIFY(d.dirty()); // still dirty, nothing was written
}

void TestDocument::title_derivedFromFilename()
{
    Document d;
    // Untitled.
    QCOMPARE(d.title(), QString(QStringLiteral("untitled")));
    // After an explicit path is set, title() is the base name.
    d.setCurrentFilePath(QStringLiteral("/home/user/docs/notes.markdown"));
    QCOMPARE(d.title(), QString(QStringLiteral("notes.markdown")));
    // setUntitledName overrides the unsaved title.
    d.setCurrentFilePath(QString());
    d.setUntitledName(QStringLiteral("untitled 3"));
    QCOMPARE(d.title(), QString(QStringLiteral("untitled 3")));
}

void TestDocument::nextUntitled_sequence()
{
    Document d;
    QCOMPARE(d.nextUntitled(), QString(QStringLiteral("untitled")));
    QCOMPARE(d.nextUntitled(), QString(QStringLiteral("untitled 1")));
    QCOMPARE(d.nextUntitled(), QString(QStringLiteral("untitled 2")));
}

QTEST_GUILESS_MAIN(TestDocument)
#include "test_document.moc"
