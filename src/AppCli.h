// AppCli — mdit's command-line entry-point logic (pure, GUI-free).
//
// The application's single-file CLI open — "`mdit <file>.md` opens the file
// if the path exists, else the app starts in the empty untitled state" — is
// isolated here as a pure function so it can be exercised without launching the
// GUI or running the real `main()` (which blocks in `app.exec()`). `main.cpp`
// calls `fileToOpen()` on the parser's positional arguments and, when the
// result is non-empty, hands that path to `MainWindow::openFile()`.
#pragma once

#include <QString>
#include <QStringList>

namespace AppCli
{

// Decide which of the given command-line positional arguments, if any, is the
// markdown file to open at startup. Returns the FIRST argument that is an
// existing regular file with a `.md` / `.markdown` suffix (case-insensitive),
// as an absolute path. Returns an empty `QString` otherwise — for a missing
// path, a path that is not a regular file, a non-markdown file, or no arguments
// at all — in which case the caller leaves the app in the empty untitled state.
//
// mdit loads only markdown files (spec: "loads only markdown files"), so a
// non-markdown existing file is a "bad arg" and is ignored — mirroring the
// drag-and-drop filter in MainWindow::handleDroppedPaths().
QString fileToOpen(const QStringList &positionalArgs);

} // namespace AppCli
