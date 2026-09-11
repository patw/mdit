// test_github_ready — subtask 10.2: guard the GitHub-ready repository
// contract. Fails the suite if a required doc file is missing/empty or drifts
// from what the spec mandates, or if the CMakeLists stops declaring the
// project + testing. Pure file-reading: no QApplication needed
// (QTEST_GUILESS_MAIN), so it runs anywhere (offscreen env is harmless).
//
// The repository root is taken from the MDIT_REPO_ROOT compile definition
// (set by tests/CMakeLists.txt to CMAKE_SOURCE_DIR); the "../../" fallback
// covers a bare `ctest` run from the build/tests working directory.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#ifndef MDIT_REPO_ROOT
#define MDIT_REPO_ROOT QStringLiteral("../../")
#endif

namespace {

QString repoRoot()
{
    const QString def = MDIT_REPO_ROOT;
    if (QFileInfo::exists(def + QStringLiteral("/CMakeLists.txt")))
        return def;
    return QStringLiteral("../../");
}

QString readRepoFile(const QString &name)
{
    const QString path = repoRoot() + QLatin1Char('/') + name;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

bool repoFileExists(const QString &name)
{
    return QFileInfo::exists(repoRoot() + QLatin1Char('/') + name);
}

} // namespace

class TestGithubReady : public QObject
{
    Q_OBJECT

private slots:
    // The compile definition points at a real checkout (sanity of the seam
    // itself — every other slot would pass vacuously on an empty root if
    // this were misconfigured... it would not, but pin it anyway).
    void repoRootResolves();

    void readmePresentAndComplete();
    void licenseIsMitWithCopyright();
    void changelogHasReleaseHeading();
    void specPresent();
    void gitignoreComplete();
    void cmakeDeclaresProjectAndTesting();
};

void TestGithubReady::repoRootResolves()
{
    QVERIFY(!repoRoot().isEmpty());
    QVERIFY(repoFileExists(QStringLiteral("CMakeLists.txt")));
}

void TestGithubReady::readmePresentAndComplete()
{
    const QString readme = readRepoFile(QStringLiteral("README.md"));
    QVERIFY2(!readme.isEmpty(), "README.md is missing or empty");
    QVERIFY2(readme.size() > 500, "README.md is suspiciously short");

    // Title + one-line purpose.
    QVERIFY(readme.startsWith(QStringLiteral("# mdit")));

    // Required sections (spec: title, features, Build prereqs+commands,
    // Run + CLI-open, Usage/shortcuts, license note).
    QVERIFY(readme.contains(QStringLiteral("## Features")));
    QVERIFY(readme.contains(QStringLiteral("## Build")));
    QVERIFY(readme.contains(QStringLiteral("## Run")));
    QVERIFY(readme.contains(QStringLiteral("## Usage")));
    QVERIFY(readme.contains(QStringLiteral("Prerequisites")));
    QVERIFY(readme.contains(QStringLiteral("CMake")));
    QVERIFY(readme.contains(QStringLiteral("Qt6")));

    // The exact build command the spec mandates.
    QVERIFY(readme.contains(QStringLiteral(
        "cmake -S . -B build -DBUILD_TESTS=ON -G Ninja && cmake --build build")));

    // The CLI-open form.
    QVERIFY(readme.contains(QStringLiteral("mdit <file>.md")));

    // License note (MIT + the copyright line).
    QVERIFY(readme.contains(QStringLiteral("MIT")));
    QVERIFY(readme.contains(QStringLiteral("Copyright (c) 2026 Pat Wendorf")));
}

void TestGithubReady::licenseIsMitWithCopyright()
{
    const QString license = readRepoFile(QStringLiteral("LICENSE"));
    QVERIFY2(!license.isEmpty(), "LICENSE is missing or empty");
    QVERIFY2(license.contains(QStringLiteral("MIT License")),
             "LICENSE does not contain the 'MIT License' header");
    QVERIFY2(license.contains(QStringLiteral("Copyright (c) 2026 Pat Wendorf")),
             "LICENSE is missing the copyright line");
}

void TestGithubReady::changelogHasReleaseHeading()
{
    const QString changelog = readRepoFile(QStringLiteral("CHANGELOG.md"));
    QVERIFY2(!changelog.isEmpty(), "CHANGELOG.md is missing or empty");
    QVERIFY2(changelog.contains(QStringLiteral("## [0.1.0]")),
             "CHANGELOG.md is missing the '## [0.1.0]' release heading");
    // Keep-a-Changelog style.
    QVERIFY(changelog.contains(QStringLiteral("Keep a Changelog")));
}

void TestGithubReady::specPresent()
{
    const QString spec = readRepoFile(QStringLiteral("spec.md"));
    QVERIFY2(!spec.isEmpty(), "spec.md is missing or empty");
    QVERIFY2(spec.size() > 500, "spec.md is suspiciously short");
    QVERIFY(spec.startsWith(QStringLiteral("# spec.md")));
}

void TestGithubReady::gitignoreComplete()
{
    QVERIFY2(repoFileExists(QStringLiteral(".gitignore")), ".gitignore is missing");
    const QString ignore = readRepoFile(QStringLiteral(".gitignore"));
    QVERIFY2(!ignore.isEmpty(), ".gitignore is empty");
    // The spec's packaging contract: build dirs, CMake/Qt cruft, export
    // artifacts.
    const QStringList required{
        QStringLiteral("build/"),
        QStringLiteral("build-*/"),
        QStringLiteral(".cache/"),
        QStringLiteral("*.user"),
        QStringLiteral(".qmake.stash"),
        QStringLiteral("*.pdf"),
        QStringLiteral("*.html"),
    };
    for (const QString &entry : required)
        QVERIFY2(ignore.contains(entry.toUtf8()),
                 ("gitignore is missing entry: " + entry).toUtf8().constData());
}

void TestGithubReady::cmakeDeclaresProjectAndTesting()
{
    const QString cmake = readRepoFile(QStringLiteral("CMakeLists.txt"));
    QVERIFY2(!cmake.isEmpty(), "CMakeLists.txt is missing or empty");

    // project(...) with a valid name (this repo's is `mdit`).
    static const QRegularExpression projectRe(
        QStringLiteral("\\bproject\\s*\\(\\s*mdit\\b"));
    QVERIFY2(cmake.contains(projectRe),
             "CMakeLists.txt does not declare project(mdit ...)");

    // enable_testing() drives the CTest suite.
    static const QRegularExpression enableTestingRe(
        QStringLiteral("\\benable_testing\\s*\\("));
    QVERIFY2(cmake.contains(enableTestingRe),
             "CMakeLists.txt does not call enable_testing()");

    // C++20 + the Qt6 components are part of the build contract.
    static const QRegularExpression cxx20Re(
        QStringLiteral("CMAKE_CXX_STANDARD\\s+20"));
    QVERIFY2(cmake.contains(cxx20Re), "CMakeLists.txt does not set C++20");
    static const QRegularExpression qt6Re(
        QStringLiteral("find_package\\(Qt6\\s+COMPONENTS\\s+Widgets\\s+Gui\\s+PrintSupport\\s+REQUIRED\\)"));
    QVERIFY2(cmake.contains(qt6Re),
             "CMakeLists.txt does not require Qt6 Widgets/Gui/PrintSupport");
}

QTEST_GUILESS_MAIN(TestGithubReady)
#include "test_github_ready.moc"
