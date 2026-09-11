#include "AppInfo.h"

#include <QCoreApplication>

#ifndef MDIT_VERSION
// Fallback for a build that did not get the CMake compile definition (the real
// value is `project(mdit VERSION ...)` in CMakeLists.txt).
#define MDIT_VERSION "0.1.0"
#endif

namespace {

QString tr_(const char *text)
{
    return QCoreApplication::translate("AppInfo", text);
}

} // namespace

namespace AppInfo {

QString name()
{
    return QStringLiteral("mdit");
}

QString version()
{
    return QStringLiteral(MDIT_VERSION);
}

QString tagline()
{
    return tr_("The ultimate lightweight Markdown tool");
}

QString copyrightLine()
{
    // One line: the copyright, the year and the license (MIT).
    return tr_("\u00A9 2026 Pat Wendorf \u2014 MIT License");
}

QString projectLine()
{
    return tr_("mdit is a Catbee project");
}

QString projectUrl()
{
    return QStringLiteral("https://catbee.ca");
}

} // namespace AppInfo
