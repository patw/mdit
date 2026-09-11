// test_about — the Help > About box.
//
// mdit's About is a real little dialog (not a status-bar message): the brand
// mark (the Pengy-style white disc + '#'), the name, the version, the one-line
// pitch and the Catbee project link. This suite pins the contents and the
// behavior:
//   * the mark is shown (a non-null 96 px pixmap of the app icon) and the window
//     carries that same icon;
//   * the title/summary/copyright strings come from AppInfo (single source of
//     truth) — "mdit", "version 0.1.0", "The ultimate lightweight Markdown
//     tool", "\u00A9 2026 Pat Wendorf \u2014 MIT License";
//   * the project line is a real rich-text link to https://catbee.ca with
//     openExternalLinks() enabled;
//   * the Close button hides it;
//   * MainWindow::onAbout() shows ONE reusable dialog, parented to the window,
//     window-modal and centered on it, and re-shows the same instance.
#include "testmain.h"
#include "AboutDialog.h"
#include "AppIcons.h"
#include "AppInfo.h"
#include "MainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

class TestAbout : public QObject
{
    Q_OBJECT
private slots:
    void showsTheBrandMarkAndMetadata();
    void projectLineIsARealLink();
    void closeButtonHidesTheDialog();
    void mainWindowShowsOneReusableCenteredDialog();
    void versionComesFromTheBuild();
};

// The mark + the identity strings (all sourced from AppInfo).
void TestAbout::showsTheBrandMarkAndMetadata()
{
    AboutDialog dlg;
    QVERIFY(dlg.nameLabel() != nullptr);
    QCOMPARE(dlg.nameLabel()->text(), QStringLiteral("mdit"));

    QVERIFY(dlg.versionLabel() != nullptr);
    QVERIFY2(dlg.versionLabel()->text().contains(AppInfo::version()),
             qPrintable(dlg.versionLabel()->text()));

    QVERIFY(dlg.taglineLabel() != nullptr);
    QCOMPARE(dlg.taglineLabel()->text(), AppInfo::tagline());
    QVERIFY(dlg.taglineLabel()->text().contains(QStringLiteral("lightweight Markdown")));

    QVERIFY(dlg.licenseLabel() != nullptr);
    QCOMPARE(dlg.licenseLabel()->text(), AppInfo::copyrightLine());
    QVERIFY(dlg.licenseLabel()->text().contains(QStringLiteral("Pat Wendorf")));
    QVERIFY(dlg.licenseLabel()->text().contains(QStringLiteral("2026")));
    QVERIFY(dlg.licenseLabel()->text().contains(QStringLiteral("MIT")));

    // The brand mark is actually painted into the dialog, and the window icon is
    // the app icon too.
    QVERIFY(dlg.iconLabel() != nullptr);
    const QPixmap mark = dlg.iconLabel()->pixmap();
    QVERIFY2(!mark.isNull(), "the About box must show the mdit mark");
    QVERIFY(mark.width() >= 48);
    QVERIFY(!dlg.windowIcon().isNull());
    QVERIFY(!AppIcons::windowIcon().isNull());
}

// "mdit is a Catbee project — catbee.ca", with the URL as a working link.
void TestAbout::projectLineIsARealLink()
{
    AboutDialog dlg;
    QVERIFY(dlg.projectLabel() != nullptr);
    const QString text = dlg.projectLabel()->text();
    QVERIFY2(text.contains(QStringLiteral("Catbee project")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("https://catbee.ca")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral(">catbee.ca<")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("<a href=")));
    QVERIFY2(dlg.projectLabel()->openExternalLinks(),
             "the Catbee link must open in the system browser");
    QCOMPARE(dlg.projectLabel()->textFormat(), Qt::RichText);
    QCOMPARE(AppInfo::projectUrl(), QStringLiteral("https://catbee.ca"));
}

// Close (and Esc) hide the reusable box.
void TestAbout::closeButtonHidesTheDialog()
{
    AboutDialog dlg;
    QVERIFY(!dlg.isVisible());

    dlg.showAbout();
    QVERIFY(dlg.isVisible());

    QPushButton *close = dlg.closeButton();
    QVERIFY2(close != nullptr, "the About box needs a Close button");
    close->click();
    QVERIFY(!dlg.isVisible());
}

// MainWindow::onAbout() shows one dialog, parented to the window, window-modal,
// centered on it, and reuses the same instance next time.
void TestAbout::mainWindowShowsOneReusableCenteredDialog()
{
    MainWindow w;
    w.resize(1000, 700);
    w.show();

    QVERIFY(w.aboutDialog() == nullptr); // lazy: nothing built until asked
    w.onAbout();

    AboutDialog *about = w.aboutDialog();
    QVERIFY(about != nullptr);
    QVERIFY2(about->isVisible(), "Help > About must show the box");
    QVERIFY(about->isWindow());
    QCOMPARE(about->parentWidget(), static_cast<QWidget *>(&w));
    QCOMPARE(about->windowModality(), Qt::WindowModal);

    // Centered over the main window (like the find popup).
    const QPoint delta =
        about->frameGeometry().center() - w.frameGeometry().center();
    QVERIFY2(qAbs(delta.x()) <= 2 && qAbs(delta.y()) <= 2,
             qPrintable(QStringLiteral("About not centered: (%1, %2)")
                            .arg(delta.x())
                            .arg(delta.y())));

    about->closeButton()->click();
    QVERIFY(!about->isVisible());

    w.onAbout(); // again: the SAME dialog comes back
    QCOMPARE(w.aboutDialog(), about);
    QVERIFY(about->isVisible());
    about->closeButton()->click();
}

// The version string is the single one from the build (CMake project VERSION),
// not a second hardcoded literal.
void TestAbout::versionComesFromTheBuild()
{
    QCOMPARE(AppInfo::version(), QStringLiteral(MDIT_VERSION));
    QVERIFY(!AppInfo::name().isEmpty());
    QVERIFY(AppInfo::copyrightLine().contains(QStringLiteral("Pat Wendorf")));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestAbout t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_about.moc"
