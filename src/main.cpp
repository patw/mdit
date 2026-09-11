// mdit — application entry point.
//
// Creates the QApplication, applies the (persisted) theme, constructs and shows
// the MainWindow (QSplitter editor|preview, menu bar + status bar), and honors
// an optional `argv[1]` markdown path.
//
// The theme mode (auto/light/dark) is persisted through Settings and applied by
// the MainWindow on construction.
#include "AppCli.h"
#include "AppIcons.h"
#include "AppInfo.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("mdit"));
    QCoreApplication::setApplicationName(AppInfo::name());
    QCoreApplication::setApplicationVersion(AppInfo::version()); // from CMake
    // The desktop-file basename: a Wayland compositor (GNOME / Ubuntu Dock) maps
    // the window to packaging/mdit.desktop through it, and THAT entry's Icon= is
    // what the dock/taskbar shows (a Wayland shell ignores window icons).
    QGuiApplication::setDesktopFileName(QStringLiteral("mdit"));

    // The application icon (the white-disc '#' mark, compiled in from the
    // resource) — set on the application for the taskbar/launcher as well as on
    // the window (MainWindow sets it too, so tests see it without main()).
    QApplication::setWindowIcon(AppIcons::windowIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("mdit — a fast, pure-Qt6 specialty markdown editor."));
    parser.addHelpOption();
    QCommandLineOption versionOpt(
        QStringLiteral("version"), QStringLiteral("Print the mdit version and exit."));
    parser.addOption(versionOpt);
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("Optional .md / .markdown file to open."));
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        std::printf("%s %s\n", qPrintable(AppInfo::name()),
                    qPrintable(AppInfo::version()));
        return 0;
    }

    // The MainWindow resolves and applies the persisted theme (auto/light/dark,
    // via Settings) on construction — no hardcoded pre-pass needed here.
    MainWindow window;

    // Single-file CLI open (subtask 10.1): open the first positional argument
    // that is an existing .md/.markdown file; a missing / non-markdown / absent
    // argument leaves the window in the empty untitled state (AppCli::fileToOpen
    // returns empty, so no open happens). The decision is a pure function so it
    // is testable without running this main() / app.exec().
    const QString openPath = AppCli::fileToOpen(parser.positionalArguments());
    if (!openPath.isEmpty())
        window.openFile(openPath);

    window.show();
    return app.exec();
}
