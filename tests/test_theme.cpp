// test_theme — the theming system (mode x accent x UI scale).
//
// mdit's theming was rebuilt on PengyCPP's model: a palette is generated from a
// **mode** (System / Light / Dark) x **accent** (Default + 7 colours), and a
// **UI scale** multiplies the fonts and the explicit widget metrics. This suite
// pins:
//   * the pure pieces — `resolveDark()`, the accent list/titles, the scale
//     helpers (clamping, factors) and `blend()`/`contrastingText()`;
//   * every generated palette is COMPLETE (`Theme::tokenKeys()` all present) and
//     self-consistent (mode token, legible fg-on-bg, contrasting primary text);
//   * the **Default** accent still reproduces mdit's original neutral colours
//     (the app must not change unless an accent is picked), while another accent
//     tints the surfaces and drives `primary`/`link`;
//   * `appStyleSheet()` really embeds the palette (no unsubstituted
//     placeholders!) and scales its metrics with the UI scale;
//   * `buildPalette()`/`apply()` map tokens onto the QPalette + the application
//     font;
//   * the MainWindow wiring — the persisted mode/accent/scale are applied on
//     startup, the View > Theme / Accent / UI Scale menus (and the menu-bar
//     corner popup) reflect the live state, changing any of them re-themes the
//     app + persists, the preview follows the theme, and the editor highlighter
//     follows the accent;
//   * the menu-bar corner button (the sun/moon toggle) keeps working.
//
// Widget test → a custom main builds a QApplication. The offscreen platform
// plugin is set by add_mdit_test() in tests/CMakeLists.
#include "testmain.h"
#include "EditorPane.h"
#include "MainWindow.h"
#include "MarkdownHighlighter.h"
#include "PreviewPane.h"
#include "RenderedDocument.h"
#include "Settings.h"
#include "Theme.h"

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>

namespace {

// The last covering (start,count,format) range of a line under the highlighter
// rule engine (mirrors test_editor/test_highlighter).
QTextCharFormat fmtIn(const MarkdownHighlighter &h, const QString &line, int pos)
{
    const auto res = h.formatsForLine(line, /*inFence=*/false);
    QTextCharFormat result;
    for (const auto &r : res.ranges)
        if (pos >= r.start && pos < r.start + r.count)
            result = r.format;
    return result;
}

double luma(const QColor &c)
{
    return 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue();
}

int channelDelta(const QColor &a, const QColor &b)
{
    return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
}

} // namespace

class TestTheme : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir makeTempDir() { return QTemporaryDir(); }

    static QSettings tempSettings(const QString &dir, const QString &name)
    {
        return QSettings(dir + QStringLiteral("/mdit/") + name, QSettings::IniFormat);
    }

private slots:
    // -- pure logic ---------------------------------------------------------
    void resolveDarkMapping();
    void accentListAndTitles();
    void everyPaletteIsCompleteAndLegible();
    void defaultAccentKeepsTheOriginalNeutralLook();
    void accentsTintTheSurfacesAndSetTheHighlight();
    void blendAndContrastHelpers();

    // -- UI scale -----------------------------------------------------------
    void uiScaleHelpers();
    void appStyleSheetIsTokenDrivenAndScales();

    // -- applying -----------------------------------------------------------
    void buildPaletteMapsTheTokens();
    void applySetsFusionFontAndQss();

    // -- MainWindow wiring --------------------------------------------------
    void mainwindowResolvesPersistedThemeOnStartup();
    void mainwindowAppliesPersistedAccentAndScale();
    void menusReflectAndDriveTheTheme();
    void cornerPopupReusesTheSameActions();
    void changingTheThemeRepaintsPreviewAndHighlighter();
    void toggleFlipsBetweenLightAndDark();
    void menubarThemeButtonShowsSunOrMoonInTheCorner();
    void menubarThemeButtonFollowsPersistedTheme();
};

// -- pure logic ---------------------------------------------------------------

void TestTheme::resolveDarkMapping()
{
    QCOMPARE(Theme::resolveDark(Settings::ThemeMode::Dark, /*sys=*/false), true);
    QCOMPARE(Theme::resolveDark(Settings::ThemeMode::Dark, /*sys=*/true), true);
    QCOMPARE(Theme::resolveDark(Settings::ThemeMode::Light, /*sys=*/false), false);
    QCOMPARE(Theme::resolveDark(Settings::ThemeMode::Light, /*sys=*/true), false);
    // "System" (the Settings::Auto enum) follows the platform scheme.
    QCOMPARE(Theme::resolveDark(Settings::ThemeMode::Auto, /*sys=*/false), false);
    QCOMPARE(Theme::resolveDark(Settings::ThemeMode::Auto, /*sys=*/true), true);
    // ...and Theme::make() agrees for all three modes.
    QCOMPARE(Theme::make(Settings::ThemeMode::Auto, QStringLiteral("default"), false).dark, false);
    QCOMPARE(Theme::make(Settings::ThemeMode::Auto, QStringLiteral("default"), true).dark, true);
    QCOMPARE(Theme::make(Settings::ThemeMode::Dark, QStringLiteral("default"), false).dark, true);
    QCOMPARE(Theme::make(Settings::ThemeMode::Light, QStringLiteral("default"), true).dark, false);
}

void TestTheme::accentListAndTitles()
{
    const QStringList accents = Theme::accents();
    QCOMPARE(accents.size(), 8);
    QCOMPARE(accents.first(), QStringLiteral("default"));
    for (const QString &a : QStringList{QStringLiteral("blue"), QStringLiteral("teal"),
                                        QStringLiteral("green"), QStringLiteral("orange"),
                                        QStringLiteral("red"), QStringLiteral("pink"),
                                        QStringLiteral("purple")})
        QVERIFY2(accents.contains(a), qPrintable(a));
    QCOMPARE(Theme::defaultAccent(), QStringLiteral("default"));
    QCOMPARE(Theme::accentTitleFor(QStringLiteral("purple")), QStringLiteral("Purple"));
    QCOMPARE(Theme::accentTitleFor(QStringLiteral("default")), QStringLiteral("Default"));
    QVERIFY(Theme::isKnownAccent(QStringLiteral("teal")));
    QVERIFY(!Theme::isKnownAccent(QStringLiteral("chartreuse")));
    // An unknown accent normalizes to Default rather than producing a broken
    // palette.
    QCOMPARE(Theme::make(/*dark=*/false, QStringLiteral("chartreuse")).accent,
             QStringLiteral("default"));
    QCOMPARE(Theme::make(/*dark=*/true, QString()).accent, QStringLiteral("default"));
}

// Every one of the 16 palettes has every token, the right mode/accent markers
// and legible text.
void TestTheme::everyPaletteIsCompleteAndLegible()
{
    const QStringList keys = Theme::tokenKeys();
    QVERIFY(keys.size() > 30);

    for (const QString &accent : Theme::accents()) {
        for (const bool dark : {false, true}) {
            const Theme t = Theme::make(dark, accent);
            const QString where = QStringLiteral("%1/%2")
                                      .arg(accent, dark ? QStringLiteral("dark")
                                                        : QStringLiteral("light"));
            for (const QString &key : keys) {
                QVERIFY2(!t[key].isEmpty(),
                         qPrintable(QStringLiteral("%1: missing token %2").arg(where, key)));
                // mode/accent/accent_title are metadata strings, not colours.
                if (key == QLatin1String("mode") || key == QLatin1String("accent")
                    || key == QLatin1String("accent_title"))
                    continue;
                QVERIFY2(QColor(t[key]).isValid(),
                         qPrintable(QStringLiteral("%1: %2 is not a colour (%3)")
                                        .arg(where, key, t[key])));
            }
            QCOMPARE(t[QStringLiteral("mode")],
                     dark ? QStringLiteral("dark") : QStringLiteral("light"));
            QCOMPARE(t[QStringLiteral("accent")], accent);
            QCOMPARE(t.dark, dark);
            QCOMPARE(t.accent, accent);
            QCOMPARE(t.accentTitle, Theme::accentTitleFor(accent));

            // Text must be legible on its surfaces (a big luma gap), and the
            // "button text on the accent" contrast must be sane too.
            QVERIFY2(qAbs(luma(t.color(QStringLiteral("fg")))
                           - luma(t.color(QStringLiteral("bg")))) > 90.0,
                     qPrintable(where + QStringLiteral(": fg/bg lack contrast")));
            QVERIFY2(qAbs(luma(t.color(QStringLiteral("input_fg")))
                           - luma(t.color(QStringLiteral("base")))) > 90.0,
                     qPrintable(where + QStringLiteral(": input_fg/base lack contrast")));
            QVERIFY2(qAbs(luma(t.color(QStringLiteral("primary_fg")))
                           - luma(t.color(QStringLiteral("primary")))) > 60.0,
                     qPrintable(where + QStringLiteral(": primary_fg/primary lack contrast")));
            QVERIFY2(qAbs(luma(t.color(QStringLiteral("selection_fg")))
                           - luma(t.color(QStringLiteral("selection")))) > 60.0,
                     qPrintable(where + QStringLiteral(": selection_fg/selection lack contrast")));
        }
    }
}

// The Default accent is byte-for-byte mdit's original look in both modes.
void TestTheme::defaultAccentKeepsTheOriginalNeutralLook()
{
    const Theme light = Theme::make(false);
    QCOMPARE(light[QStringLiteral("bg")], QStringLiteral("#f0f0f0"));
    QCOMPARE(light[QStringLiteral("fg")], QStringLiteral("#1e1e1e"));
    QCOMPARE(light[QStringLiteral("base")], QStringLiteral("#ffffff"));
    QCOMPARE(light[QStringLiteral("primary")], QStringLiteral("#2a82da"));
    QCOMPARE(light[QStringLiteral("doc_bg")], QStringLiteral("#ffffff"));
    QCOMPARE(light[QStringLiteral("doc_link")], QStringLiteral("#2a82da"));

    const Theme dark = Theme::make(true);
    QCOMPARE(dark[QStringLiteral("bg")], QStringLiteral("#1e1e1e"));
    QCOMPARE(dark[QStringLiteral("base")], QStringLiteral("#181818"));
    QCOMPARE(dark[QStringLiteral("fg")], QStringLiteral("#dcdcdc"));
    QCOMPARE(dark[QStringLiteral("doc_bg")], QStringLiteral("#1e1e1e"));
    QCOMPARE(dark[QStringLiteral("doc_link")], QStringLiteral("#6cb2f0"));
}

// A non-default accent tints the surfaces toward the accent and drives the
// highlight/link colours — that is the whole point of the accent system.
void TestTheme::accentsTintTheSurfacesAndSetTheHighlight()
{
    const Theme neutralLight = Theme::make(false);
    const Theme purpleLight = Theme::make(false, QStringLiteral("purple"));
    const Theme purpleDark = Theme::make(true, QStringLiteral("purple"));

    // The highlight IS the accent…
    QCOMPARE(purpleLight[QStringLiteral("primary")], QStringLiteral("#8839ef"));
    QVERIFY(luma(purpleLight.color(QStringLiteral("primary"))) > luma(neutralLight.color(QStringLiteral("primary")))
            || purpleLight[QStringLiteral("primary")] != neutralLight[QStringLiteral("primary")]);
    QCOMPARE(purpleLight[QStringLiteral("link")], purpleLight[QStringLiteral("primary")]);

    // …and the surfaces are tinted, but only subtly (still a usable UI).
    QVERIFY(purpleLight[QStringLiteral("bg")] != neutralLight[QStringLiteral("bg")]);
    QVERIFY(purpleDark[QStringLiteral("bg")] != Theme::make(true)[QStringLiteral("bg")]);
    QVERIFY(channelDelta(purpleLight.color(QStringLiteral("bg")),
                         neutralLight.color(QStringLiteral("bg"))) <= 40);
    QVERIFY(channelDelta(purpleDark.color(QStringLiteral("bg")),
                         Theme::make(true).color(QStringLiteral("bg"))) <= 60);

    // The accent names differ per accent (no accidental cookie-cutter palettes).
    QVERIFY(Theme::make(false, QStringLiteral("teal"))[QStringLiteral("bg")]
            != Theme::make(false, QStringLiteral("red"))[QStringLiteral("bg")]);
}

void TestTheme::blendAndContrastHelpers()
{
    QCOMPARE(Theme::blend(QColor(0, 0, 0), QColor(255, 255, 255), 0.5), QColor(128, 128, 128));
    QCOMPARE(Theme::blend(QColor(10, 20, 30), QColor(10, 20, 30), 0.9), QColor(10, 20, 30));
    QCOMPARE(Theme::blend(QColor(0, 0, 0), QColor(9, 9, 9), 0.0), QColor(0, 0, 0));
    QCOMPARE(Theme::blend(QColor(0, 0, 0), QColor(9, 9, 9), 2.0), QColor(9, 9, 9)); // clamped
    QVERIFY(Theme::isDarkColor(QColor(20, 20, 20)));
    QVERIFY(!Theme::isDarkColor(QColor(240, 240, 240)));
    QCOMPARE(Theme::contrastingText(QColor(20, 20, 20)), QColor(0xff, 0xff, 0xff));
    QCOMPARE(Theme::contrastingText(QColor(240, 240, 240)), QColor(0x1e, 0x1e, 0x1e));
}

// -- UI scale -----------------------------------------------------------------

void TestTheme::uiScaleHelpers()
{
    QCOMPARE(Theme::defaultScale(), 100);
    QCOMPARE(Theme::minScale(), Settings::minUiScale());
    QCOMPARE(Theme::maxScale(), Settings::maxUiScale());
    QCOMPARE(Theme::clampScale(10), 50);
    QCOMPARE(Theme::clampScale(1000), 300);
    QCOMPARE(Theme::clampScale(150), 150);
    QCOMPARE(Theme::scaleFactor(100), 1.0);
    QCOMPARE(Theme::scaleFactor(150), 1.5);
    QCOMPARE(Theme::scaleFactor(10), 0.5);        // clamped to 50%
    QCOMPARE(Theme::scaled(8, 100), 8);
    QCOMPARE(Theme::scaled(8, 150), 12);
    QCOMPARE(Theme::scaled(8, 10), 4);            // clamped
    QCOMPARE(Theme::scaled(0, 100), 1);           // never zero-size
    QCOMPARE(Theme::scaledFont(10.0, 125), 12.5);

    const QList<int> scales = Theme::offeredScales();
    QVERIFY(scales.size() >= 5);
    QVERIFY(scales.contains(100));
    for (int i = 1; i < scales.size(); ++i)
        QVERIFY2(scales.at(i) > scales.at(i - 1), "offered scales must ascend");

    // The scaled fonts really grow with the scale (system + monospace).
    const QFont system100 = Theme::scaledSystemFont(100);
    const QFont system150 = Theme::scaledSystemFont(150);
    QVERIFY(system100.pointSizeF() > 0.0);
    QVERIFY2(system150.pointSizeF() > system100.pointSizeF(), "system font must scale");
    const QFont mono100 = Theme::scaledMonoFont(100);
    const QFont mono200 = Theme::scaledMonoFont(200);
    QVERIFY(mono100.pointSizeF() > 0.0);
    QVERIFY2(mono200.pointSizeF() > mono100.pointSizeF(), "mono font must scale");
}

void TestTheme::appStyleSheetIsTokenDrivenAndScales()
{
    const Theme theme = Theme::make(true, QStringLiteral("teal"));
    const QString sheet = Theme::appStyleSheet(theme, 100);

    QVERIFY(!sheet.isEmpty());
    // The palette is embedded…
    QVERIFY2(sheet.contains(theme[QStringLiteral("bg")]), qPrintable(theme[QStringLiteral("bg")]));
    QVERIFY(sheet.contains(theme[QStringLiteral("fg")]));
    QVERIFY(sheet.contains(theme[QStringLiteral("primary")]));
    QVERIFY(sheet.contains(theme[QStringLiteral("selection")]));
    QVERIFY(sheet.contains(theme[QStringLiteral("chrome_bg")]));
    // …with no unsubstituted placeholder left behind (the guard that keeps a
    // newly added token from silently rendering as "@{newtoken}").
    QVERIFY2(!sheet.contains(QStringLiteral("@{")), "unsubstituted token placeholder");
    QVERIFY2(!sheet.contains(QLatin1Char('%')), "positional placeholder leaked");

    // Metrics follow the UI scale.
    const QString sheet150 = Theme::appStyleSheet(theme, 150);
    QVERIFY(sheet150 != sheet);
    QVERIFY2(sheet150.contains(QStringLiteral("padding: 5px 12px")),
             qPrintable(sheet150.left(400))); // 3*1.5 -> 5, 8*1.5 -> 12
    QVERIFY(sheet.contains(QStringLiteral("padding: 3px 8px")));

    // Light vs dark really differ.
    QVERIFY(Theme::appStyleSheet(Theme::make(false), 100)
            != Theme::appStyleSheet(Theme::make(true), 100));
}

// -- applying -----------------------------------------------------------------

void TestTheme::buildPaletteMapsTheTokens()
{
    for (const bool dark : {false, true}) {
        const Theme theme = Theme::make(dark, QStringLiteral("green"));
        const QPalette pal = Theme::buildPalette(theme);
        QCOMPARE(pal.color(QPalette::Window), theme.color(QStringLiteral("bg")));
        QCOMPARE(pal.color(QPalette::WindowText), theme.color(QStringLiteral("fg")));
        QCOMPARE(pal.color(QPalette::Base), theme.color(QStringLiteral("base")));
        QCOMPARE(pal.color(QPalette::Text), theme.color(QStringLiteral("input_fg")));
        QCOMPARE(pal.color(QPalette::Highlight), theme.color(QStringLiteral("selection")));
        QCOMPARE(pal.color(QPalette::HighlightedText), theme.color(QStringLiteral("selection_fg")));
        QCOMPARE(pal.color(QPalette::ToolTipBase), theme.color(QStringLiteral("tooltip_bg")));
        // PlaceholderText is set too (an unset one was an earlier dark-mode bug).
        QCOMPARE(pal.color(QPalette::PlaceholderText), theme.color(QStringLiteral("muted")));
    }
}

void TestTheme::applySetsFusionFontAndQss()
{
    QApplication *app = qApp;
    QVERIFY(app != nullptr);

    Theme::apply(*app, Theme::make(true), 150);
    QVERIFY(app->style() != nullptr); // Fusion (installed by apply)
    QVERIFY(!app->styleSheet().isEmpty());
    QCOMPARE(app->palette().color(QPalette::Base), QColor(24, 24, 24)); // dark base
    // The UI scale rides on the application font.
    QCOMPARE(app->font().pointSizeF(), Theme::scaledSystemFont(150).pointSizeF());

    Theme::apply(*app, Theme::make(false), 100);
    QCOMPARE(app->palette().color(QPalette::Base), QColor(255, 255, 255)); // light base
    QVERIFY(app->styleSheet().contains(QStringLiteral("#f0f0f0")));        // light bg
    QCOMPARE(app->font().pointSizeF(), Theme::scaledSystemFont(100).pointSizeF());

    // Leave the application on a neutral theme for the remaining tests.
    Theme::apply(*app, Theme::make(false), 100);
}

// -- MainWindow wiring --------------------------------------------------------

void TestTheme::mainwindowResolvesPersistedThemeOnStartup()
{
    const QTemporaryDir dir = makeTempDir();

    QSettings darkIni = tempSettings(dir.path(), QStringLiteral("dark.ini"));
    darkIni.setValue(QStringLiteral("theme"), QStringLiteral("dark"));
    darkIni.sync();
    {
        MainWindow w(nullptr, &darkIni);
        QCOMPARE(w.darkTheme(), true);
        QCOMPARE(w.themeMode(), Settings::ThemeMode::Dark);
    }

    QSettings lightIni = tempSettings(dir.path(), QStringLiteral("light.ini"));
    lightIni.setValue(QStringLiteral("theme"), QStringLiteral("light"));
    lightIni.sync();
    {
        MainWindow w(nullptr, &lightIni);
        QCOMPARE(w.darkTheme(), false);
    }
}

// The accent + UI scale persist and are applied on startup (the whole point of
// stealing the system): palette, application font, editor font and preview.
void TestTheme::mainwindowAppliesPersistedAccentAndScale()
{
    const QTemporaryDir dir = makeTempDir();
    QSettings ini = tempSettings(dir.path(), QStringLiteral("accent.ini"));
    ini.setValue(QStringLiteral("theme"), QStringLiteral("dark"));
    ini.setValue(QStringLiteral("themeAccent"), QStringLiteral("purple"));
    ini.setValue(QStringLiteral("uiScale"), 150);
    ini.sync();

    MainWindow w(nullptr, &ini);
    QCOMPARE(w.themeMode(), Settings::ThemeMode::Dark);
    QCOMPARE(w.themeAccent(), QStringLiteral("purple"));
    QCOMPARE(w.uiScale(), 150);
    QVERIFY(w.darkTheme());

    const Theme theme = w.theme();
    QCOMPARE(theme.accent, QStringLiteral("purple"));
    QVERIFY(theme.dark);
    // Applied to the application…
    QCOMPARE(qApp->palette().color(QPalette::Base), theme.color(QStringLiteral("base")));
    QCOMPARE(qApp->font().pointSizeF(), Theme::scaledSystemFont(150).pointSizeF());
    // …to the editor (scaled monospace font + accent-tinted highlighter)…
    QCOMPARE(w.editorPane()->font().pointSizeF(), Theme::scaledMonoFont(150).pointSizeF());
    MarkdownHighlighter *h = w.editorPane()->highlighter();
    QVERIFY(h != nullptr);
    QCOMPARE(fmtIn(*h, QStringLiteral("# Title"), 2).foreground().color(),
             theme.color(QStringLiteral("primary")));
    // …and to the preview document's stylesheet.
    w.editorPane()->setPlainText(QStringLiteral("# Hi\n"));
    w.updateLivePreview();
    QTextDocument *doc = w.previewPane()->displayedQTextDocument();
    QVERIFY(doc != nullptr);
    QCOMPARE(doc->defaultStyleSheet(), RenderedDocument::stylesheet(theme));
    QVERIFY(doc->defaultStyleSheet().contains(theme[QStringLiteral("doc_link")]));
}

void TestTheme::menusReflectAndDriveTheTheme()
{
    const QTemporaryDir dir = makeTempDir();
    QSettings ini = tempSettings(dir.path(), QStringLiteral("menus.ini"));
    ini.setValue(QStringLiteral("theme"), QStringLiteral("dark"));
    ini.setValue(QStringLiteral("themeAccent"), QStringLiteral("default"));
    ini.setValue(QStringLiteral("uiScale"), 100);
    ini.sync();

    MainWindow w(nullptr, &ini);
    QVERIFY(w.themeMenu() != nullptr);
    QVERIFY(w.accentMenu() != nullptr);
    QVERIFY(w.uiScaleMenu() != nullptr);

    // The mode group has System/Light/Dark with the current one ticked.
    const QList<QAction *> modes = w.themeMenu()->actions();
    QCOMPARE(modes.size(), 3);
    QCOMPARE(w.themeModeAction(Settings::ThemeMode::System), modes.at(0));
    QCOMPARE(w.themeModeAction(Settings::ThemeMode::Light), modes.at(1));
    QCOMPARE(w.themeModeAction(Settings::ThemeMode::Dark), modes.at(2));
    for (QAction *a : modes)
        QVERIFY(a->isCheckable());
    QVERIFY(w.themeModeAction(Settings::ThemeMode::Dark)->isChecked());
    QVERIFY(!w.themeModeAction(Settings::ThemeMode::Light)->isChecked());

    // One action per accent / per offered scale, current ones ticked.
    QCOMPARE(w.accentMenu()->actions().size(), Theme::accents().size());
    QCOMPARE(w.uiScaleMenu()->actions().size(), Theme::offeredScales().size());
    QVERIFY(w.accentAction(QStringLiteral("default"))->isChecked());
    QVERIFY(w.uiScaleAction(100)->isChecked());

    // Drive them: each one re-themes + persists.
    w.accentAction(QStringLiteral("orange"))->trigger();
    QCOMPARE(w.themeAccent(), QStringLiteral("orange"));
    QCOMPARE(ini.value(QStringLiteral("themeAccent")).toString(), QStringLiteral("orange"));
    QCOMPARE(w.theme()[QStringLiteral("primary")], QStringLiteral("#df8e1d"));
    QVERIFY(w.accentAction(QStringLiteral("orange"))->isChecked());
    QVERIFY(!w.accentAction(QStringLiteral("default"))->isChecked());

    w.uiScaleAction(150)->trigger();
    QCOMPARE(w.uiScale(), 150);
    QCOMPARE(ini.value(QStringLiteral("uiScale")).toInt(), 150);
    QCOMPARE(qApp->font().pointSizeF(), Theme::scaledSystemFont(150).pointSizeF());
    QVERIFY(w.editorPane()->font().pointSizeF() > Theme::scaledMonoFont(100).pointSizeF());

    w.themeModeAction(Settings::ThemeMode::Light)->trigger();
    QCOMPARE(w.themeMode(), Settings::ThemeMode::Light);
    QCOMPARE(w.darkTheme(), false);
    QCOMPARE(ini.value(QStringLiteral("theme")).toString(), QStringLiteral("light"));
    QVERIFY(w.themeModeAction(Settings::ThemeMode::Light)->isChecked());
    QVERIFY(!w.themeModeAction(Settings::ThemeMode::Dark)->isChecked());
    // The accent survived the mode change.
    QCOMPARE(w.themeAccent(), QStringLiteral("orange"));
    QCOMPARE(w.theme()[QStringLiteral("mode")], QStringLiteral("light"));
}

void TestTheme::cornerPopupReusesTheSameActions()
{
    MainWindow w;
    QMenu *popup = w.themeOptionsMenu();
    QVERIFY(popup != nullptr);
    // 3 modes + 8 accents + 7 scales + 2 separators.
    QCOMPARE(popup->actions().size(), 3 + Theme::accents().size()
                                          + Theme::offeredScales().size() + 2);
    // The very same QAction objects as the View submenus (one source of truth).
    QVERIFY(popup->actions().contains(w.themeModeAction(Settings::ThemeMode::Dark)));
    QVERIFY(popup->actions().contains(w.accentAction(QStringLiteral("teal"))));
    QVERIFY(popup->actions().contains(w.uiScaleAction(125)));
    // The corner button opens it via its arrow, and still toggles on click.
    QToolButton *button = w.themeButton();
    QCOMPARE(button->menu(), popup);
    QCOMPARE(button->popupMode(), QToolButton::MenuButtonPopup);
}

void TestTheme::changingTheThemeRepaintsPreviewAndHighlighter()
{
    MainWindow w;
    w.editorPane()->setPlainText(QStringLiteral("# Heading\n\ntext\n"));
    w.updateLivePreview();

    // Light + Default first, then dark + red.
    w.setThemeMode(Settings::ThemeMode::Light);
    w.setThemeAccent(QStringLiteral("default"));
    QTextDocument *doc = w.previewPane()->displayedQTextDocument();
    QVERIFY(doc != nullptr);
    QCOMPARE(doc->defaultStyleSheet(), RenderedDocument::stylesheet(w.theme()));
    const QColor lightHeading =
        fmtIn(*w.editorPane()->highlighter(), QStringLiteral("# Heading"), 2).foreground().color();

    w.setThemeMode(Settings::ThemeMode::Dark);
    w.setThemeAccent(QStringLiteral("red"));
    QVERIFY(w.theme().dark);
    QCOMPARE(w.theme()[QStringLiteral("primary")], QStringLiteral("#d20f39"));
    // The preview re-rendered with the new theme's document stylesheet…
    QCOMPARE(doc->defaultStyleSheet(), RenderedDocument::stylesheet(w.theme()));
    QVERIFY(doc->defaultStyleSheet().contains(w.theme()[QStringLiteral("doc_bg")]));
    // …and the editor highlighter switched to the accent-tinted palette.
    const QColor darkHeading =
        fmtIn(*w.editorPane()->highlighter(), QStringLiteral("# Heading"), 2).foreground().color();
    QCOMPARE(darkHeading, w.theme().color(QStringLiteral("primary")));
    QVERIFY(darkHeading != lightHeading);
}

void TestTheme::toggleFlipsBetweenLightAndDark()
{
    const QTemporaryDir dir = makeTempDir();
    QSettings ini = tempSettings(dir.path(), QStringLiteral("toggle.ini"));
    ini.setValue(QStringLiteral("theme"), QStringLiteral("light"));
    ini.setValue(QStringLiteral("themeAccent"), QStringLiteral("teal"));
    ini.sync();

    MainWindow w(nullptr, &ini);
    QCOMPARE(w.darkTheme(), false);

    w.onToggleTheme();
    QVERIFY(w.darkTheme());
    QCOMPARE(w.themeMode(), Settings::ThemeMode::Dark);
    QCOMPARE(w.settings()->theme(), Settings::ThemeMode::Dark);
    QCOMPARE(ini.value(QStringLiteral("theme")).toString(), QStringLiteral("dark"));
    // The accent is untouched by the quick toggle.
    QCOMPARE(w.themeAccent(), QStringLiteral("teal"));
    QCOMPARE(w.theme()[QStringLiteral("primary")], QStringLiteral("#179299"));

    w.onToggleTheme();
    QVERIFY(!w.darkTheme());
    QCOMPARE(w.themeMode(), Settings::ThemeMode::Light);
    QCOMPARE(ini.value(QStringLiteral("theme")).toString(), QStringLiteral("light"));
    QCOMPARE(w.themeAccent(), QStringLiteral("teal"));
}

// The menu bar carries ONE sun/moon toggle in its right-hand corner (the app's
// only chrome row): sun while light, moon while dark, tooltip naming the state,
// the accent and the click action.
void TestTheme::menubarThemeButtonShowsSunOrMoonInTheCorner()
{
    const QTemporaryDir dir = makeTempDir();
    QSettings ini = tempSettings(dir.path(), QStringLiteral("button.ini"));
    ini.setValue(QStringLiteral("theme"), QStringLiteral("light"));
    ini.setValue(QStringLiteral("themeAccent"), QStringLiteral("default"));
    ini.sync();

    MainWindow w(nullptr, &ini);
    QAction *button = w.themeButtonAction();
    QVERIFY(button != nullptr);
    QVERIFY(w.themeAction() != nullptr); // the View-menu twin still exists

    QCOMPARE(button->text(), QStringLiteral("\u2600\uFE0F"));
    QVERIFY(button->toolTip().contains(QStringLiteral("Light")));
    QVERIFY(button->toolTip().contains(QStringLiteral("dark"))); // what a click does

    w.onToggleTheme();
    QVERIFY(w.darkTheme());
    QCOMPARE(button->text(), QStringLiteral("\U0001F319"));
    QVERIFY(button->toolTip().contains(QStringLiteral("Dark")));
    QVERIFY(button->toolTip().contains(QStringLiteral("light")));

    w.onToggleTheme();
    QCOMPARE(button->text(), QStringLiteral("\u2600\uFE0F"));

    // The button is the menu bar's RIGHT-HAND CORNER widget — right-aligned by
    // construction — and shares the menu action (no second shortcut).
    QToolButton *buttonWidget =
        qobject_cast<QToolButton *>(w.menuBar()->cornerWidget(Qt::TopRightCorner));
    QVERIFY2(buttonWidget != nullptr, "no corner widget on the menu bar");
    QCOMPARE(buttonWidget, w.themeButton());
    QCOMPARE(buttonWidget->defaultAction(), button);
    QVERIFY2(w.findChildren<QToolBar *>().isEmpty(),
             "the toolbar is gone - the menu bar is the only chrome row");

    // Only the menu twin owns Ctrl+T (no ambiguous shortcut pair).
    QVERIFY(w.themeAction()->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_T)));
    QVERIFY(button->shortcuts().isEmpty());
}

// The button (and the applied palette) start on the PERSISTED choice.
void TestTheme::menubarThemeButtonFollowsPersistedTheme()
{
    const QTemporaryDir dir = makeTempDir();
    QSettings ini = tempSettings(dir.path(), QStringLiteral("persisted.ini"));
    ini.setValue(QStringLiteral("theme"), QStringLiteral("dark"));
    ini.setValue(QStringLiteral("themeAccent"), QStringLiteral("blue"));
    ini.sync();

    MainWindow w(nullptr, &ini);
    QVERIFY(w.darkTheme());
    QCOMPARE(w.themeButtonAction()->text(), QStringLiteral("\U0001F319"));
    QVERIFY(w.themeButtonAction()->toolTip().contains(QStringLiteral("Blue")));

    w.onToggleTheme();
    QCOMPARE(ini.value(QStringLiteral("theme")).toString(), QStringLiteral("light"));
    QSettings ini2 = tempSettings(dir.path(), QStringLiteral("persisted.ini"));
    MainWindow w2(nullptr, &ini2);
    QVERIFY(!w2.darkTheme());
    QCOMPARE(w2.themeButtonAction()->text(), QStringLiteral("\u2600\uFE0F"));
    QCOMPARE(w2.themeAccent(), QStringLiteral("blue"));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestTheme t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_theme.moc"
