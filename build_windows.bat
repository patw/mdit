@echo off
REM Build mdit for Windows: mdit.exe + the Qt DLLs next to it, ready to zip.
REM
REM Prerequisites:
REM   1. Qt6 (MSVC 64-bit): https://www.qt.io/download-qt-installer
REM      e.g. C:\Qt\6.8.2\msvc2022_64
REM   2. Visual Studio Build Tools 2022 ("Desktop development with C++")
REM   3. CMake (or: winget install Kitware.CMake)
REM
REM Run from a "Developer Command Prompt for VS 2022".
REM CI does the same steps in .github/workflows/release.yml.
setlocal enabledelayedexpansion
set ROOT=%~dp0
cd /d "%ROOT%"

if "%QT6_DIR%"=="" set QT6_DIR=%Qt6_DIR%
if "%QT6_DIR%"=="" (
    for %%V in (6.10.2 6.10.1 6.10.0 6.9.0 6.8.2) do (
        if exist "C:\Qt\%%V\msvc2022_64" set QT6_DIR=C:\Qt\%%V\msvc2022_64
    )
)
if "%QT6_DIR%"=="" (
    echo ERROR: Could not find Qt6. Set QT6_DIR, e.g. set QT6_DIR=C:\Qt\6.8.2\msvc2022_64
    exit /b 1
)
echo Using Qt6: %QT6_DIR%
set PATH=%QT6_DIR%\bin;%PATH%

echo.
echo ==^> Building mdit...
cmake -S . -B build_windows -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT6_DIR%"
if errorlevel 1 exit /b 1
cmake --build build_windows --parallel
if errorlevel 1 exit /b 1

echo.
echo ==^> Smoke testing...
build_windows\mdit.exe --version
if errorlevel 1 exit /b 1

echo.
echo ==^> Bundling the Qt DLLs...
set DIST=%ROOT%mdit-Windows
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy build_windows\mdit.exe "%DIST%\" >nul
cd "%DIST%"
windeployqt --release --no-translations mdit.exe
cd /d "%ROOT%"

echo.
echo ==^> Packaging a zip...
powershell -NoProfile -Command "Compress-Archive -Path 'mdit-Windows\*' -DestinationPath 'mdit-Windows.zip' -Force"

echo.
echo ==^> Done: %DIST% (and mdit-Windows.zip)
endlocal
