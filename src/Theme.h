// Theme — mdit's token-based theming system.
//
// Adapted from PengyCPP's `themehelper.h` (the same idea: a named-token palette
// generated from a **mode** × **accent**, plus a direct **UI-scale** multiplier):
//
//   * `Theme::make(dark, accent)` builds a complete palette as a map of named
//     colour tokens ("bg", "fg", "primary", "doc_link", …). There are **three
//     modes** (System / Light / Dark — System resolves from the platform's
//     colour scheme) and **eight accents** (Default + Blue/Teal/Green/Orange/
//     Red/Pink/Purple) that tint the surfaces and set the highlight colour, so
//     you get 16 palettes, not just light/dark.
//   * Everything the app draws comes from those tokens: the `QPalette`
//     (`buildPalette`), the application QSS (`appStyleSheet`), the **preview /
//     export stylesheet** (`RenderedDocument` reads `doc_*` tokens) and the
//     editor's markdown highlighter (accent-tinted by `MarkdownHighlighter`).
//   * **UI scaling** is a separate, direct multiplier (`scaleFactor`/`scaled`/
//     `scaledFont`) applied to the application font and the explicit widget
//     metrics in the stylesheet — exactly PengyCPP's model: Qt still owns OS/DPI
//     scaling, this is an extra "make everything bigger" knob (50…300%).
//
// The **Default** accent keeps mdit's original neutral colours, so the app looks
// identical to before unless you pick an accent.
#pragma once

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QList>
#include <QMap>
#include <QPalette>
#include <QString>
#include <QStringList>

#include "Settings.h"

class Theme
{
public:
    // -- the palette ---------------------------------------------------------
    // Named colour tokens (all "#rrggbb"). Use `tokenKeys()` in tests to assert
    // every palette is complete.
    QMap<QString, QString> tokens;
    bool dark = false;          // the resolved mode
    QString accent;             // "default" | "blue" | … (machine name)
    QString accentTitle;        // "Default" | "Blue" | … (for the UI)

    // Token lookup ("#rrggbb", empty for an unknown key).
    QString operator[](const QString &key) const { return tokens.value(key); }
    QColor color(const QString &key) const;

    // -- construction --------------------------------------------------------
    // Pure: no QApplication needed (used by RenderedDocument and the tests).
    // An unknown accent falls back to "default".
    static Theme make(bool dark, const QString &accent = QStringLiteral("default"));
    // Mode-aware: System resolves through `systemDark`.
    static Theme make(Settings::ThemeMode mode, const QString &accent, bool systemDark);

    static QStringList tokenKeys();
    static QStringList accents();                 // default, blue, teal, … purple
    static QString accentTitleFor(const QString &accent); // "Default", "Blue", …
    static QString defaultAccent();               // "default"
    static bool isKnownAccent(const QString &accent);

    // -- mode resolution ----------------------------------------------------
    // Dark / Light are explicit, System (the old "auto") follows the platform.
    static bool resolveDark(Settings::ThemeMode mode, bool systemDark);
    static bool isSystemDark(QApplication &app);

    // -- UI scaling (a separate multiplier on top of Qt's DPI handling) ------
    static int defaultScale();                    // 100 (%)
    static int minScale();                        // 50
    static int maxScale();                        // 300
    static QList<int> offeredScales();            // 75, 100, 110, 125, 150, 175, 200
    static int clampScale(int percent);
    static double scaleFactor(int percent);       // clamped percent / 100
    static int scaled(int px, int percent);       // a scaled pixel metric
    static double scaledFont(double points, int percent);
    // The platform's general/monospace font with the scale applied.
    static QFont scaledSystemFont(int percent);
    static QFont scaledMonoFont(int percent);

    // -- applying -----------------------------------------------------------
    // A Fusion-style QPalette built from the tokens.
    static QPalette buildPalette(const Theme &theme);
    // The application QSS (chrome + widgets), with the accents and the scaled
    // metrics baked in.
    static QString appStyleSheet(const Theme &theme, int scalePercent);
    // The whole application: Fusion + palette + scaled application font + QSS.
    static void apply(QApplication &app, const Theme &theme, int scalePercent);

    // -- helpers used while building palettes -------------------------------
    // Blend `a` toward `b` by `t` (0..1) — the accent-tinting primitive.
    static QColor blend(const QColor &a, const QColor &b, double t);
    // "#1e1e1e" or "#ffffff", whichever reads on `background`.
    static QColor contrastingText(const QColor &background);
    static bool isDarkColor(const QColor &color);
};
