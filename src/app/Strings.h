// All user-visible UI text lives here so it can be reviewed and translated
// in one place.
#pragma once

namespace snippin::str {

inline constexpr wchar_t kAppName[] = L"SnipPin";
inline constexpr wchar_t kTagline[] = L"Pin windows and live snippets on top of everything.";

// Tray / menus
inline constexpr wchar_t kMenuAlwaysOnTop[] = L"&Always on top";
inline constexpr wchar_t kMenuOpacity[] = L"&Opacity";
inline constexpr wchar_t kMenuGhost[] = L"&Ghost mode (click-through)";
inline constexpr wchar_t kMenuSnipRegion[] = L"&Snip a region\u2026";
inline constexpr wchar_t kMenuMirrorWindow[] = L"&Mirror whole window";
inline constexpr wchar_t kMenuRelease[] = L"&Restore original state";
inline constexpr wchar_t kMenuFocus[] = L"&Bring to front";
inline constexpr wchar_t kMenuLastActive[] = L"Last active window";
inline constexpr wchar_t kMenuPinnedWindows[] = L"&Pinned windows";
inline constexpr wchar_t kMenuSnippets[] = L"S&nippets";
inline constexpr wchar_t kMenuRecent[] = L"Re&cent";
inline constexpr wchar_t kMenuRecall[] = L"&Recall";
inline constexpr wchar_t kMenuRemoveFromHistory[] = L"Remove from &history";
inline constexpr wchar_t kMenuClearHistory[] = L"&Clear history";
inline constexpr wchar_t kMenuPickWindow[] = L"P&ick a window\u2026";
inline constexpr wchar_t kMenuShowStash[] = L"Show s&tash panel";
inline constexpr wchar_t kMenuRestoreAll[] = L"Restore &all windows";
inline constexpr wchar_t kMenuSettings[] = L"S&ettings\u2026";
inline constexpr wchar_t kMenuRunAsAdmin[] = L"Restart as a&dministrator";
inline constexpr wchar_t kMenuAbout[] = L"A&bout SnipPin";
inline constexpr wchar_t kMenuExit[] = L"E&xit";
inline constexpr wchar_t kMenuNone[] = L"(none)";
inline constexpr wchar_t kMenuCloseAllSnippets[] = L"Close all snippets";

// Snippet menu
inline constexpr wchar_t kMenuLiveMode[] = L"&Live mirror";
inline constexpr wchar_t kMenuSnapshotMode[] = L"&Frozen snapshot";
inline constexpr wchar_t kMenuRefreshSnapshot[] = L"Re&fresh snapshot";
inline constexpr wchar_t kMenuZoom100[] = L"Actual si&ze (100%)";
inline constexpr wchar_t kMenuFocusSource[] = L"Show &source window";
inline constexpr wchar_t kMenuReselect[] = L"Select a &different region\u2026";
inline constexpr wchar_t kMenuCloseSnippet[] = L"&Close snippet";
inline constexpr wchar_t kSnippetPrefix[] = L"Snippet: ";

// Overlays and status text
inline constexpr wchar_t kRegionSelectorHint[] =
    L"Drag to select a region  \u00B7  Click for the whole window  \u00B7  Esc to cancel";
inline constexpr wchar_t kPickerHint[] = L"Click a window to choose it  \u00B7  Esc to cancel";
inline constexpr wchar_t kSourceMinimized[] = L"Source window is minimized";
inline constexpr wchar_t kSourceClosed[] = L"Source window was closed";
inline constexpr wchar_t kNoContent[] = L"Nothing to show";

// Notifications
inline constexpr wchar_t kNotifyRunning[] =
    L"SnipPin is running in the notification area. Ctrl+right-click any window for options.";
inline constexpr wchar_t kNotifyAccessDenied[] =
    L"This window belongs to an elevated (administrator) application. Use \"Restart as administrator\" to manage it.";
inline constexpr wchar_t kNotifyUnsupported[] =
    L"This window draws its own transparency, so its opacity cannot be changed. You can still pin it or snip it.";
inline constexpr wchar_t kNotifyNotCandidate[] = L"This window cannot be pinned.";
inline constexpr wchar_t kNotifyHotkeyFailed[] =
    L"Some keyboard shortcuts are already used by another program: ";
inline constexpr wchar_t kNotifyRecovered[] = L"Restored windows left modified by a previous session.";
inline constexpr wchar_t kNotifyRecallLaunching[] = L"Opening \u201C%s\u201D\u2026";
inline constexpr wchar_t kNotifyRecallNotFound[] = L"Could not find or open \u201C%s\u201D.";
inline constexpr wchar_t kNotifyNoTarget[] = L"No window selected. Click a window first or use Pick a window.";
inline constexpr wchar_t kNotifyRestored[] = L"All windows were restored.";

// Stash panel
inline constexpr wchar_t kStashTitle[] = L"SnipPin \u2013 Stash";
inline constexpr wchar_t kStashColumnWindow[] = L"Window";
inline constexpr wchar_t kStashColumnState[] = L"State";
inline constexpr wchar_t kStashGroupPinned[] = L"Pinned now";
inline constexpr wchar_t kStashGroupSnippets[] = L"Snippets";
inline constexpr wchar_t kStashGroupRecent[] = L"Recent";
inline constexpr wchar_t kStateTopmost[] = L"On top";
inline constexpr wchar_t kStateGhost[] = L"Ghost";
inline constexpr wchar_t kStateLive[] = L"Live";
inline constexpr wchar_t kStateSnapshot[] = L"Snapshot";

// About
inline constexpr wchar_t kAboutFormat[] =
    L"SnipPin %s\n\n%s\n\nMode: %s\nSettings: %s\n\nOpen source under the MIT License.\nhttps://github.com/rafsbo/SnipPin";
inline constexpr wchar_t kModeStandard[] = L"Standard";
inline constexpr wchar_t kModeAdmin[] = L"Administrator";
inline constexpr wchar_t kModeUiAccess[] = L"UIAccess (above other topmost windows)";
inline constexpr wchar_t kModePortableSuffix[] = L" (portable)";

}  // namespace snippin::str
