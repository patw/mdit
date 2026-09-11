// test_export_html — subtask 9.1: Export → HTML in MainWindow.
//
// Verifies headlessly (offscreen QApplication, isolated temp-file QSettings):
//   - RenderedDocument::standaloneHtml() assembles a real standalone page
//     (<!DOCTYPE html>, <html>, UTF-8 charset, the document title escaped,
//     the theme stylesheet in <style>, the lifted <body> content — with a
//     fallback for fragments that carry no <body> tag);
//   - exportHtmlDocument() renders through the SHARED RenderedDocument path
//     (same renderer as the preview) and writes non-empty standalone HTML to
//     the chosen .html file (doctype + <html> + the rendered content + the
//     theme stylesheet), reporting "Exported <name>" in the status bar;
//   - the suggested dialog name is the document's stem with .html
//     ("notes.md" -> "notes.html", untitled -> "untitled.html");
//   - the export uses the CURRENT editor content (unsaved edits included)
//     and does not change the document's dirty state;
//   - a canceled dialog writes nothing; an un-writable path fails with
//     "Could not export <name>" and leaves state untouched;
//   - the dark theme exports the dark stylesheet;
//   - relative image references are COPIED next to the saved HTML (same
//     relative path, subdirectories recreated) so the exported file keeps
//     working wherever it is saved (spec §10).
//
// The Export HTML dialog is scripted by overriding the virtual
// askExportHtmlPath() seam (same pattern as askSaveAsPath in test_save), so
// no real modal dialog ever blocks the headless run.

#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"
#include "Settings.h"
#include "RenderedDocument.h"

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>

// A MainWindow whose Export HTML dialog answer is fully scripted (no real
// dialog).
class ScriptedExportWindow : public MainWindow
{
public:
    ScriptedExportWindow(QWidget *parent = nullptr, QSettings *backing = nullptr)
        : MainWindow(parent, backing) {}

    QString scriptedExportPath; // empty == cancel
    QString lastSuggestedName;
    int exportAsks = 0;

    QString askExportHtmlPath(const QString &suggestedName) override
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

static QString fileContents(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

// A small real PNG (Qt can resolve + copy it; 1x1 is plenty).
static bool writePng(const QString &path)
{
    QPixmap pm(4, 4);
    pm.fill(Qt::red);
    return pm.save(path);
}

class TestExportHtml : public QObject
{
    Q_OBJECT

private slots:
    void standaloneHtmlWrapsDoctypeTitleAndTheme();
    void standaloneHtmlFallsBackWithoutBodyTag();
    void exportWritesNonEmptyStandaloneHtml();
    void suggestedHtmlNameComesFromDocumentStem();
    void exportUsesCurrentEditorContentAndKeepsDirty();
    void exportCancelWritesNothing();
    void exportBadPathFailsWithoutStateChange();
    void darkThemeExportsDarkStylesheet();
    void exportCopiesRelativeImagesNextToHtml();
};

void TestExportHtml::standaloneHtmlWrapsDoctypeTitleAndTheme()
{
    const QString fragment = QStringLiteral(
        "<html><head><meta name=\"qrichtext\" content=\"1\" /></head>"
        "<body link=\"#00c\" vlink=\"#e60000\">"
        "<h1>Hello</h1>"
        "</body></html>");

    const QString light = RenderedDocument::standaloneHtml("notes", fragment, false);
    QVERIFY(light.contains(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(light.contains(QStringLiteral("<html")));
    QVERIFY(light.contains(QStringLiteral("charset=\"utf-8\"")));
    QVERIFY(light.contains(QStringLiteral("<title>notes</title>")));
    // The light stylesheet is embedded.
    QVERIFY(light.contains(QStringLiteral("#ffffff")));
    // The lifted body content survives the wrap.
    QVERIFY(light.contains(QStringLiteral("<h1>Hello</h1>")));
    // The page ends as a closed standalone document.
    QVERIFY(light.endsWith(QStringLiteral("</html>\n")));

    // Title is HTML-escaped.
    const QString titled =
        RenderedDocument::standaloneHtml(QStringLiteral("a<b & \"c\""), fragment, false);
    QVERIFY(titled.contains(
        QStringLiteral("<title>a&lt;b &amp; &quot;c&quot;</title>")));

    // The dark variant carries the dark stylesheet instead.
    const QString dark = RenderedDocument::standaloneHtml("notes", fragment, true);
    QVERIFY(dark.contains(QStringLiteral("#1e1e1e")));
    QVERIFY(!dark.contains(QStringLiteral("background-color: #ffffff;")));
}

void TestExportHtml::standaloneHtmlFallsBackWithoutBodyTag()
{
    // A fragment with no <body> tag is used as the body as-is — still a
    // valid standalone page, never an empty one.
    const QString page = RenderedDocument::standaloneHtml(
        "t", QStringLiteral("<html><p>plain</p></html>"), false);
    QVERIFY(page.contains(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(page.contains(QStringLiteral("<p>plain</p>")));
}

void TestExportHtml::exportWritesNonEmptyStandaloneHtml()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath = writeTemp(
        dir.path(), "notes.md",
        "# Hello world\n\n**bold** and *em* and `code`\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedExportWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));

    const QString out = dir.filePath(QStringLiteral("out.html"));
    w.scriptedExportPath = out;
    QVERIFY(w.exportHtmlDocument());
    QCOMPARE(w.exportAsks, 1);

    // The file is non-empty standalone HTML (the subtask's core assertion).
    QVERIFY(QFile::exists(out));
    const QString html = fileContents(out);
    QVERIFY(!html.isEmpty());
    QVERIFY(html.contains(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(QStringLiteral("<html")));
    // The rendered content is in there (this build inlines the styles).
    QVERIFY(html.contains(QStringLiteral("Hello world")));
    QVERIFY(html.contains(QStringLiteral("bold")));
    QVERIFY(html.contains(QStringLiteral("code")));
    // The theme stylesheet (light) ships with the page.
    QVERIFY(html.contains(QStringLiteral("background-color: #ffffff;")));
    QVERIFY(html.contains(QStringLiteral("<style>")));
    // The status bar reports the export.
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Exported out.html")));
    // The export does not dirty the document.
    QVERIFY(!w.dirty());
}

void TestExportHtml::suggestedHtmlNameComesFromDocumentStem()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath = writeTemp(dir.path(), "notes.md", "x\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedExportWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));
    w.scriptedExportPath = QString(); // cancel — we only care about the prefill
    w.onExportHtml(); // the menu slot drives the same path
    QCOMPARE(w.lastSuggestedName, QString(QStringLiteral("notes.html")));

    // Untitled: the untitled document name with .html.
    ScriptedExportWindow w2(nullptr, &ini);
    w2.scriptedExportPath = QString();
    w2.onExportHtml();
    QCOMPARE(w2.lastSuggestedName, QString(QStringLiteral("untitled.html")));
}

void TestExportHtml::exportUsesCurrentEditorContentAndKeepsDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath =
        writeTemp(dir.path(), "doc.md", "alpha is the old text\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedExportWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));

    // An UNSAVED edit: the export must carry the editor's current content,
    // not the stale on-disk one.
    w.editorPane()->setPlainText(QStringLiteral("beta is the new text\n"));
    QVERIFY(w.dirty());

    const QString out = dir.filePath(QStringLiteral("doc.html"));
    w.scriptedExportPath = out;
    QVERIFY(w.exportHtmlDocument());

    const QString html = fileContents(out);
    QVERIFY(html.contains(QStringLiteral("beta is the new text")));
    QVERIFY(!html.contains(QStringLiteral("alpha is the old text")));
    // Export is read-only over the document: still dirty, path unchanged.
    QVERIFY(w.dirty());
    QCOMPARE(w.document()->currentFilePath(), mdPath);
    QCOMPARE(fileContents(mdPath), QString(QStringLiteral("alpha is the old text\n")));
}

void TestExportHtml::exportCancelWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("cancelled.html"));

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedExportWindow w(nullptr, &ini);
    w.editorPane()->setPlainText(QStringLiteral("some text\n"));

    w.scriptedExportPath = QString(); // the user cancels the dialog
    QVERIFY(!w.exportHtmlDocument());
    QCOMPARE(w.exportAsks, 1);
    QVERIFY(!QFile::exists(out));
}

void TestExportHtml::exportBadPathFailsWithoutStateChange()
{
    QTemporaryDir dir; // keep alive for the ini file
    QVERIFY(dir.isValid());
    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);

    ScriptedExportWindow w(nullptr, &ini);
    const QString mdPath =
        writeTemp(dir.path(), "doc.md", "alpha\n");
    QVERIFY(w.openFile(mdPath));
    const QString original = fileContents(mdPath);

    // No such directory: the write must fail, not silently succeed.
    w.scriptedExportPath = QStringLiteral("/no/such/dir/definitely/out.html");
    QVERIFY(!w.exportHtmlDocument());

    QCOMPARE(w.document()->currentFilePath(), mdPath);
    QVERIFY(!w.dirty()); // the export never dirtied the document
    QCOMPARE(fileContents(mdPath), original);
    QCOMPARE(w.statusBar()->currentMessage(),
             QString(QStringLiteral("Could not export out.html")));
}

void TestExportHtml::darkThemeExportsDarkStylesheet()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mdPath = writeTemp(dir.path(), "dark.md", "hi\n");

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedExportWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));
    // The export must follow the current theme (the theming system's mode).
    w.setThemeMode(Settings::ThemeMode::Dark);
    QVERIFY(w.darkTheme());

    const QString out = dir.filePath(QStringLiteral("dark.html"));
    w.scriptedExportPath = out;
    QVERIFY(w.exportHtmlDocument());

    const QString html = fileContents(out);
    QVERIFY(html.contains(QStringLiteral("#1e1e1e"))); // dark stylesheet
    QVERIFY(!html.contains(QStringLiteral("background-color: #ffffff;")));
}

void TestExportHtml::exportCopiesRelativeImagesNextToHtml()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Two images: one at the top level, one in a subdirectory.
    const QString img1 = dir.filePath(QStringLiteral("img.png"));
    const QString img2 = dir.filePath(QStringLiteral("screenshots/x.png"));
    QVERIFY(QDir(dir.path() + QStringLiteral("/screenshots")).mkpath(QStringLiteral(".")));
    QVERIFY(writePng(img1));
    QVERIFY(writePng(img2));
    const QString mdPath = writeTemp(
        dir.path(), "withimg.md",
        "![logo](img.png)\n\n![shot](screenshots/x.png)\n");

    // Export to a DIFFERENT directory: the images must be copied there with
    // the same relative paths (spec §10 — "HTML references them by path
    // relative to the saved HTML").
    const QString outDir = dir.path() + QStringLiteral("/exported");
    QVERIFY(QDir().mkpath(outDir));

    QSettings ini(dir.path() + QStringLiteral("/export.ini"),
                  QSettings::IniFormat);
    ScriptedExportWindow w(nullptr, &ini);
    QVERIFY(w.openFile(mdPath));

    const QString out = outDir + QStringLiteral("/withimg.html");
    w.scriptedExportPath = out;
    QVERIFY(w.exportHtmlDocument());

    const QString html = fileContents(out);
    QVERIFY(html.contains(QStringLiteral("<img")));
    QVERIFY(html.contains(QStringLiteral("img.png")));
    QVERIFY(html.contains(QStringLiteral("screenshots/x.png")));
    // The relative images were copied next to the saved HTML.
    const QString copied1 = outDir + QStringLiteral("/img.png");
    const QString copied2 = outDir + QStringLiteral("/screenshots/x.png");
    QVERIFY(QFile::exists(copied1));
    QVERIFY(QFile::exists(copied2));
    // (No static QFile::size(path) on this Qt build — QFileInfo instead.)
    QCOMPARE(QFileInfo(copied1).size(), QFileInfo(img1).size());
    QCOMPARE(QFileInfo(copied2).size(), QFileInfo(img2).size());
    // Byte-identical copies.
    QCOMPARE(fileContents(copied1), fileContents(img1));
    QCOMPARE(fileContents(copied2), fileContents(img2));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestExportHtml t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_export_html.moc"
