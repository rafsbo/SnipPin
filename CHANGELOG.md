# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed
- `install-uiaccess.ps1` failed in Windows PowerShell 5.1 with "Cannot bind argument to parameter 'Path' because it is an empty string" when run with `-File`: the script folder is now resolved reliably.

## [1.1.0] - Unreleased

### Added
- User interface in the 24 official EU languages: Bulgarian, Croatian, Czech, Danish, Dutch, English, Estonian, Finnish, French, German, Greek, Hungarian, Irish, Italian, Latvian, Lithuanian, Maltese, Polish, Portuguese, Romanian, Slovak, Slovenian, Spanish, Swedish.
- *Settings → Language*: automatic (follows the Windows display language) or any language; switching applies immediately.
- Community translations can be tested without compiling by placing `<code>.lang` files in a `lang` folder next to the executable.
- *Start with Windows* item in the tray menu.
- Unit tests that check every translation for completeness, placeholders and menu accelerators.
- `docs/TRANSLATING.md` guide for translators.

### Changed
- Start with Windows is more reliable: the entry is repaired when SnipPin is moved, Task Manager's "Disabled" state is shown correctly, and SnipPin waits for the notification area when Windows starts it early at sign-in.
- The settings dialog is wider so that longer translations fit.
- Translated messages use a safe placeholder formatter instead of `printf`.

## [1.0.0] - 2026-11-17

### Added
- Always-on-top pinning for any window, Explorer folder or dialog, with a z-order guard for fullscreen applications.
- Real-time opacity control and ghost mode (click-through).
- Snippet pinning: live DWM mirrors of a window region, PrintWindow fallback, frozen snapshots, zoom and whole-window mirrors.
- Stash panel and persistent history with recall (re-applies pin, opacity, ghost mode and snippets; reopens folders and apps).
- Context menus: tray icon, modifier + right-click on any window, snippets and stash panel entries.
- Configurable global hotkeys and settings dialog; portable mode.
- Crash-recovery journal and panic restore hotkey.
- UIAccess edition (`SnipPinUIA.exe`) and installer script.
- Windows 7 SP1 – Windows 11 support, x86 and x64 builds.

[Unreleased]: https://github.com/rafsbo/SnipPin/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/rafsbo/SnipPin/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/rafsbo/SnipPin/releases/tag/v1.0.0
