// test_settings — subtask 7 (first item): the typed QSettings wrapper.
//
// Every test runs against an ISOLATED QSettings in a QTemporaryDir
// (QSettings::IniFormat + QSettings::UserConfig), so the user's real
// mdit settings are never read or written. It asserts the spec defaults
// (theme auto, preview visible, splitter 0.5, recent cap 5), typed
// round-trips through a fresh QSettings instance (real disk persistence),
// and the recent-files cap / most-recent-first / dedupe behavior.
//
// Pure-logic test (no QApplication): QTEST_GUILESS_MAIN.
#include "Settings.h"

#include "testmain.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

class TestSettings : public QObject
{
    Q_OBJECT

private:
    // A throwaway temp dir (QTemporaryDir's default ctor always creates a valid
    // dir under the system temp). The dir is returned by value so the caller
    // keeps it alive for the scope; the QSettings points at <dir>/mdit/test.ini.
    QTemporaryDir makeTempDir()
    {
        return QTemporaryDir();
    }

    static QSettings tempSettings(const QString &dir)
    {
        // Isolate by backing the settings on an explicit file path (this Qt
        // build has no QSettings::UserConfig scope, only the fileName/Format
        // ctor for a portable temp-file backend).
        return QSettings(dir + QStringLiteral("/mdit/test.ini"),
                         QSettings::IniFormat);
    }

private slots:
    // -- identity ---------------------------------------------------------

    void defaultOrgAndApp()
    {
        QCOMPARE(Settings::defaultOrganization(), QStringLiteral("mdit"));
        QCOMPARE(Settings::defaultApplication(), QStringLiteral("mdit"));
        QCOMPARE(Settings::defaultRecentCap(), 5);
    }

    // -- defaults on a fresh (empty) settings -----------------------------

    void defaultsOnFreshSettings()
    {
        const QTemporaryDir dir = makeTempDir();
        QSettings ini = tempSettings(dir.path());
        Settings s(&ini);

        // The shipped defaults: light, Default accent, 100% UI scale.
        QCOMPARE(s.theme(), Settings::ThemeMode::Light);
        QCOMPARE(s.themeAccent(), QStringLiteral("default"));
        QCOMPARE(s.uiScale(), 100);
        QCOMPARE(s.previewVisible(), true);
        QVERIFY(qAbs(s.splitterRatio() - 0.5) < 1e-9);
        QCOMPARE(s.recentFiles(), QStringList());
        QCOMPARE(s.recentCap(), 5);
    }

    // Regression guard: a test run must NEVER write the user's real config.
    // (The theme suite once left theme=dark + themeAccent=red in
    // ~/.config/mdit/mdit.conf, so mdit came back looking "reset" to a theme
    // nobody picked.) `isolateUserSettings()` runs in main(); this proves it.
    void defaultSettingsAreIsolatedFromTheUserConfig()
    {
        // The harness seam is active…
        QVERIFY2(qEnvironmentVariableIsSet("MDIT_SETTINGS_INI"),
                 "tests/testmain.h did not install the settings seam");
        const QString seamPath = qEnvironmentVariable("MDIT_SETTINGS_INI");
        QVERIFY2(seamPath.startsWith(isolatedConfigHome()),
                 qPrintable(QStringLiteral("seam outside the temp dir: %1").arg(seamPath)));

        // …and it lives in a TEMP dir, never in the user's home config — the real
        // invariant (~/.config/mdit on Linux, ~/Library/Preferences on macOS, the
        // registry on Windows). NOTE: the native QSettings path is still
        // redirected on Linux (native == ini there) but not on macOS/Windows, so
        // assert the seam's location rather than comparing the two paths.
        QVERIFY2(seamPath.startsWith(QDir::tempPath()),
                 qPrintable(QStringLiteral("seam is not in a temp dir: %1").arg(seamPath)));
        QVERIFY2(!seamPath.startsWith(QDir::homePath() + QStringLiteral("/.config")),
                 qPrintable(seamPath));
        QVERIFY2(!seamPath.startsWith(QDir::homePath() + QStringLiteral("/Library")),
                 qPrintable(seamPath));

        // A write through the DEFAULT Settings (no backing) lands in the seam ini.
        Settings s;
        s.setTheme(Settings::ThemeMode::Dark);
        s.setThemeAccent(QStringLiteral("red"));
        s.setUiScale(110);
        s.sync();
        QSettings raw(seamPath, QSettings::IniFormat);
        QCOMPARE(raw.value(QStringLiteral("themeAccent")).toString(), QStringLiteral("red"));
        QCOMPARE(raw.value(QStringLiteral("uiScale")).toInt(), 110);

        // Leave the isolated store on the shipped defaults for later tests.
        s.setTheme(Settings::ThemeMode::Light);
        s.setThemeAccent(QStringLiteral("default"));
        s.setUiScale(100);
        s.sync();
    }

    // -- theme round-trip --------------------------------------------------

    void themeRoundTrip()
    {
        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setTheme(Settings::ThemeMode::Dark);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.theme(), Settings::ThemeMode::Dark);
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            s.setTheme(Settings::ThemeMode::Light);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.theme(), Settings::ThemeMode::Light);
        }
    }

    // -- preview visibility round-trip ------------------------------------
    // -- theme accent round-trip ------------------------------------------

    void themeAccentRoundTripAndEmptyFallsBackToDefault()
    {
        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setThemeAccent(QStringLiteral("purple"));
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.themeAccent(), QStringLiteral("purple"));
            s.setThemeAccent(QString()); // empty -> the default accent
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.themeAccent(), QStringLiteral("default"));
        }
        QCOMPARE(Settings::defaultThemeAccent(), QStringLiteral("default"));
    }

    // -- UI scale round-trip + clamping ------------------------------------

    void uiScaleRoundTripAndClamps()
    {
        QCOMPARE(Settings::defaultUiScale(), 100);
        QCOMPARE(Settings::minUiScale(), 50);
        QCOMPARE(Settings::maxUiScale(), 300);

        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setUiScale(150);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.uiScale(), 150);
            s.setUiScale(5); // below the floor -> clamped on write
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.uiScale(), Settings::minUiScale());
            s.setUiScale(9999); // above the ceiling -> clamped
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.uiScale(), Settings::maxUiScale());
        }

        // A nonsense value already on disk is clamped on read too.
        const QTemporaryDir dir2 = makeTempDir();
        QSettings ini3 = tempSettings(dir2.path());
        ini3.setValue(QStringLiteral("uiScale"), 12345);
        ini3.sync();
        Settings s(&ini3);
        QCOMPARE(s.uiScale(), Settings::maxUiScale());
    }

    void previewVisibleRoundTrip()
    {
        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setPreviewVisible(false);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.previewVisible(), false);
        }
    }

    // -- splitter ratio round-trip + clamping ------------------------------

    void splitterRatioRoundTrip()
    {
        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setSplitterRatio(0.3);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QVERIFY(qAbs(s.splitterRatio() - 0.3) < 1e-9);
        }
    }

    void splitterRatioClampsToValidRange()
    {
        const QTemporaryDir dir = makeTempDir();
        QSettings ini = tempSettings(dir.path());
        Settings s(&ini);

        // Out-of-range writes fall back to the 50/50 default.
        s.setSplitterRatio(1.7);
        QVERIFY(qAbs(s.splitterRatio() - 0.5) < 1e-9);
        s.setSplitterRatio(-1.0);
        QVERIFY(qAbs(s.splitterRatio() - 0.5) < 1e-9);

        // Boundary values are valid and round-trip.
        s.setSplitterRatio(0.0);
        QVERIFY(qAbs(s.splitterRatio() - 0.0) < 1e-9);
        s.setSplitterRatio(1.0);
        QVERIFY(qAbs(s.splitterRatio() - 1.0) < 1e-9);
    }

    // -- recent files: round-trip, cap, order, dedupe ----------------------

    void recentFilesRoundTrip()
    {
        const QStringList files{QStringLiteral("/a/one.md"),
                                QStringLiteral("/b/two.md"),
                                QStringLiteral("/c/three.md")};
        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setRecentFiles(files);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.recentFiles(), files);
        }
    }

    void setRecentFilesTrimsToCapAndDedupes()
    {
        const QTemporaryDir dir = makeTempDir();
        QSettings ini = tempSettings(dir.path());
        Settings s(&ini);

        QStringList seven;
        for (int i = 0; i < 7; ++i)
            seven.append(QStringLiteral("/f/%1.md").arg(i));
        // Duplicate one entry so the list has 8 raw entries.
        seven.append(seven.first());
        s.setRecentFiles(seven);

        // Duplicates dropped -> 7 unique; cap 5 trims to the first 5.
        QCOMPARE(s.recentFiles().size(), 5);
        QCOMPARE(s.recentFiles(), seven.mid(0, 5));
        QVERIFY(!s.recentFiles().contains(seven.first(), Qt::CaseSensitive)
                || s.recentFiles().count(seven.first()) == 1);
    }

    void addRecentFileCappedMostRecentFirst()
    {
        const QTemporaryDir dir = makeTempDir();
        QSettings ini = tempSettings(dir.path());
        Settings s(&ini);

        for (int i = 0; i < 6; ++i)
            s.addRecentFile(QStringLiteral("/r/%1.md").arg(i));

        // Cap of 5: the 6th (oldest, /r/0.md) is dropped, most-recent first.
        QCOMPARE(s.recentFiles().size(), 5);
        QCOMPARE(s.recentFiles().first(), QStringLiteral("/r/5.md"));
        QCOMPARE(s.recentFiles().last(), QStringLiteral("/r/1.md"));
        QVERIFY(!s.recentFiles().contains(QStringLiteral("/r/0.md")));
    }

    void addRecentFileMovesExistingToFrontAndDedupes()
    {
        const QTemporaryDir dir = makeTempDir();
        QSettings ini = tempSettings(dir.path());
        Settings s(&ini);

        s.addRecentFile(QStringLiteral("/x/a.md"));
        s.addRecentFile(QStringLiteral("/x/b.md"));
        s.addRecentFile(QStringLiteral("/x/c.md"));
        const QStringList expectFront{QStringLiteral("/x/c.md"),
                                      QStringLiteral("/x/b.md"),
                                      QStringLiteral("/x/a.md")};
        QCOMPARE(s.recentFiles(), expectFront);

        // Re-adding an existing entry moves it to the front without growing
        // the list.
        s.addRecentFile(QStringLiteral("/x/a.md"));
        QCOMPARE(s.recentFiles().size(), 3);
        QCOMPARE(s.recentFiles().first(), QStringLiteral("/x/a.md"));

        // Empty paths are ignored.
        s.addRecentFile(QString());
        QCOMPARE(s.recentFiles().size(), 3);
    }

    void recentCapIsSettableAndPersists()
    {
        const QTemporaryDir dir = makeTempDir();
        {
            QSettings ini = tempSettings(dir.path());
            Settings s(&ini);
            s.setRecentCap(2);
            for (int i = 0; i < 4; ++i)
                s.addRecentFile(QStringLiteral("/cap/%1.md").arg(i));
            QCOMPARE(s.recentFiles().size(), 2);
            s.sync();
        }
        {
            QSettings ini2 = tempSettings(dir.path());
            Settings s(&ini2);
            QCOMPARE(s.recentCap(), 2);
            QCOMPARE(s.recentFiles().size(), 2);
            // A negative cap is coerced to the default.
            s.setRecentCap(-1);
            QCOMPARE(s.recentCap(), Settings::defaultRecentCap());
        }
    }

    // -- real disk persistence --------------------------------------------

    void writesReachDisk()
    {
        const QTemporaryDir dir = makeTempDir();
        QSettings ini = tempSettings(dir.path());
        Settings s(&ini);
        s.setTheme(Settings::ThemeMode::Dark);
        s.setPreviewVisible(false);
        s.setSplitterRatio(0.25);
        s.addRecentFile(QStringLiteral("/d/disk.md"));
        s.sync();

        // An ini file was written under <dir>/mdit (the exact file name is a
        // Qt/platform detail — just assert it landed on disk).
        const QStringList inis =
            QDir(dir.path() + QStringLiteral("/mdit"))
                .entryList(QStringList{QStringLiteral("*.ini")}, QDir::Files);
        QVERIFY(!inis.isEmpty());
        // And the written value is actually in the file on disk.
        QFile f(dir.path() + QStringLiteral("/mdit/") + inis.first());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString fileContents = f.readAll();
        QVERIFY(fileContents.contains(QStringLiteral("dark")));
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "test_settings.moc"
