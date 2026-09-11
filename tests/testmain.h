// testmain.h — shared test harness helpers.
//
// `isolateUserSettings()` MUST be called by every test's main() that builds a
// MainWindow (i.e. anything that can persist a preference).
//
// Why: MainWindow's default constructor uses the REAL user settings
// (QSettings("mdit", "mdit") -> ~/.config/mdit/mdit.conf). Tests that exercise
// the theme, recent files, the splitter ratio or preview visibility therefore
// used to write into the developer's/our own config — and that is not a
// hypothetical: the theme suite left `theme=dark`, `themeAccent=red` and
// `uiScale=110` behind, so mdit started up looking "reset" to a theme nobody
// chose, and recentFiles filled with /tmp/test_*/ paths.
//
// Qt resolves the native (ini on Unix) path from the QSettings path table, so
// pointing that table at a process-lifetime temp dir redirects EVERY default
// QSettings in the test binary — no per-test plumbing, and a test can no longer
// touch the user's config even by accident. Tests that need explicit control
// still pass their own backing QSettings, as before.
#pragma once

#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

// The temp dir the isolated settings live in (shared by the helper below).
inline QString &isolatedConfigHomeRef()
{
    static QString path;
    return path;
}

inline void isolateUserSettings()
{
    // One temp dir for the whole test process (a function-local static: the
    // first call creates it, every later call reuses it).
    static QTemporaryDir dir;
    Q_ASSERT(dir.isValid());
    // Belt and braces: redirect the Qt settings path table (verified to work on
    // this Qt) and XDG_CONFIG_HOME for anything else that reads it.
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    isolatedConfigHomeRef() = dir.path();
}

// Run the isolation at LOAD time for any binary that includes this header, so a
// test can never forget it (and it covers QTEST_GUILESS_MAIN binaries, which
// build their own QCoreApplication inside QtTest's main).
namespace testmain_detail {
struct AutoIsolateSettings {
    AutoIsolateSettings() { isolateUserSettings(); }
};
const AutoIsolateSettings autoIsolateSettings;
} // namespace testmain_detail

// The temp dir the isolated settings live in.
inline QString isolatedConfigHome()
{
    return isolatedConfigHomeRef();
}

// The path QSettings("mdit","mdit") would use right now (Qt has no static path
// getter, so ask a throwaway instance - it honours setPath()).
inline QString defaultSettingsFilePath()
{
    return QSettings(QStringLiteral("mdit"), QStringLiteral("mdit")).fileName();
}

// The directory the isolated settings live in (for tests asserting they are NOT
// writing to the real user config).
inline QString isolatedSettingsDir()
{
    return isolatedConfigHome();
}
