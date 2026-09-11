#include "Theme.h"

#include <QFontDatabase>
#include <QPalette>
#include <QStyleHints>

namespace {

// The accent colour table (the same eight as PengyCPP). "default" is mdit's
// original blue, so the neutral palette is byte-for-byte what it always was.
struct AccentDef {
    const char *name;
    const char *primary;
};
const AccentDef kAccents[] = {
    {"default", "#2a82da"},
    {"blue", "#1e66f5"},
    {"teal", "#179299"},
    {"green", "#40a02b"},
    {"orange", "#df8e1d"},
    {"red", "#d20f39"},
    {"pink", "#ea76cb"},
    {"purple", "#8839ef"},
};

QString titleCase(const QString &word)
{
    if (word.isEmpty())
        return word;
    QString out = word;
    out[0] = out.at(0).toUpper();
    return out;
}

} // namespace

QColor Theme::color(const QString &key) const
{
    return QColor(tokens.value(key));
}

QStringList Theme::tokenKeys()
{
    return {
        // meta
        QStringLiteral("mode"), QStringLiteral("accent"), QStringLiteral("accent_title"),
        // window chrome + text
        QStringLiteral("bg"), QStringLiteral("fg"), QStringLiteral("base"),
        QStringLiteral("alt_base"), QStringLiteral("button"), QStringLiteral("button_text"),
        QStringLiteral("disabled_fg"),
        // chrome surfaces
        QStringLiteral("chrome_bg"), QStringLiteral("menu_bg"), QStringLiteral("panel"),
        QStringLiteral("panel_2"), QStringLiteral("border"), QStringLiteral("border_soft"),
        QStringLiteral("muted"), QStringLiteral("hover"),
        QStringLiteral("input_bg"), QStringLiteral("input_fg"),
        // interaction
        QStringLiteral("selection"), QStringLiteral("selection_fg"),
        QStringLiteral("primary"), QStringLiteral("primary_hover"),
        QStringLiteral("primary_fg"), QStringLiteral("link"),
        QStringLiteral("tooltip_bg"), QStringLiteral("tooltip_fg"),
        // the rendered preview / exported document (RenderedDocument)
        QStringLiteral("doc_bg"), QStringLiteral("doc_fg"), QStringLiteral("doc_h1"),
        QStringLiteral("doc_h2"), QStringLiteral("doc_h3"), QStringLiteral("doc_h4"),
        QStringLiteral("doc_code_bg"), QStringLiteral("doc_code_fg"),
        QStringLiteral("doc_table_border"), QStringLiteral("doc_table_head_bg"),
        QStringLiteral("doc_quote_fg"), QStringLiteral("doc_hr"), QStringLiteral("doc_link"),
    };
}

QStringList Theme::accents()
{
    QStringList out;
    for (const AccentDef &a : kAccents)
        out << QString::fromLatin1(a.name);
    return out;
}

QString Theme::accentTitleFor(const QString &accent)
{
    return titleCase(isKnownAccent(accent) ? accent : defaultAccent());
}

QString Theme::defaultAccent()
{
    return QStringLiteral("default");
}

bool Theme::isKnownAccent(const QString &accent)
{
    for (const AccentDef &a : kAccents) {
        if (accent == QString::fromLatin1(a.name))
            return true;
    }
    return false;
}

QColor Theme::blend(const QColor &a, const QColor &b, double t)
{
    const double u = qBound(0.0, t, 1.0);
    return QColor(qRound(a.red() + (b.red() - a.red()) * u),
                  qRound(a.green() + (b.green() - a.green()) * u),
                  qRound(a.blue() + (b.blue() - a.blue()) * u));
}

bool Theme::isDarkColor(const QColor &color)
{
    // Rec. 601 luma — the same test QStyleHints-level code uses.
    return (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) < 128.0;
}

QColor Theme::contrastingText(const QColor &background)
{
    return isDarkColor(background) ? QColor(0xff, 0xff, 0xff) : QColor(0x1e, 0x1e, 0x1e);
}

Theme Theme::make(bool dark, const QString &accentIn)
{
    const QString accent = isKnownAccent(accentIn) ? accentIn : defaultAccent();
    QColor primary;
    for (const AccentDef &a : kAccents) {
        if (accent == QString::fromLatin1(a.name)) {
            primary = QColor(QString::fromLatin1(a.primary));
            break;
        }
    }

    Theme t;
    t.dark = dark;
    t.accent = accent;
    t.accentTitle = accentTitleFor(accent);
    QMap<QString, QString> &c = t.tokens;

    auto put = [&c](const char *key, const QColor &col) {
        c.insert(QString::fromLatin1(key), col.name(QColor::HexRgb));
    };

    if (!dark) {
        // --- light -----------------------------------------------------------
        // The Default accent reproduces mdit's original light colours exactly;
        // the others tint the surfaces toward the accent.
        const bool plain = (accent == defaultAccent());
        const QColor bg = plain ? QColor(0xf0, 0xf0, 0xf0) : blend(QColor(0xf0, 0xf0, 0xf0), primary, 0.07);
        const QColor base = plain ? QColor(0xff, 0xff, 0xff) : blend(QColor(0xff, 0xff, 0xff), primary, 0.035);
        const QColor chrome = plain ? QColor(0xf0, 0xf0, 0xf0) : blend(QColor(0xf0, 0xf0, 0xf0), primary, 0.09);
        const QColor menu = plain ? QColor(0xff, 0xff, 0xff) : blend(QColor(0xff, 0xff, 0xff), primary, 0.05);
        const QColor button = plain ? QColor(0xe0, 0xe0, 0xe0) : blend(QColor(0xe0, 0xe0, 0xe0), primary, 0.12);
        const QColor border = plain ? QColor(0xb0, 0xb0, 0xb0) : blend(QColor(0xb0, 0xb0, 0xb0), primary, 0.30);
        const QColor borderSoft = plain ? QColor(0xcc, 0xcc, 0xcc) : blend(QColor(0xcc, 0xcc, 0xcc), primary, 0.22);
        const QColor muted = plain ? QColor(0x6b, 0x72, 0x80) : blend(QColor(0x6b, 0x72, 0x80), primary, 0.18);
        const QColor hover = plain ? QColor(0xd5, 0xd5, 0xd5) : blend(QColor(0xd5, 0xd5, 0xd5), primary, 0.18);
        const QColor inputBg = plain ? QColor(0xff, 0xff, 0xff) : blend(QColor(0xff, 0xff, 0xff), primary, 0.03);
        const QColor selection = primary;
        const QColor selectionFg = contrastingText(primary);
        const QColor fg = QColor(0x1e, 0x1e, 0x1e);

        put("mode", QColor(0xff, 0xff, 0xff)); // overwritten by the string below
        c["mode"] = QStringLiteral("light");
        put("bg", bg);
        put("fg", fg);
        put("base", base);
        put("alt_base", plain ? QColor(0xf5, 0xf5, 0xf5) : blend(QColor(0xf5, 0xf5, 0xf5), primary, 0.06));
        put("button", button);
        put("button_text", fg);
        put("disabled_fg", QColor(0x96, 0x96, 0x96));
        put("chrome_bg", chrome);
        put("menu_bg", menu);
        put("panel", plain ? QColor(0xe8, 0xe8, 0xe8) : blend(QColor(0xe8, 0xe8, 0xe8), primary, 0.10));
        put("panel_2", button);
        put("border", border);
        put("border_soft", borderSoft);
        put("muted", muted);
        put("hover", hover);
        put("input_bg", inputBg);
        put("input_fg", fg);
        put("selection", selection);
        put("selection_fg", selectionFg);
        put("primary", primary);
        put("primary_hover", primary.darker(112));
        put("primary_fg", contrastingText(primary));
        put("link", primary);
        put("tooltip_bg", QColor(0xff, 0xff, 0xdc));
        put("tooltip_fg", fg);

        // The rendered document (preview + HTML/PDF export).
        put("doc_bg", plain ? QColor(0xff, 0xff, 0xff) : blend(QColor(0xff, 0xff, 0xff), primary, 0.035));
        put("doc_fg", plain ? QColor(0x1e, 0x1e, 0x1e) : blend(QColor(0x1e, 0x1e, 0x1e), primary, 0.20));
        put("doc_h1", plain ? QColor(0x1a, 0x1a, 0x1a) : blend(QColor(0x1a, 0x1a, 0x1a), primary, 0.60));
        put("doc_h2", plain ? QColor(0x1a, 0x1a, 0x1a) : blend(QColor(0x1a, 0x1a, 0x1a), primary, 0.45));
        put("doc_h3", plain ? QColor(0x22, 0x22, 0x22) : blend(QColor(0x22, 0x22, 0x22), primary, 0.28));
        put("doc_h4", plain ? QColor(0x33, 0x33, 0x33) : blend(QColor(0x33, 0x33, 0x33), primary, 0.14));
        put("doc_code_bg", plain ? QColor(0xf0, 0xf0, 0xf0) : blend(QColor(0xf0, 0xf0, 0xf0), primary, 0.10));
        put("doc_code_fg", QColor(0x22, 0x22, 0x22));
        put("doc_table_border", plain ? QColor(0xcc, 0xcc, 0xcc) : borderSoft);
        put("doc_table_head_bg", plain ? QColor(0xee, 0xee, 0xee) : blend(QColor(0xee, 0xee, 0xee), primary, 0.12));
        put("doc_quote_fg", plain ? QColor(0x55, 0x55, 0x55) : muted);
        put("doc_hr", plain ? QColor(0xcc, 0xcc, 0xcc) : borderSoft);
        put("doc_link", primary);
    } else {
        // --- dark ------------------------------------------------------------
        const bool plain = (accent == defaultAccent());
        const QColor bg = plain ? QColor(0x1e, 0x1e, 0x1e) : blend(QColor(0x1e, 0x1e, 0x1e), primary, 0.12);
        const QColor base = plain ? QColor(0x18, 0x18, 0x18) : blend(QColor(0x18, 0x18, 0x18), primary, 0.10);
        const QColor chrome = plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.14);
        const QColor menu = plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.14);
        const QColor button = plain ? QColor(0x3a, 0x3a, 0x3a) : blend(QColor(0x3a, 0x3a, 0x3a), primary, 0.14);
        const QColor border = plain ? QColor(0x55, 0x55, 0x55) : blend(QColor(0x55, 0x55, 0x55), primary, 0.28);
        const QColor borderSoft = plain ? QColor(0x44, 0x44, 0x44) : blend(QColor(0x44, 0x44, 0x44), primary, 0.22);
        const QColor muted = plain ? QColor(0xb0, 0xb0, 0xb0) : blend(QColor(0xb0, 0xb0, 0xb0), primary, 0.16);
        const QColor hover = plain ? QColor(0x45, 0x45, 0x45) : blend(QColor(0x45, 0x45, 0x45), primary, 0.20);
        const QColor fg = QColor(0xdc, 0xdc, 0xdc);
        const QColor link = plain ? QColor(0x6c, 0xb2, 0xf0) : primary.lighter(118);
        const QColor selection = plain ? QColor(0x2a, 0x82, 0xda) : blend(bg, primary, 0.45);

        c["mode"] = QStringLiteral("dark");
        put("bg", bg);
        put("fg", fg);
        put("base", base);
        put("alt_base", plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.12));
        put("button", button);
        put("button_text", fg);
        put("disabled_fg", QColor(0x6e, 0x6e, 0x6e));
        put("chrome_bg", chrome);
        put("menu_bg", menu);
        put("panel", plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.12));
        put("panel_2", button);
        put("border", border);
        put("border_soft", borderSoft);
        put("muted", muted);
        put("hover", hover);
        put("input_bg", plain ? QColor(0x18, 0x18, 0x18) : blend(QColor(0x18, 0x18, 0x18), primary, 0.08));
        put("input_fg", fg);
        put("selection", selection);
        put("selection_fg", QColor(0xff, 0xff, 0xff));
        // The accent IS the primary colour in both modes (PengyCPP parity);
        // only the hover tone shifts for the dark background.
        put("primary", primary);
        put("primary_hover", primary.lighter(115));
        put("primary_fg", contrastingText(primary));
        put("link", link);
        put("tooltip_bg", plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.16));
        put("tooltip_fg", fg);

        put("doc_bg", plain ? QColor(0x1e, 0x1e, 0x1e) : blend(QColor(0x1e, 0x1e, 0x1e), primary, 0.12));
        put("doc_fg", plain ? QColor(0xdc, 0xdc, 0xdc) : blend(QColor(0xdc, 0xdc, 0xdc), primary, 0.12));
        put("doc_h1", plain ? QColor(0xf0, 0xf0, 0xf0) : blend(QColor(0xf0, 0xf0, 0xf0), primary, 0.55));
        put("doc_h2", plain ? QColor(0xf0, 0xf0, 0xf0) : blend(QColor(0xf0, 0xf0, 0xf0), primary, 0.40));
        put("doc_h3", plain ? QColor(0xe8, 0xe8, 0xe8) : blend(QColor(0xe8, 0xe8, 0xe8), primary, 0.25));
        put("doc_h4", plain ? QColor(0xe0, 0xe0, 0xe0) : blend(QColor(0xe0, 0xe0, 0xe0), primary, 0.12));
        put("doc_code_bg", plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.14));
        put("doc_code_fg", plain ? QColor(0xd8, 0xd8, 0xd8) : blend(QColor(0xd8, 0xd8, 0xd8), primary, 0.10));
        put("doc_table_border", plain ? QColor(0x44, 0x44, 0x44) : borderSoft);
        put("doc_table_head_bg", plain ? QColor(0x2a, 0x2a, 0x2a) : blend(QColor(0x2a, 0x2a, 0x2a), primary, 0.16));
        put("doc_quote_fg", plain ? QColor(0xb0, 0xb0, 0xb0) : muted);
        put("doc_hr", plain ? QColor(0x44, 0x44, 0x44) : borderSoft);
        put("doc_link", link);
    }

    c["accent"] = accent;
    c["accent_title"] = t.accentTitle;
    return t;
}

Theme Theme::make(Settings::ThemeMode mode, const QString &accent, bool systemDark)
{
    return make(resolveDark(mode, systemDark), accent);
}

bool Theme::resolveDark(Settings::ThemeMode mode, bool systemDark)
{
    switch (mode) {
    case Settings::ThemeMode::Dark:
        return true;
    case Settings::ThemeMode::Light:
        return false;
    case Settings::ThemeMode::Auto:
    default:
        return systemDark;
    }
}

bool Theme::isSystemDark(QApplication &app)
{
    // Qt 6.5+ exposes the platform's colour scheme through QStyleHints. Older Qt
    // (Ubuntu 24.04 still ships 6.4.2) has neither colorScheme() nor
    // Qt::ColorScheme, so fall back to the palette's window colour — the same
    // luma test the accent code uses. Guarded with QT_VERSION_CHECK so both
    // build cleanly.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return app.styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    Q_UNUSED(app);
    return qApp ? isDarkColor(qApp->palette().color(QPalette::Window)) : false;
#endif
}

// -- UI scaling ---------------------------------------------------------------

int Theme::defaultScale()
{
    return 100;
}

int Theme::minScale()
{
    return 50;
}

int Theme::maxScale()
{
    return 300;
}

QList<int> Theme::offeredScales()
{
    // PengyCPP's list — fine-grained around 100%, coarser as it grows.
    return {75, 100, 110, 125, 150, 175, 200};
}

int Theme::clampScale(int percent)
{
    return qBound(minScale(), percent, maxScale());
}

double Theme::scaleFactor(int percent)
{
    return clampScale(percent) / 100.0;
}

int Theme::scaled(int px, int percent)
{
    return qMax(1, qRound(px * scaleFactor(percent)));
}

double Theme::scaledFont(double points, int percent)
{
    return qMax(1.0, points * scaleFactor(percent));
}

QFont Theme::scaledSystemFont(int percent)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    const double base = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
    font.setPointSizeF(scaledFont(base, percent));
    return font;
}

QFont Theme::scaledMonoFont(int percent)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const double base = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
    font.setPointSizeF(scaledFont(base, percent));
    return font;
}

// -- palette + stylesheet -----------------------------------------------------

QPalette Theme::buildPalette(const Theme &theme)
{
    const QString mode = theme["mode"];
    QPalette pal;
    pal.setColor(QPalette::Window, theme.color(QStringLiteral("bg")));
    pal.setColor(QPalette::WindowText, theme.color(QStringLiteral("fg")));
    pal.setColor(QPalette::Base, theme.color(QStringLiteral("base")));
    pal.setColor(QPalette::AlternateBase, theme.color(QStringLiteral("alt_base")));
    pal.setColor(QPalette::Text, theme.color(QStringLiteral("input_fg")));
    pal.setColor(QPalette::Button, theme.color(QStringLiteral("button")));
    pal.setColor(QPalette::ButtonText, theme.color(QStringLiteral("button_text")));
    pal.setColor(QPalette::Highlight, theme.color(QStringLiteral("selection")));
    pal.setColor(QPalette::HighlightedText, theme.color(QStringLiteral("selection_fg")));
    pal.setColor(QPalette::Link, theme.color(QStringLiteral("link")));
    pal.setColor(QPalette::ToolTipBase, theme.color(QStringLiteral("tooltip_bg")));
    pal.setColor(QPalette::ToolTipText, theme.color(QStringLiteral("tooltip_fg")));
    pal.setColor(QPalette::PlaceholderText, theme.color(QStringLiteral("muted")));
    pal.setColor(QPalette::Disabled, QPalette::Text, theme.color(QStringLiteral("disabled_fg")));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, theme.color(QStringLiteral("disabled_fg")));
    Q_UNUSED(mode);
    return pal;
}

QString Theme::appStyleSheet(const Theme &theme, int scalePercent)
{
    // Metrics scale with the UI scale; colours come from the tokens. Tokens are
    // substituted by name (`@{bg}`) so the sheet stays readable and cannot
    // suffer positional-argument drift when a token is added.
    QString sheet = QStringLiteral(R"QSS(
QMainWindow, QWidget { background-color: @{bg}; color: @{fg}; }
QPlainTextEdit { background-color: @{base}; color: @{input_fg}; }
QTextBrowser { background-color: @{doc_bg}; color: @{doc_fg}; }
QSplitter::handle { background-color: @{border_soft}; }
QMenuBar { background-color: @{chrome_bg}; color: @{fg}; padding: 1px; }
QMenuBar::item { padding: @itemPadVpx @itemPadHpx; border-radius: @radiuspx; }
QMenuBar::item:selected { background-color: @{selection}; color: @{selection_fg}; }
QMenu { background-color: @{menu_bg}; color: @{fg}; border: 1px solid @{border_soft}; }
QMenu::item { padding: @itemPadVpx @itemPadHpx; }
QMenu::item:selected { background-color: @{selection}; color: @{selection_fg}; }
QMenu::separator { background-color: @{border_soft}; height: 1px; margin: 3px 6px; }
QStatusBar { background-color: @{chrome_bg}; color: @{fg}; }
QToolTip { background-color: @{tooltip_bg}; color: @{tooltip_fg}; border: 1px solid @{border_soft}; }
QPushButton { background-color: @{button}; color: @{fg}; border: 1px solid @{border};
              border-radius: @radiuspx; padding: @padVpx @padHpx; }
QPushButton:hover { background-color: @{hover}; border-color: @{selection}; }
QPushButton:pressed { background-color: @{selection}; color: @{selection_fg}; }
QPushButton:disabled { color: @{disabled_fg}; background-color: @{bg}; border-color: @{border_soft}; }
QDialogButtonBox QPushButton { min-width: @buttonMinpx; }
QLineEdit, QTextEdit, QComboBox, QSpinBox { background-color: @{base}; color: @{input_fg};
              border: 1px solid @{border}; border-radius: @radiuspx; padding: @padVpx @inputPadHpx;
              selection-background-color: @{selection}; selection-color: @{selection_fg}; }
QLineEdit:focus, QTextEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: @{primary}; }
QCheckBox, QRadioButton, QLabel { color: @{fg}; }
QLabel#FindCount { color: @{muted}; }
QScrollBar { background-color: @{bg}; }
QDialog { background-color: @{bg}; color: @{fg}; }
QToolButton { color: @{fg}; }
QToolButton:hover, QToolButton:checked { background-color: @{hover}; border-radius: @radiuspx; }
)QSS");

    // Colours first (the `@{}` delimiters keep token names from colliding).
    for (auto it = theme.tokens.constBegin(); it != theme.tokens.constEnd(); ++it)
        sheet.replace(QStringLiteral("@{%1}").arg(it.key()), it.value());

    // ...then the scaled metrics.
    sheet.replace(QStringLiteral("@padV"), QString::number(scaled(3, scalePercent)));
    sheet.replace(QStringLiteral("@padH"), QString::number(scaled(8, scalePercent)));
    sheet.replace(QStringLiteral("@inputPadH"), QString::number(scaled(6, scalePercent)));
    sheet.replace(QStringLiteral("@itemPadV"), QString::number(scaled(4, scalePercent)));
    sheet.replace(QStringLiteral("@itemPadH"), QString::number(scaled(9, scalePercent)));
    sheet.replace(QStringLiteral("@radius"), QString::number(scaled(4, scalePercent)));
    sheet.replace(QStringLiteral("@buttonMin"), QString::number(scaled(72, scalePercent)));
    return sheet;
}

void Theme::apply(QApplication &app, const Theme &theme, int scalePercent)
{
    app.setStyle(QStringLiteral("Fusion"));
    app.setPalette(buildPalette(theme));
    // The UI scale rides on the application font (widgets that set their own
    // font — the editor's monospace face — are scaled by their owners).
    app.setFont(scaledSystemFont(scalePercent));
    app.setStyleSheet(appStyleSheet(theme, scalePercent));
}
