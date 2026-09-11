// Settings — mdit's typed QSettings wrapper.
//
// Owns persistence for the handful of user-preference values the app keeps:
//   • theme           (system / light / dark)  — default light
//   • themeAccent     ("default" | blue | … )  — default "default"
//   • uiScale         (percent, 50..300)        — default 100
//   • previewVisible  (bool)                    — default true
//   • splitterRatio   (0.0..1.0)                — default 0.5 (50/50)
//   • recentFiles     (capped list)             — default empty, cap 5
//
// The class is a thin, *typed* layer over QSettings: callers never see string
// keys or raw QVariant types — they get an enum, a bool, a double and a
// QStringList. It is GUI-independent (no QWidget) so it is exercised headlessly
// in `test_settings`.
//
// Isolation for tests: the default constructor uses the real user settings
// under org/app "mdit"/"mdit". To avoid clobbering a user's real settings,
// a backing QSettings may be supplied (tests point it at a QTemporaryDir with
// QSettings::IniFormat + QSettings::UserConfig); when one is supplied the real
// settings are neither read nor written.
#pragma once

#include <QString>
#include <QStringList>

class QSettings;

class Settings
{
public:
    // The three theme modes. `Auto` follows the platform colour scheme; the UI
    // (and new code) spells it "System" — same value, clearer name.
    // The SHIPPED DEFAULT is Light (with the Default accent and 100% UI scale),
    // so a fresh install always looks the same regardless of the desktop theme;
    // System is opt-in.
    enum class ThemeMode { Auto, Light, Dark, System = Auto };

    // Uses the real user settings (org/app "mdit") unless `backing` is
    // supplied. When `backing` is non-null it is used as-is and NOT deleted
    // here (the caller owns it).
    explicit Settings(QSettings *backing = nullptr);
    ~Settings();

    // -- org / app ---------------------------------------------------------
    static QString defaultOrganization();  // "mdit"
    static QString defaultApplication();   // "mdit"

    // -- theme (system / light / dark) -------------------------------------
    // NOTE: the enum keeps its original member names (Auto == "follow the
    // system") for compatibility; the UI calls it "System".
    ThemeMode theme() const;               // default Light (opt into System)
    void setTheme(ThemeMode mode);

    // -- theme accent -------------------------------------------------------
    // "default" | blue | teal | green | orange | red | pink | purple. Stored
    // as-is: Theme::make() normalizes an unknown accent to "default", and
    // Theme::accents() is the single source of the list.
    QString themeAccent() const;           // default "default"
    void setThemeAccent(const QString &accent);
    static QString defaultThemeAccent();   // "default"

    // -- UI scale (a direct multiplier on fonts + explicit widget metrics) --
    int uiScale() const;                   // default 100 (%)
    void setUiScale(int percent);          // clamped to [min,max]
    static int minUiScale();               // 50
    static int maxUiScale();               // 300
    static int defaultUiScale();           // 100

    // -- preview visibility ------------------------------------------------
    bool previewVisible() const;           // default true
    void setPreviewVisible(bool visible);

    // -- splitter ratio (0.0..1.0, fraction the editor takes) --------------
    double splitterRatio() const;          // default 0.5
    void setSplitterRatio(double ratio);

    // -- recent files (most-recent-first, capped) --------------------------
    QStringList recentFiles() const;       // default empty
    // Replace the list, dropping empty/duplicate entries and trimming to the
    // cap.
    void setRecentFiles(const QStringList &files);
    // Prepend `path` (moving an existing entry to the front), drop duplicates,
    // and trim to the cap. Ignored when empty.
    void addRecentFile(const QString &path);

    int recentCap() const;                 // default 5
    void setRecentCap(int cap);
    static int defaultRecentCap();         // 5

    // Flush any pending writes to storage (QSettings batches writes).
    void sync();

private:
    static ThemeMode themeFromKey(const QString &key);
    static QString themeToKey(ThemeMode mode);
    static bool ratioValid(double ratio);

    QSettings *m_owned;   // the real settings, created when no backing given
    QSettings *m_borrowed; // an externally supplied backing (never deleted)
    QSettings *m_s;       // the one actually used
};
