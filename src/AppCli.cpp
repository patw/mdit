#include "AppCli.h"

#include <QFileInfo>

namespace AppCli
{

QString fileToOpen(const QStringList &positionalArgs)
{
    // Single-file focus: the first positional argument that is an existing
    // regular file with a markdown suffix is the one to open. mdit loads
    // only `.md` / `.markdown` (mirrors the drag-and-drop filter), so any other
    // existing file — like a missing path or a directory — is a "bad arg" that
    // leaves the app in the empty untitled state.
    for (const QString &arg : positionalArgs) {
        const QFileInfo info(arg);
        if (!info.exists() || !info.isFile())
            continue;
        const QString suffix = info.suffix().toLower();
        if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown"))
            return info.absoluteFilePath();
    }
    return QString();
}

} // namespace AppCli
