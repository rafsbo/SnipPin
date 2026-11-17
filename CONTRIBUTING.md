# Contributing to SnipPin

Thanks for helping! Bug reports, feature ideas, documentation fixes and pull requests are all welcome.

## Ground rules

- **English everywhere**: code identifiers, comments, commit messages, UI text, documentation.
- **Windows 7 SP1 must keep working.** The project compiles with `_WIN32_WINNT=0x0601`. If you need a newer API, resolve it at runtime in `src/platform/ApiShim.cpp` and provide a fallback. CI fails if a post-Windows-7 function is imported statically.
- **No new runtime dependencies.** SnipPin ships as a single executable with the static CRT.
- **Never break other applications' windows.** Any change to a foreign window must go through `WindowTracker`, so it is journaled and reversible.
- **No DLL injection or global in-context hooks.**

## Getting started

1. Fork and clone the repository.
2. Build with Visual Studio 2022 (v143 toolset) using the CMake presets. See [README.md](README.md#building-from-source).
3. Run the unit tests (`ctest --preset vs2022-x64-release` or `ctest --preset tests-host`).

## Code style

- C++17, Win32 API (wide-character `W` functions only).
- 4-space indentation, 120-column soft limit, braces on the same line.
- Types and functions in `PascalCase`, variables in `camelCase`, members with a trailing underscore (`hwnd_`), constants as `kName`.
- Prefer small RAII helpers (`GdiObject`) over manual cleanup.
- Put platform-independent logic in `src/logic/` (no `<windows.h>`) and add unit tests in `tests/`.
- All user-visible strings belong in `src/app/Strings.h`.

## Pull requests

1. Create a feature branch from `main`.
2. Keep pull requests focused. Describe *what* and *why*, and add screenshots/GIFs for UI changes.
3. Make sure CI is green (MSVC x86/x64, MinGW cross build, unit tests, Windows 7 import check).
4. Tick the relevant items of [docs/TESTING.md](docs/TESTING.md) that you checked by hand, and say on which Windows versions.

## Reporting bugs

Please use the issue templates and include your Windows version, display scaling (DPI) and the application whose window misbehaves. A log can be enabled with `[Debug] LogToFile=1` in `settings.ini`.

## License

By contributing you agree that your contributions are licensed under the [MIT License](LICENSE).
