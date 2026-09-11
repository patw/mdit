// test_icon — the application icon (post-loop polish).
//
// mdit's mark mirrors the Pengy icon (~/Personal/Pengy/pengy.png): a **solid
// white disc with a fully transparent outside**, carrying a bold **`#`** (the
// markdown ATX heading element) instead of the penguin. The PNGs are generated
// by tools/make_icon.py and compiled into the binary through resources/mdit.qrc.
//
// This suite pins the visual contract so the artwork cannot silently rot:
//   * AppIcons::windowIcon() is a multi-size icon (16 … 512) and every size
//     really resolves out of the compiled-in resource;
//   * the pixels are what the design says — transparent corners, an opaque white
//     disc (opaque at 0.45*size from the centre on every axis, transparent
//     beyond 0.52*size), and a DARK '#' inside whose strokes are centred;
//   * the '#' is structurally a hash: a scanline through the middle row crosses
//     exactly TWO dark bars (the verticals) and one down the middle column
//     crosses exactly TWO (the horizontals);
//   * the window carries the icon too (so a taskbar/launcher shows it) and it is
//     the same artwork as the application icon.
#include "testmain.h"
#include "AppIcons.h"
#include "MainWindow.h"

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QIcon>
#include <QPixmap>
#include <QSize>

namespace {

constexpr int kMainSize = 256; // the size whose pixels are examined

bool isDark(QRgb p)
{
    return qAlpha(p) > 200 && qMax(qMax(qRed(p), qGreen(p)), qBlue(p)) < 90;
}

bool isWhite(QRgb p)
{
    return qAlpha(p) > 200 && qRed(p) >= 250 && qGreen(p) >= 250 && qBlue(p) >= 250;
}

QImage loadIcon(int size)
{
    QImage img(AppIcons::iconPath(size));
    return img.convertToFormat(QImage::Format_ARGB32);
}

// Runs of dark pixels along one scanline (a run = a contiguous dark span at
// least `minRun` px long, so antialiased fringes do not count).
int darkRuns(const QImage &img, int index, bool column, int minRun)
{
    const int n = column ? img.height() : img.width();
    int runs = 0;
    int run = 0;
    for (int i = 0; i < n; ++i) {
        const QRgb p = column ? img.pixel(index, i) : img.pixel(i, index);
        if (isDark(p)) {
            ++run;
        } else {
            if (run >= minRun)
                ++runs;
            run = 0;
        }
    }
    if (run >= minRun)
        ++runs;
    return runs;
}

} // namespace

class TestIcon : public QObject
{
    Q_OBJECT
private slots:
    void windowIconIsMultiSize();
    void resourceSizesAllResolve();
    void discIsWhiteAndRoundWithATransparentOutside();
    void centreCarriesACentredHashMark();
    void tinyIconIsStillAMark();
    void windowCarriesTheIcon();
};

// One QIcon carrying every generated size, so Qt can pick the right one.
void TestIcon::windowIconIsMultiSize()
{
    const QIcon icon = AppIcons::windowIcon();
    QVERIFY2(!icon.isNull(), "AppIcons::windowIcon() must not be null");

    const QList<QSize> sizes = icon.availableSizes();
    QVERIFY(!sizes.isEmpty());
    for (const int expected : {16, 24, 32, 48, 64, 128, 256, 512})
        QVERIFY2(sizes.contains(QSize(expected, expected)),
                 qPrintable(QStringLiteral("icon is missing the %1 px size").arg(expected)));
}

// Every size is a real, decodable member of the compiled-in resource.
void TestIcon::resourceSizesAllResolve()
{
    for (const int size : {16, 24, 32, 48, 64, 128, 256, 512}) {
        const QString path = AppIcons::iconPath(size);
        QCOMPARE(path, QStringLiteral(":/icons/mdit-%1.png").arg(size));
        QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("missing resource %1").arg(path)));
        const QImage img = loadIcon(size);
        QVERIFY2(!img.isNull(), qPrintable(QStringLiteral("%1 did not decode").arg(path)));
        QCOMPARE(img.size(), QSize(size, size));
    }
}

// A solid white disc, transparent outside, on every axis.
void TestIcon::discIsWhiteAndRoundWithATransparentOutside()
{
    const QImage img = loadIcon(kMainSize);
    QVERIFY(!img.isNull());
    const double c = kMainSize / 2.0;

    // Transparent outside the circle: the four corners and a ring beyond the
    // disc edge (radius = 0.4872 * size) but still inside the canvas, so the
    // sample never leaves the image.
    for (const QPoint &corner : {QPoint(0, 0), QPoint(kMainSize - 1, 0),
                                 QPoint(0, kMainSize - 1), QPoint(kMainSize - 1, kMainSize - 1)}) {
        QCOMPARE(qAlpha(img.pixel(corner)), 0);
    }
    const double outside = 0.494 * kMainSize;
    for (int deg = 0; deg < 360; deg += 15) {
        const double a = deg * M_PI / 180.0;
        const int x = qBound(0, int(c + outside * std::cos(a)), kMainSize - 1);
        const int y = qBound(0, int(c + outside * std::sin(a)), kMainSize - 1);
        QVERIFY2(qAlpha(img.pixel(x, y)) == 0,
                 qPrintable(QStringLiteral("not transparent at (%1, %2)").arg(x).arg(y)));
    }

    // Opaque disc just inside the edge, on eight axes (white where the mark is
    // not, dark where a bar passes).
    for (int deg = 0; deg < 360; deg += 45) {
        const double a = deg * M_PI / 180.0;
        const int x = qBound(0, int(c + 0.45 * kMainSize * std::cos(a)), kMainSize - 1);
        const int y = qBound(0, int(c + 0.45 * kMainSize * std::sin(a)), kMainSize - 1);
        const QRgb p = img.pixel(x, y);
        QVERIFY2(qAlpha(p) > 200,
                 qPrintable(QStringLiteral("disc not opaque at (%1, %2)").arg(x).arg(y)));
        QVERIFY2(isWhite(p) || isDark(p), "the disc must be white (or carry the mark)");
    }

    // Plain white area between the mark and the disc edge.
    QVERIFY2(isWhite(img.pixel(int(c + 0.40 * kMainSize), int(c))),
             "the disc should be solid white away from the mark");

    // The white disc is (roughly) 97% of the canvas, mirroring the Pengy icon.
    int opaque = 0;
    for (int y = 0; y < kMainSize; ++y)
        for (int x = 0; x < kMainSize; ++x)
            if (qAlpha(img.pixel(x, y)) > 200)
                ++opaque;
    const double coverage = double(opaque) / (kMainSize * kMainSize);
    QVERIFY2(coverage > 0.70 && coverage < 0.78,
             qPrintable(QStringLiteral("disc coverage %1 is not the Pengy-mirroring ratio")
                            .arg(coverage)));
}

// Inside the disc: a dark '#', centred, with two vertical and two horizontal
// bars (beyond the crossing they are the only dark pixels on the centre lines).
void TestIcon::centreCarriesACentredHashMark()
{
    const QImage img = loadIcon(kMainSize);
    QVERIFY(!img.isNull());
    const int c = kMainSize / 2;
    const int minRun = kMainSize / 32; // ignore antialiased fringes

    // A horizontal scanline through the middle row meets the two VERTICAL bars
    // (the middle row sits between the two horizontal bars), and the middle
    // column meets the two HORIZONTAL bars.
    QCOMPARE(darkRuns(img, c, /*column=*/false, minRun), 2);
    QCOMPARE(darkRuns(img, c, /*column=*/true, minRun), 2);

    // The mark is centred: the dark pixels' centre of mass is the image centre.
    long sumX = 0;
    long sumY = 0;
    long dark = 0;
    for (int y = 0; y < kMainSize; ++y) {
        for (int x = 0; x < kMainSize; ++x) {
            if (isDark(img.pixel(x, y))) {
                sumX += x;
                sumY += y;
                ++dark;
            }
        }
    }
    QVERIFY2(dark > 0, "the mark is missing");
    const double meanX = double(sumX) / dark;
    const double meanY = double(sumY) / dark;
    QVERIFY2(qAbs(meanX - c) <= 3.0 && qAbs(meanY - c) <= 3.0,
             qPrintable(QStringLiteral("mark not centred: centre of mass (%1, %2)")
                            .arg(meanX)
                            .arg(meanY)));

    // It is a mark, not a blob: the dark pixels are a modest share of the disc
    // (the true weight is ~18%).
    const double share = double(dark) / (kMainSize * kMainSize);
    QVERIFY2(share > 0.08 && share < 0.35,
             qPrintable(QStringLiteral("mark coverage %1 is out of range").arg(share)));

    // And the bars do not touch: the middle of the mark (the crossing region's
    // centre) is white — i.e. there is white between the two verticals and
    // between the two horizontals.
    QVERIFY2(isWhite(img.pixel(c, c)), "the counters between the bars must stay white");
}

// The 16 px size (the pixel-snapped variant) is still the same mark.
void TestIcon::tinyIconIsStillAMark()
{
    const QImage img = loadIcon(16);
    QVERIFY(!img.isNull());
    QCOMPARE(qAlpha(img.pixel(0, 0)), 0); // still a disc on transparency

    int dark = 0;
    int white = 0;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const QRgb p = img.pixel(x, y);
            if (isDark(p))
                ++dark;
            else if (isWhite(p))
                ++white;
        }
    }
    QVERIFY2(dark >= 8, "the 16 px icon lost its mark");
    QVERIFY2(white >= 40, "the 16 px icon lost its white disc");
    // Two bars cross each centre line here too (2 px thick, 3 px apart).
    QCOMPARE(darkRuns(img, 8, /*column=*/false, 2), 2);
    QCOMPARE(darkRuns(img, 8, /*column=*/true, 2), 2);
}

// The window carries the icon as well (title bar / taskbar), with the same
// artwork as the application icon.
void TestIcon::windowCarriesTheIcon()
{
    MainWindow w;
    const QIcon windowIcon = w.windowIcon();
    QVERIFY2(!windowIcon.isNull(), "MainWindow must carry the app icon");
    QVERIFY(!windowIcon.availableSizes().isEmpty());

    const QImage fromWindow = windowIcon.pixmap(32, 32).toImage();
    const QImage fromApp = AppIcons::windowIcon().pixmap(32, 32).toImage();
    QVERIFY2(fromWindow == fromApp, "the window icon must be the app icon");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Never write to the real ~/.config/mdit during a test run.
    isolateUserSettings();
    TestIcon t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_icon.moc"
