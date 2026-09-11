# mdit

**A calm, native Markdown editor for people who want to write in Markdown and see the result immediately.**

mdit keeps the useful parts close at hand: a clean editor, a live preview, and straightforward file handling. It is built for notes, READMEs, documentation, and any Markdown file where you want to stay focused instead of configuring an editor.

[![CI](https://github.com/patw/mdit/actions/workflows/ci.yml/badge.svg)](https://github.com/patw/mdit/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/patw/mdit?sort=semver)](https://github.com/patw/mdit/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

![mdit with Markdown source on the left and its live preview on the right](assets/screenshots/mdit-split-view.png)

## Why mdit?

Markdown is simple; its editor should be too. mdit is a small desktop app that gives you a responsive writing surface alongside an always-current rendering of your document. There are no accounts, workspaces, browser tabs, or plugin hunting—just your files.

Use it when you want to:

- write Markdown with syntax highlighting and line numbers;
- check headings, tables, task lists, links, and images as you type;
- keep a familiar plain-text workflow with Open, Save, drag and drop, and recent files;
- export a finished document to HTML or PDF; and
- make the interface yours with light/dark themes, accent colours, and UI scaling.

The preview, theme, and split position are remembered between launches. Find and replace, undo/redo, unsaved-change protection, and relative-image support cover the everyday details without getting in the way.

## Features

- Side-by-side Markdown editor and live GFM/CommonMark preview
- Fast native Qt6 app for Linux, macOS, and Windows
- Syntax highlighting, line numbers, word and character counts
- Find, replace, undo, redo, recent files, and drag-and-drop opening
- HTML and PDF export that matches the preview
- Light, dark, and system themes; selectable accents and UI scale
- No web engine or external Markdown library

## Install

Download a release for your platform from the [releases page](https://github.com/patw/mdit/releases). Linux releases are available as a `.deb` and an AppImage; macOS as a DMG; Windows as a ZIP.

On Debian/Ubuntu, install the downloaded package with:

```sh
sudo apt install ./mdit_0.1.0_amd64.deb
```

## Build

### Prerequisites

- CMake 3.22 or newer
- Ninja (or another CMake generator)
- A C++20 compiler
- Qt6 with Widgets, Gui, PrintSupport, and Test

On Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build g++ qt6-base-dev
```

Build and test:

```sh
cmake -S . -B build -DBUILD_TESTS=ON -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```sh
./build/mdit
./build/mdit path/to/notes.md
```

Once installed, `mdit <file>.md` opens an existing `.md` or `.markdown` file. You can also open files from the menu or drop one onto the window.

## Usage

The workflow is intentionally familiar: write on the left, read on the right, then save or export when you are ready.

| Action | Shortcut |
|---|---|
| New / Open / Save | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Shift+Z` or `Ctrl+Y` |
| Find / Replace | `Ctrl+F` / `Ctrl+H` |
| Show or hide preview | `Ctrl+Shift+P` |
| Toggle light/dark | `Ctrl+T` |

Use **File → Export** for HTML or PDF, and **View** for preview, theme, accent, and scale settings.

## License

MIT License. Copyright (c) 2026 Pat Wendorf. See [LICENSE](LICENSE).
