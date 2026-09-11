#include "Settings.h"

#include <QDir>
#include <QSettings>

// String keys. Kept in one place so a later key-rename is a single edit.
static const QString kTheme        = QStringLiteral("theme");
static const QString kThemeAccent  = QStringLiteral("themeAccent");
static const QString kUiScale      = QStringLiteral("uiScale");
static const QString kPreviewVisible = QStringLiteral("previewVisible");
static const QString kSplitterRatio = QStringLiteral("splitterRatio");
static const QString kRecentFiles  = QStringLiteral("recentFiles");
static const QString kRecentCap    = QStringLiteral("recentCap");

Settings::Settings(QSettings *backing)
    : m_owned(nullptr), m_borrowed(backing), m_s(backing)
{
    if (!m_s) {
        // Test harness seam (see Settings.h): an explicit ini file instead of the
        // platform's native store. Empty in production.
        const QString testIni = qEnvironmentVariable("MDIT_SETTINGS_INI");
        if (!testIni.isEmpty())
            m_owned = new QSettings(testIni, QSettings::IniFormat);
        else
            m_owned = new QSettings(defaultOrganization(), defaultApplication());
        m_s = m_owned;
    }
}

Settings::~Settings()
{
    delete m_owned;
}

// -- org / app ---------------------------------------------------------

QString Settings::defaultOrganization()
{
    return QStringLiteral("mdit");
}

QString Settings::defaultApplication()
{
    return QStringLiteral("mdit");
}

// -- theme -------------------------------------------------------------

Settings::ThemeMode Settings::theme() const
{
    // Light is the shipped default (see Settings.h): a fresh mdit looks the same
    // on every desktop; "System" is an explicit choice the user can make.
    return themeFromKey(m_s->value(kTheme, QStringLiteral("light")).toString());
}

void Settings::setTheme(ThemeMode mode)
{
    m_s->setValue(kTheme, themeToKey(mode));
}

// -- theme accent -------------------------------------------------------

QString Settings::themeAccent() const
{
    return m_s->value(kThemeAccent, defaultThemeAccent()).toString();
}

void Settings::setThemeAccent(const QString &accent)
{
    m_s->setValue(kThemeAccent, accent.isEmpty() ? defaultThemeAccent() : accent);
}

QString Settings::defaultThemeAccent()
{
    return QStringLiteral("default");
}

// -- UI scale -----------------------------------------------------------

int Settings::uiScale() const
{
    return qBound(minUiScale(), m_s->value(kUiScale, defaultUiScale()).toInt(),
                  maxUiScale());
}

void Settings::setUiScale(int percent)
{
    m_s->setValue(kUiScale, qBound(minUiScale(), percent, maxUiScale()));
}

int Settings::minUiScale()
{
    return 50;
}

int Settings::maxUiScale()
{
    return 300;
}

int Settings::defaultUiScale()
{
    return 100;
}

// -- preview visibility ------------------------------------------------

bool Settings::previewVisible() const
{
    return m_s->value(kPreviewVisible, true).toBool();
}

void Settings::setPreviewVisible(bool visible)
{
    m_s->setValue(kPreviewVisible, visible);
}

// -- splitter ratio ----------------------------------------------------

double Settings::splitterRatio() const
{
    const double r = m_s->value(kSplitterRatio, 0.5).toDouble();
    return ratioValid(r) ? r : 0.5;
}

void Settings::setSplitterRatio(double ratio)
{
    if (!ratioValid(ratio))
        ratio = 0.5;
    m_s->setValue(kSplitterRatio, ratio);
}

// -- recent files ------------------------------------------------------

QStringList Settings::recentFiles() const
{
    return m_s->value(kRecentFiles).toStringList();
}

void Settings::setRecentFiles(const QStringList &files)
{
    QStringList out;
    for (const QString &f : files) {
        if (f.isEmpty() || out.contains(f))
            continue;
        out.append(f);
    }
    const int cap = recentCap();
    if (out.size() > cap)
        out = out.mid(0, cap);
    m_s->setValue(kRecentFiles, out);
}

void Settings::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    QStringList out;
    out.append(path); // most recent first
    for (const QString &f : recentFiles()) {
        if (f != path)
            out.append(f);
    }
    const int cap = recentCap();
    if (out.size() > cap)
        out = out.mid(0, cap);
    m_s->setValue(kRecentFiles, out);
}

int Settings::recentCap() const
{
    const int cap = m_s->value(kRecentCap, defaultRecentCap()).toInt();
    return cap < 0 ? defaultRecentCap() : cap;
}

void Settings::setRecentCap(int cap)
{
    if (cap < 0)
        cap = defaultRecentCap();
    m_s->setValue(kRecentCap, cap);
}

int Settings::defaultRecentCap()
{
    return 5;
}

// -- sync --------------------------------------------------------------

void Settings::sync()
{
    m_s->sync();
}

// -- helpers -----------------------------------------------------------

Settings::ThemeMode Settings::themeFromKey(const QString &key)
{
    if (key == QLatin1String("light"))
        return ThemeMode::Light;
    if (key == QLatin1String("dark"))
        return ThemeMode::Dark;
    return ThemeMode::Auto;
}

QString Settings::themeToKey(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Dark:
        return QStringLiteral("dark");
    case ThemeMode::Auto:
        return QStringLiteral("auto");
    }
    return QStringLiteral("auto");
}

bool Settings::ratioValid(double ratio)
{
    return ratio >= 0.0 && ratio <= 1.0;
}
