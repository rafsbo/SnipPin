# Architecture

SnipPin is a single-process, single-UI-thread Win32 application written in C++17. It has no framework and no runtime dependencies beyond system DLLs that ship with Windows 7.

```
┌──────────────────────────── SnipPin.exe (single process, single UI thread) ───────────────────────────┐
│                                                                                                        │
│  App (hidden top-level host window + message loop)                                                     │
│    ├── HotkeyManager    RegisterHotKey, user-configurable, conflict reporting                          │
│    ├── TrayIcon         Shell_NotifyIcon v4, re-added on "TaskbarCreated"                              │
│    ├── MouseHook        WH_MOUSE_LL in-process: modifier+right-click menu, wheel routing to snippets   │
│    └── Menus/commands   one command layer for hotkeys, tray, gesture, snippet and panel menus          │
│                                                                                                        │
│  Core                                                                                                  │
│    ├── WindowTracker    pinned windows: original ex-style, alpha, topmost state, identity             │
│    ├── ZOrderGuard      out-of-context WinEvent hooks → debounced, rate-limited HWND_TOPMOST re-assert │
│    ├── StashService     MRU history (history.ini), recall / relaunch / reopen folders                  │
│    └── RecoveryJournal  write-ahead log of every style change (journal.ini)                            │
│                                                                                                        │
│  Snippets                                                                                              │
│    ├── RegionSelector   colour-keyed overlay to drag the region                                        │
│    ├── SnippetWindow    DWM thumbnail │ PrintWindow fallback │ frozen snapshot                         │
│    └── SnippetManager   ownership, deferred close, hit-testing                                         │
│                                                                                                        │
│  UI: WindowPicker (crosshair) · StashPanel (ListView with groups) · SettingsDialog (resource dialog)   │
│  Platform: ApiShim (runtime-resolved Win8+/Win10+ APIs) · WinUtil · Settings (UTF-16 INI) · Log        │
│  Logic (no <windows.h>, unit-tested): Geometry · Hotkey parsing · Window identity matching             │
└────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Design principles

### Windows 7 baseline, newer features at runtime
The code is compiled with `_WIN32_WINNT=0x0601`. Every newer API (per-monitor DPI functions, `SystemParametersInfoForDpi`, `GetDpiForMonitor`, …) is resolved with `GetProcAddress` in `platform/ApiShim.cpp`, with a fallback. The OS version comes from `RtlGetVersion`, which is not subject to manifest-based version lies. CI runs `tools/check_win7_imports.py` on the MinGW builds and fails if a post-Windows-7 function is statically imported.

### No injection, no global DLL hooks
- WinEvent hooks are **out-of-context** (`WINEVENT_OUTOFCONTEXT`), so callbacks run on SnipPin's own thread.
- The single low-level mouse hook (`WH_MOUSE_LL`) also runs in-process by design. Its callback only checks modifier keys, compares the cursor with SnipPin's own snippet rectangles and posts a message. Windows silently removes slow low-level hooks, so nothing heavy happens there.
- One executable therefore handles 32-bit and 64-bit target windows, and avoids the antivirus false positives common with injection-based tools.

### Every change to a foreign window is reversible
`WindowTracker` captures the original extended style, layered attributes and topmost state on first touch. `RecoveryJournal` writes them to `journal.ini` **before** the change is applied. They are restored:
- when the user releases the window,
- on exit, on log-off (`WM_ENDSESSION`) and from the unhandled-exception filter,
- on the next start, if SnipPin was killed. Entries are validated by HWND **and** process id, because HWNDs are recycled.

The panic hotkey (`Ctrl+Alt+Backspace`) and the gesture menu (which also finds ghosted windows under the cursor) guarantee that a click-through window can always be recovered.

Windows that draw their own per-pixel alpha (`UpdateLayeredWindow`: some WPF, Chromium or Electron windows) reject `SetLayeredWindowAttributes`. They are detected (`GetLayeredWindowAttributes` fails) and reported as *unsupported* instead of being broken. Pinning still works for them.

### Z-order guard
`HWND_TOPMOST` alone is not enough: a fullscreen or topmost app that becomes foreground can raise itself above pinned windows. `ZOrderGuard` listens for `EVENT_SYSTEM_FOREGROUND` and `EVENT_SYSTEM_MINIMIZEEND`. After a 60 ms debounce it re-inserts every pinned window at `HWND_TOPMOST` with `SWP_NOACTIVATE`. A slower periodic check repairs windows that lost `WS_EX_TOPMOST`. Re-assertions are capped (15 per second) so two "always on top" tools can never fight in a tight loop. The guard pauses while a menu, the picker or the region selector is open.

### Snippet rendering
| Renderer | When | Notes |
|---|---|---|
| DWM thumbnail | Default (DWM composition on) | `DwmRegisterThumbnail` + `rcSource` crop. GPU composited, updates while covered. The crop is rescaled with `DwmQueryThumbnailSourceSize`, because Windows bitmap-stretches DPI-unaware windows. |
| PrintWindow | Composition off (Win7 Basic/Classic), or *compatibility rendering* enabled | Polls at a configurable interval. `PW_RENDERFULLCONTENT` on Windows 8.1+. |
| Frozen snapshot | User choice | Captured from the screen at selection time (exactly what the user saw); *Refresh* uses PrintWindow. |

Regions are stored relative to the source window's top-left corner in physical pixels, so they survive moves and are reusable from the history.

Snippets are `WS_EX_NOACTIVATE` tool windows: dragging or zooming them never steals focus from the app being worked in. On Windows 7/8, inactive windows don't receive wheel input, so the mouse hook routes wheel events to the snippet under the cursor.

### DPI
The manifest declares PerMonitorV2 (with `true/pm` for Windows 8.1 and system awareness on 7). All geometry is handled in physical pixels. Overlay text, borders and the stash panel scale with `GetDpiForWindow` via `ApiShim`. Snippets intentionally keep their content 1:1 when moved across monitors.

### Command layer
Hotkeys, the tray menu, the modifier+right-click menu, snippet menus and stash panel menus all go through the same functions in `App` (`ExecuteWindowAction`, `ExecuteSnippetAction`, …). Menu ids are allocated in blocks of 32 per object, so one menu can contain actions for many windows and snippets.

Snippet destruction is always **deferred** through the host window's queue. A snippet is never deleted while one of its own message handlers (for example its context menu) is still on the stack.

### Identity and recall
HWNDs don't survive restarts, so history entries store the process image path, window class, title and, for Explorer windows, the folder path (`IShellWindows`). `logic::MatchScore` ranks candidates. Explorer windows only match the same folder. Recall applies the saved state to a live match, or launches the app / opens the folder and waits up to 15 s for the window to appear.

## Process model and instances
A named mutex enforces one instance per session. A second launch posts a registered message that opens the stash panel of the running instance. `ChangeWindowMessageFilterEx` lets this message reach an elevated instance. *Restart as administrator* starts the elevated copy with `--replace`, and it waits for the mutex.

## Files
| File | Owner |
|---|---|
| `settings.ini` | `SettingsStore` |
| `history.ini` | `StashService` |
| `journal.ini` | `RecoveryJournal` |

INI files are created with a UTF-16LE BOM, so the profile API stores non-ASCII titles and paths correctly.
