# SnipPin

[![Build](https://github.com/rafsbo/SnipPin/actions/workflows/build.yml/badge.svg)](https://github.com/rafsbo/SnipPin/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Windows 7 – 11](https://img.shields.io/badge/Windows-7%20%E2%80%93%2011-0078D6)

**SnipPin** is a lightweight, open-source Windows utility that keeps any window, folder or dialog **always on top**, even over fullscreen applications. It can also pin a **cropped, live snippet** of a window as a floating mini-widget. It's built for data comparison: keep a reference table, a chart or a folder visible while you work in another app.

- Single native executable (C++17 / Win32). No .NET, no runtime to install, about 2–5 MB of RAM.
- Runs on **Windows 7 SP1, 8, 8.1, 10 and 11**, in x86 and x64 builds.
- No DLL injection, no telemetry, no network access.
- Available in the **24 official languages of the European Union**.

---

## Features

| | |
|---|---|
| 📌 **Always on top** | Pin any standard window, Explorer folder or dialog above everything else. A z-order guard re-asserts the pin when a fullscreen app pushes itself to the front. |
| 🌫️ **Real-time transparency** | Change opacity in 10 % steps with a hotkey, the context menu or `Ctrl` + mouse wheel on a snippet. |
| 👻 **Ghost mode (click-through)** | The window stays visible but mouse clicks pass straight through to the application underneath (`WS_EX_TRANSPARENT`). |
| ✂️ **Snippet pinning** | Drag a rectangle over any part of a window and pin it as a floating widget. Snippets are **live mirrors** (DWM thumbnails, GPU-composited, near-zero CPU) or **frozen snapshots** for before/after comparisons. You can zoom them with the mouse wheel. |
| 🪞 **Mirror whole window** | A live copy of an entire window, useful when the original must stay where it is. |
| 🗂️ **Stash & history** | A side panel lists pinned windows, snippets and recently used windows. One click brings a window back with its pin, opacity, ghost mode and snippet regions. Closed folders and apps are reopened automatically. |
| 🖱️ **Right-click menus everywhere** | `Ctrl` + right-click on any window, right-click on the tray icon, on a snippet or on a stash entry. Every feature is in these menus as well as on a keyboard shortcut. |
| 🌍 **24 languages** | Bulgarian, Croatian, Czech, Danish, Dutch, English, Estonian, Finnish, French, German, Greek, Hungarian, Irish, Italian, Latvian, Lithuanian, Maltese, Polish, Portuguese, Romanian, Slovak, Slovenian, Spanish and Swedish. SnipPin follows the Windows display language by default, and you can switch in *Settings* without restarting. |
| 🚀 **Start with Windows** | One click in the tray menu (*Start with Windows*) or in *Settings*, and SnipPin is back in the notification area after every restart. |
| 🛟 **Safe by design** | Original window styles are journaled before any change. A crash, kill or log-off never leaves another app's window translucent or click-through: SnipPin restores them on the next start. A panic hotkey restores everything instantly. |

## What "on top of fullscreen" can and cannot mean

| Kind of fullscreen app | Can SnipPin stay above it? |
|---|---|
| Maximized windows; borderless fullscreen (browser F11, Excel/Word full-screen view, most modern games in "borderless" mode, video players) | ✅ Yes |
| Apps that are topmost themselves (PowerPoint slideshow, kiosk apps) | ✅ Yes. The z-order guard re-asserts the pin after they take the foreground. |
| True **exclusive-mode** DirectX/OpenGL fullscreen (older games) | ❌ Not with documented APIs: the app owns the display. On Windows 10+, *Fullscreen Optimizations* usually turn these into borderless windows, which then work. |
| Above the Start menu and other topmost tools | ⚙️ Use the **UIAccess edition** for snippets and mirrors (see below). |

Windows that belong to elevated (administrator) applications can only be managed when SnipPin runs elevated too: use **Restart as administrator** from the menu.

## System requirements

- Windows 7 SP1, 8, 8.1, 10 or 11 (32-bit or 64-bit).
- Live snippets on Windows 7 need the Aero theme (DWM composition). With the Basic or Classic theme SnipPin switches to a compatible renderer automatically.
- No other dependency.

## Installation

1. Download `SnipPin-<version>-x64.zip` (or `-x86` for 32-bit Windows) from the [Releases](https://github.com/rafsbo/SnipPin/releases) page.
2. Extract it anywhere and run **`SnipPin.exe`**. The icon appears in the notification area.
3. Optional: right-click the tray icon and tick **Start with Windows**, so you never have to start SnipPin again after a restart.

> Unsigned builds may trigger a SmartScreen warning ("Windows protected your PC"). Choose *More info → Run anyway*, or build from source.

**Portable mode:** create an empty file named `portable.ini` next to `SnipPin.exe`. Settings and history are then stored next to the executable instead of `%APPDATA%\SnipPin`.

## Quick start

1. Click the window you want to keep visible and press **`Ctrl+Alt+T`**. It is now always on top.
2. Press **`Ctrl+Alt+↓`** a few times to make it translucent, then **`Ctrl+Alt+G`** for ghost mode: you can now click and type in the app underneath.
3. Press **`Ctrl+Alt+G`** again to get the window back, or **`Ctrl+Alt+Backspace`** to restore everything.
4. To pin only part of a window, press **`Ctrl+Alt+S`**, drag a rectangle, and move the floating snippet wherever you like.

## Keyboard shortcuts

All shortcuts are global and can be changed in *Settings*.

| Action | Default |
|---|---|
| Toggle always on top for the active window | `Ctrl+Alt+T` |
| Pick a window with the crosshair (then choose an action) | `Ctrl+Alt+P` |
| Decrease / increase opacity (window, or snippet under the cursor) | `Ctrl+Alt+↓` / `Ctrl+Alt+↑` |
| Toggle ghost mode (click-through) | `Ctrl+Alt+G` |
| Snip a region of the active window | `Ctrl+Alt+S` |
| Show / hide the stash panel | `Ctrl+Alt+H` |
| **Restore all windows (panic)** | `Ctrl+Alt+Backspace` |

`Ctrl+Alt+G` is smart: if the active window is a ghost it turns ghost mode off. Otherwise, if any window or snippet is ghosted (and therefore cannot be clicked), it brings back the most recently ghosted one. If nothing is ghosted, it ghosts the snippet under the cursor or the active window.

## Mouse and context menus

| Where | What happens |
|---|---|
| **`Ctrl` + right-click** on any window | The SnipPin menu for that window: always on top, opacity, ghost mode, snip, mirror, restore. The click is not passed to the application. The modifier can be changed (or the gesture disabled) in *Settings*. Ghosted windows and snippets under the cursor are found too, so this is also the quickest way to bring them back. |
| Right-click the **tray icon** | Last active window, pinned windows, snippets, recent windows, global actions. Left-click toggles the stash panel. |
| Right-click a **snippet** | Live/snapshot, refresh, opacity, ghost, actual size, show source, select a different region, close. |
| Snippet: drag / edges / wheel | Move / resize (aspect ratio kept) / zoom. `Ctrl` + wheel changes opacity, double-click shows the source window. |
| Right-click a **stash panel** row | The menu for that pinned window, snippet or history entry. Double-click recalls it. |

## UIAccess edition (`SnipPinUIA.exe`)

Every release contains a second executable built with `uiAccess="true"` in its manifest. Windows then places SnipPin's **own** windows (snippets, mirrors, menus) in a z-order band *above* ordinary topmost windows, including the taskbar and other "always on top" tools.

Windows only starts a uiAccess program when it is **digitally signed with a trusted certificate** and **installed in a secure folder** such as `C:\Program Files`. The included `install-uiaccess.ps1` script does both, and can create a certificate trusted only on your machine. See **[docs/UIACCESS.md](docs/UIACCESS.md)** for details and the Windows 7 procedure.

> The higher band applies to windows SnipPin creates. A pinned window from another application stays in the normal topmost band. For the strongest "on top" behaviour, use **Mirror whole window** or a snippet.

## Start with Windows

Tick **Start with Windows** in the tray menu, or *Start SnipPin automatically when I sign in to Windows* in *Settings*. SnipPin then registers itself for the current user (`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`), with no administrator rights and no installer.

- If you move `SnipPin.exe` to another folder, the start-up entry is repaired the next time you start SnipPin from its new location.
- On Windows 8 and later, the switch in *Task Manager → Startup apps* (or *Settings → Apps → Startup*) is respected. SnipPin shows the real state, and ticking the option again re-enables it.
- *Restart as administrator* is not remembered at start-up: Windows does not start elevated programs from the Run key. To manage administrator windows without elevation, use the [UIAccess edition](docs/UIACCESS.md).

## Languages

SnipPin speaks all 24 official EU languages. *Settings → Language* offers *Automatic (system language)*, the default, or any specific language. The change applies immediately.

The translations are new: corrections from native speakers are very welcome. See **[docs/TRANSLATING.md](docs/TRANSLATING.md)**. You can also try a translation without compiling: put the edited `<code>.lang` file in a `lang` folder next to `SnipPin.exe`.

## Configuration files

| File | Content |
|---|---|
| `settings.ini` | Hotkeys, language, gesture modifier, opacity step, z-order guard, snippet renderer… (human-editable, UTF-16) |
| `history.ini` | Stash / recent windows with their pin, opacity, ghost and snippet regions |
| `journal.ini` | Crash-recovery journal (temporary; deleted on a clean exit) |
| `snippin.log` | Only when `[Debug] LogToFile=1` is set |

Location: `%APPDATA%\SnipPin\` (or next to the exe in portable mode).

## Building from source

### Visual Studio 2022 (recommended)

Requirements: Visual Studio 2022 with *Desktop development with C++* (MSVC **v143** toolset, Windows SDK) and CMake 3.21+.

```bash
cmake --preset vs2022-x64
```

```bash
cmake --build --preset vs2022-x64-release
```

```bash
ctest --preset vs2022-x64-release
```

Use `vs2022-x86` / `vs2022-x86-release` for 32-bit. You can also open the folder in Visual Studio (*File → Open → Folder*) and pick a preset. Executables are written to `build/<preset>/bin/Release/`.

> Keep the **v143** toolset: newer MSVC toolsets are dropping Windows 7 as a target, while v143 binaries are known to run on Windows 7 SP1. The project builds with the static CRT (`/MT`), so no redistributable is needed.

### MinGW-w64 (cross-compile from Linux)

```bash
sudo apt-get install g++-mingw-w64 cmake
```

```bash
cmake --preset mingw-x64 && cmake --build --preset mingw-x64
```

### Unit tests on any OS

The platform-independent logic (geometry, hotkey parsing, history matching, translation catalogues) has unit tests that also run on Linux and macOS. They also check that every language file is complete and safe:

```bash
cmake --preset tests-host && cmake --build --preset tests-host && ctest --preset tests-host
```

## Project structure

```
src/
  app/        App host, command router, tray icon, hotkeys, mouse gesture hook
  i18n/       String table (keys + English), catalogue parser, safe formatting
  core/       WindowTracker (pin/opacity/ghost), ZOrderGuard, StashService, RecoveryJournal
  snippet/    RegionSelector overlay, SnippetWindow (DWM / PrintWindow / snapshot), SnippetManager
  ui/         WindowPicker, StashPanel, SettingsDialog
  platform/   Win32 helpers, runtime API shim for post-Windows-7 APIs, settings, localization, logging
  logic/      Platform-independent, unit-tested logic
res/          Icon, manifests (standard + uiAccess), version info, settings dialog
res/lang/     Translations (one UTF-8 .lang file per language)
tests/        Unit tests (self-contained harness, no dependencies)
docs/         Architecture, UIAccess guide, translation guide, manual test checklist
tools/        Icon generator, UIAccess installer script
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design.

## Contributing

Bug reports, ideas, translations and pull requests are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and, for translations, [docs/TRANSLATING.md](docs/TRANSLATING.md). Code and comments are in English; UI text is translated through `res/lang/`.

## License

[MIT](LICENSE) © 2026 Raffaele Borrelli and SnipPin contributors.
