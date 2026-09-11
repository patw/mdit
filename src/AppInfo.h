// AppInfo — the single source of mdit's identity strings.
//
// Used by main(), the About dialog and the tests, so the About box, the window
// metadata and the CLI can never disagree about the name/version/tagline.
//
// The **version comes from CMake** (`project(mdit VERSION ...)`) through the
// MDIT_VERSION compile definition that the build adds to mdit_core — there is no
// second literal to keep in sync.
#pragma once

#include <QString>

namespace AppInfo
{
// "mdit" — the application / binary name.
QString name();
// The CMake project version, e.g. "0.1.0".
QString version();
// "The ultimate lightweight Markdown tool" — the one-line pitch.
QString tagline();
// "© 2026 Pat Wendorf — MIT License".
QString copyrightLine();
// "mdit is a Catbee project" — the project line (the URL lives in projectUrl()).
QString projectLine();
// "https://catbee.ca".
QString projectUrl();
} // namespace AppInfo
