#include "AppIcons.h"

#include <QString>

// Static-library gotcha: rcc turns resources/mdit.qrc into an object file inside
// the mdit_core archive, and NOTHING references its registration function — so a
// linker that only pulls in the objects a binary actually uses can drop it and
// `:/icons/mdit-*.png` silently stops resolving. Q_INIT_RESOURCE() both
// registers the resource explicitly and (via the extern reference) pins the
// generated object file into every binary that links mdit_core. The macro cannot
// live inside a namespace, hence this file-scope helper.
static void ensureIconResource()
{
    Q_INIT_RESOURCE(mdit);
}

namespace {

// Every size tools/make_icon.py renders and resources/mdit.qrc embeds. Each is
// drawn natively (not scaled), so the small sizes stay crisp.
const int kSizes[] = {16, 24, 32, 48, 64, 128, 256, 512};

} // namespace

namespace AppIcons {

QString iconPath(int size)
{
    return QStringLiteral(":/icons/mdit-%1.png").arg(size);
}

QIcon windowIcon()
{
    ensureIconResource();
    QIcon icon;
    for (const int size : kSizes)
        icon.addFile(iconPath(size));
    return icon;
}

} // namespace AppIcons
