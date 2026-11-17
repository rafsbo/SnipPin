# Manual test checklist

Automated tests cover the platform-independent logic only (`tests/`). The Win32 behaviour must be checked by hand before each release on, at minimum, **Windows 7 SP1 (Aero and Basic theme)**, **Windows 10** and **Windows 11**, using both the x86 and x64 builds. A virtual machine is fine for Windows 7.

Record the result in the release pull request as ✅ / ❌ / n.a. per OS.

## Start-up and tray
- [ ] Fresh start shows the tray icon and the first-run notification once.
- [ ] Starting a second instance opens the stash panel of the first one instead.
- [ ] Restarting Explorer (`taskkill /f /im explorer.exe`, then start it) brings the tray icon back.
- [ ] `portable.ini` next to the exe makes settings/history appear next to the exe.

## Always on top
- [ ] `Ctrl+Alt+T` pins/unpins Notepad, an Explorer folder and a dialog (e.g. *Run*).
- [ ] A pinned window stays above: a maximized window, a browser in F11 fullscreen, a PowerPoint slideshow, a video player in fullscreen.
- [ ] Two "always on top" tools at once do not cause flicker or high CPU (rate limiting).
- [ ] Pinning a window of an elevated app shows the "elevated" notification; after *Restart as administrator* it works.

## Opacity and ghost mode
- [ ] `Ctrl+Alt+↓/↑` changes opacity in steps and never goes below the minimum.
- [ ] `Ctrl+Alt+G` makes the window click-through; clicks reach the app underneath.
- [ ] `Ctrl+Alt+G` again (with another app focused) brings the ghost back.
- [ ] `Ctrl` + right-click over a ghost window opens its menu.
- [ ] `Ctrl+Alt+Backspace` restores every window and snippet.
- [ ] A window with its own per-pixel alpha (e.g. a WPF app with transparency) reports "unsupported" and stays intact.

## Crash safety
- [ ] Ghost a window, kill SnipPin from Task Manager, start it again: the window is restored and a notification is shown.
- [ ] Sign out with a ghosted window: after signing in nothing is left translucent.

## Context menus
- [ ] `Ctrl` + right-click on a window opens the SnipPin menu; the application does not show its own menu.
- [ ] Changing the gesture modifier in Settings takes effect immediately; disabling it restores normal right-click.
- [ ] Tray right-click menu: last active window, pinned windows, snippets, recent, global items all work.
- [ ] Menus close when clicking elsewhere.

## Snippets
- [ ] `Ctrl+Alt+S`: drag selects a region; a single click selects the whole window; Esc cancels.
- [ ] The live snippet updates while the source is covered by other windows.
- [ ] Minimizing the source shows "Source window is minimized"; restoring resumes.
- [ ] Closing the source shows "Source window was closed" (or keeps the last frame).
- [ ] Mouse wheel zooms, `Ctrl` + wheel changes opacity (also on Windows 7, where inactive windows normally get no wheel input).
- [ ] Resizing from an edge keeps the aspect ratio.
- [ ] Switching to *Frozen snapshot* keeps the image; *Refresh* updates it.
- [ ] Windows 7 Basic theme: snippets still work (PrintWindow renderer).
- [ ] Ghost mode + opacity on a snippet behave like on normal windows (verify on Windows 7 with Aero).
- [ ] Mixed DPI (100 % + 150 % monitors): the crop matches the selected area for a DPI-aware app (e.g. Notepad on Windows 11) and a DPI-unaware app.

## Stash
- [ ] Pinned windows, snippets and recent entries appear in the right groups.
- [ ] Recall of a running window re-applies pin/opacity/ghost and recreates snippets.
- [ ] Recall of a closed Explorer folder reopens the folder and pins it.
- [ ] History survives a restart; non-ASCII titles/paths display correctly.

## Settings
- [ ] Changing a hotkey works immediately; a conflicting hotkey is reported.
- [ ] *Start SnipPin when I sign in* adds/removes the `HKCU\...\Run` value.
- [ ] *Restore defaults* resets the dialog.

## UIAccess edition
- [ ] Unsigned `SnipPinUIA.exe` outside Program Files fails to start (expected, documented error).
- [ ] After `install-uiaccess.ps1 -SelfSign`, it starts from the Start menu; About shows *UIAccess*.
- [ ] Snippets stay above the taskbar and above another topmost tool.
- [ ] `install-uiaccess.ps1 -Uninstall` removes files, shortcut and certificate.
