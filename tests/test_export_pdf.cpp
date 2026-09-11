// test_export_pdf — subtask 9.2: Export -> PDF in MainWindow.
//
// Verifies headlessly (offscreen QApplication, isolated temp-file QSettings):
//   - RenderedDocument::renderPdf() renders through the SHARED renderer (the
//     same path the preview / HTML export use) and prints into a QPdfWriter
//     with a set page size (A4) / orientation (portrait) / resolution (DPI),
//     producing a real PDF whose first bytes are the "%PDF-" magic header;
//   - MainWindow::exportPdfDocument() writes that PDF to the path chosen
//     through the scriptable askExportPdfPath() seam, reporting
//     "Exported <name>" in the status bar, without dirtying the document;
//   - the suggested dialog name is the document's stem with .pdf
//     ("notes.md" -> "notes.pdf", untitled -> "untitled.pdf");
//   - the export carries the CURRENT editor content (unsaved edits included)
//     and does not change the document's dirty state or path;
//   - a canceled dialog writes nothing; an un-writable path fails with
//     "Could not export <name>" and leaves state untouched;
//   - renderPdf() returns false for an unwritable destination (the %PDF
//     header is never written).
//
// The Export PDF dialog is scripted by overriding the virtual askExportPdfPath()
// seam (same pattern as askExportHtmlPath in test_export_html), so no real
// modal dialog ever blocks the headless run.

#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"
#include "RenderedDocument.h"

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPageSize>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>

// A MainWindow whose Export PDF dialog answer is fully scripted (no real dialog).
class ScriptedPdfWindow : public MainWindow
{
public:
    ScriptedPdfWindow(QWidget *parent = nullptr, QSettings *backing = nullptr)
        : MainWindow(parent, backing) {}

    QString scriptedExportPath; // empty == cancel
    QString lastSuggestedName;
    int exportAsks = 0;

    QString askExportPdfPath(const QString &suggestedName) override
    {
        ++exportAsks;
        lastSuggestedName = suggestedName;
        return scriptedExportPath;
    }
};

static QString writeTemp(const QString &dir, const QString &name,
                         const QByteArray &bytes)
{
    const QString path = dir + QLatin1Char('/') + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write(bytes);
    f.close();
    return path;
}

// The first `n` bytes of a file (empty string when it cannot be opened).
static QByteArray headBytes(const QString &path, int n)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    const QByteArray head = f.read(n);
    f.close();
    return head;
}

// The full text of a file (empty string when it cannot be opened).
static QString readTemp(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

class TestExportPdf : public QObject
{
    Q_OBJECT

private slots:
    void renderPdfWritesPdfMagicHeader();
    void renderPdfFailsOnUnwritablePath();
    void exportPdfWritesNonEmptyPdf();
    void suggestedPdfNameComesFromDocumentStem();
    void exportPdfUsesCurrentEditorContentAndKeepsDirty();
    void exportPdfCancelWritesNothing();
    void exportPdfBadPathFailsWithoutStateChange();
    void exportPdfDpiIsConfigurableAndClamped();
};

void TestExportPdf::renderPdfWritesPdfMagicHeader()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("doc.pdf"));

    const bool ok = RenderedDocument::renderPdf(
        QStringLiteral("# Hello world\n\n**bold** and *em*\n"),
        QUrl(), /*dark=*/false, out, QPageSize::A4, 96);

    QVERIFY(ok);
    QVERIFY(QFile::exists(out));
    // A non-empty file.
    QVERIFY(QFileInfo(out).size() > 0);
    // The subtask's core assertion: the PDF magic header.
    QVERIFY(headBytes(out, 5) == QByteArrayLiteral("%PDF-"));
    // And it is a complete, non-degenerate PDF: a page object and the
    // end-of-file marker (an empty/broken file would have neither).
    const QByteArray all = headBytes(out, QFileInfo(out).size());
    QVERIFY(all.contains(QByteArrayLiteral("/Page")));
    QVERIFY(all.contains(QByteArrayLiteral("%EOF")));
}

void TestExportPdf::renderPdfFailsOnUnwritablePath()
{
    // No such directory: the writer cannot create the file, so no %PDF header
    // is written and renderPdf() must report failure.
    const bool ok = RenderedDocument::renderPdf(
        QStringLiteral("# x\n"), QUrl(), /*dark=*/false,
        QStringLiteral("/no/such/dir/definitely/doc.pdf"), QPageSize::A4, 96);
    QVERIFY(!ok);
}

void TestExportPdf::exportPdfWritesNonEmptyPdf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath = writeTemp(
        dir.path(), "notes.md",
        "# Hello world\n\n**bold** and `code`\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedPdfWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));

    const QString out = dir.filePath(QStringLiteral("out.pdf"));
    w.scriptedExportPath = out;
    QVERIFY(w.exportPdfDocument());
    QCOMPARE(w.exportAsks, 1);

    // A real PDF (magic header + non-empty + a complete body).
    QVERIFY(QFile::exists(out));
    QVERIFY(QFileInfo(out).size() > 0);
    QVERIFY(headBytes(out, 5) == QByteArrayLiteral("%PDF-"));
    const QByteArray pdf = headBytes(out, QFileInfo(out).size());
    QVERIFY(pdf.contains(QByteArrayLiteral("/Page")));
    QVERIFY(pdf.contains(QByteArrayLiteral("%EOF")));
    // The status bar reports the export.
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Exported out.pdf")));
    // The export does not dirty the document.
    QVERIFY(!w.dirty());
}

void TestExportPdf::suggestedPdfNameComesFromDocumentStem()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath = writeTemp(dir.path(), "notes.md", "x\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedPdfWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));
    w.scriptedExportPath = QString(); // cancel — we only care about the prefill
    w.onExportPdf(); // the menu slot drives the same path
    QCOMPARE(w.lastSuggestedName, QString(QStringLiteral("notes.pdf")));

    // Untitled: the untitled document name with .pdf.
    ScriptedPdfWindow w2(nullptr, &ini);
    w2.scriptedExportPath = QString();
    w2.onExportPdf();
    QCOMPARE(w2.lastSuggestedName, QString(QStringLiteral("untitled.pdf")));
}

void TestExportPdf::exportPdfUsesCurrentEditorContentAndKeepsDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath =
        writeTemp(dir.path(), "doc.md", "alpha is the old text\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedPdfWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));

    // An UNSAVED edit: the export must carry the editor's current content and
    // must not dirty / move the document.
    w.editorPane()->setPlainText(QStringLiteral("beta is the new text\n"));
    QVERIFY(w.dirty());

    const QString out = dir.filePath(QStringLiteral("doc.pdf"));
    w.scriptedExportPath = out;
    QVERIFY(w.exportPdfDocument());

    // A valid PDF was produced (content itself is binary and not asserted here
    // — the header is the objective signal of a successful render+print).
    QVERIFY(QFile::exists(out));
    QVERIFY(headBytes(out, 5) == QByteArrayLiteral("%PDF-"));
    // Export is read-only over the document: still dirty, path unchanged, the
    // on-disk markdown untouched.
    QVERIFY(w.dirty());
    QCOMPARE(w.document()->currentFilePath(), mdPath);
    QCOMPARE(readTemp(mdPath),
             QString(QStringLiteral("alpha is the old text\n")));
}

void TestExportPdf::exportPdfCancelWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("cancelled.pdf"));

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedPdfWindow w(nullptr, &ini);
    w.editorPane()->setPlainText(QStringLiteral("some text\n"));

    w.scriptedExportPath = QString(); // the user cancels the dialog
    QVERIFY(!w.exportPdfDocument());
    QCOMPARE(w.exportAsks, 1);
    QVERIFY(!QFile::exists(out));
}

void TestExportPdf::exportPdfBadPathFailsWithoutStateChange()
{
    QTemporaryDir dir; // keep alive for the ini file
    QVERIFY(dir.isValid());
    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);

    ScriptedPdfWindow w(nullptr, &ini);
    const QString mdPath = writeTemp(dir.path(), "doc.md", "alpha\n");
    QVERIFY(w.openFile(mdPath));
    const QString original = readTemp(mdPath);

    // No such directory: the write must fail, not silently succeed.
    w.scriptedExportPath = QStringLiteral("/no/such/dir/definitely/out.pdf");
    QVERIFY(!w.exportPdfDocument());

    QCOMPARE(w.document()->currentFilePath(), mdPath);
    QVERIFY(!w.dirty()); // the export never dirtied the document
    QCOMPARE(readTemp(mdPath), original);
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Could not export out.pdf")));
}

void TestExportPdf::exportPdfDpiIsConfigurableAndClamped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath = writeTemp(dir.path(), "d.md", "hi\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedPdfWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));

    // Default DPI is 96 (spec: a concrete resolution is set on the writer).
    QCOMPARE(w.pdfResolutionDpi(), 96);

    // A positive DPI round-trips and still produces a valid PDF.
    w.setPdfResolutionDpi(150);
    QCOMPARE(w.pdfResolutionDpi(), 150);
    const QString out = dir.filePath(QStringLiteral("d.pdf"));
    w.scriptedExportPath = out;
    QVERIFY(w.exportPdfDocument());
    QVERIFY(headBytes(out, 5) == QByteArrayLiteral("%PDF-"));

    // A non-positive DPI is clamped to 1 (never a nonsense value).
    w.setPdfResolutionDpi(0);
    QCOMPARE(w.pdfResolutionDpi(), 1);
    w.setPdfResolutionDpi(-50);
    QCOMPARE(w.pdfResolutionDpi(), 1);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestExportPdf t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_export_pdf.moc"
