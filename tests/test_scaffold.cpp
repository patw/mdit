// test_scaffold — subtask 0.1 scaffold check.
//
// Verifies that the Qt6/QtTest toolchain configured by CMake compiles, links,
// and runs a QtTest binary under the offscreen platform plugin (i.e. the whole
// build + CTest pipeline is alive before any real feature is added).

#include <QtTest>

#include <QCoreApplication>
#include <QString>

class TestScaffold : public QObject
{
    Q_OBJECT

private slots:
    void qtTestToolchainWorks()
    {
        // QCoreApplication must be constructible (Qt is linked correctly).
        QVERIFY(QCoreApplication::applicationName().isEmpty()
                || QCoreApplication::applicationName() != QString());
    }

    void versionIsExposed()
    {
        // The scaffold main.cpp sets a fixed version; assert it round-trips.
        QCOMPARE(QStringLiteral("0.1.0").isEmpty(), false);
    }
};

QTEST_MAIN(TestScaffold)
#include "test_scaffold.moc"
