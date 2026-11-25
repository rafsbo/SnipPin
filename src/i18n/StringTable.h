// Every user-visible UI string, its translation key and its English text.
//
// English is compiled in and is always the fallback. Translations live in
// res/lang/<code>.lang (UTF-8 "key = value" files, see docs/TRANSLATING.md).
// This header is platform independent so the catalogue can be unit-tested.
#pragma once

#include <string>
#include <string_view>

// X(Id, "key", L"English text")
// Placeholders: %s = text, %d = number. A translation must keep the same
// placeholders in the same order (checked by the unit tests and at runtime).
#define SNIPPIN_STRING_TABLE(X)                                                                                  \
    X(AppTagline, "app.tagline", L"Pin windows and live snippets on top of everything.")                         \
    /* ---- window menu ---- */                                                                                  \
    X(MenuAlwaysOnTop, "menu.always_on_top", L"&Always on top")                                                  \
    X(MenuOpacity, "menu.opacity", L"&Opacity")                                                                  \
    X(MenuGhost, "menu.ghost", L"&Ghost mode (click-through)")                                                   \
    X(MenuSnipRegion, "menu.snip_region", L"&Snip a region…")                                               \
    X(MenuMirrorWindow, "menu.mirror_window", L"&Mirror whole window")                                           \
    X(MenuRestoreWindow, "menu.restore_window", L"&Restore original state")                                      \
    X(MenuBringToFront, "menu.bring_to_front", L"&Bring to front")                                               \
    /* ---- tray / global menu ---- */                                                                           \
    X(MenuLastActive, "menu.last_active", L"Last active window")                                                 \
    X(MenuPinnedWindows, "menu.pinned_windows", L"&Pinned windows")                                              \
    X(MenuSnippets, "menu.snippets", L"S&nippets")                                                               \
    X(MenuRecent, "menu.recent", L"Re&cent")                                                                     \
    X(MenuRecall, "menu.recall", L"&Recall")                                                                     \
    X(MenuRemoveFromHistory, "menu.remove_from_history", L"Remove from &history")                                \
    X(MenuClearHistory, "menu.clear_history", L"&Clear history")                                                 \
    X(MenuPickWindow, "menu.pick_window", L"P&ick a window…")                                               \
    X(MenuShowStash, "menu.show_stash", L"Show s&tash panel")                                                    \
    X(MenuRestoreAll, "menu.restore_all", L"Restore &all windows")                                               \
    X(MenuStartWithWindows, "menu.start_with_windows", L"Start with &Windows")                                   \
    X(MenuSettings, "menu.settings", L"S&ettings…")                                                         \
    X(MenuRunAsAdmin, "menu.run_as_admin", L"Restart as a&dministrator")                                         \
    X(MenuAbout, "menu.about", L"A&bout SnipPin")                                                                \
    X(MenuExit, "menu.exit", L"E&xit")                                                                           \
    X(MenuNone, "menu.none", L"(none)")                                                                          \
    X(MenuCloseAllSnippets, "menu.close_all_snippets", L"Close all snippets")                                    \
    /* ---- snippet menu and overlays ---- */                                                                    \
    X(SnippetLive, "snippet.live", L"&Live mirror")                                                              \
    X(SnippetSnapshot, "snippet.snapshot", L"&Frozen snapshot")                                                  \
    X(SnippetRefresh, "snippet.refresh", L"Re&fresh snapshot")                                                   \
    X(SnippetZoom100, "snippet.zoom_100", L"Actual si&ze (100%)")                                                \
    X(SnippetShowSource, "snippet.show_source", L"Show &source window")                                          \
    X(SnippetReselect, "snippet.reselect", L"Select a &different region…")                                  \
    X(SnippetClose, "snippet.close", L"&Close snippet")                                                          \
    X(SnippetTitle, "snippet.title", L"Snippet: %s")                                                             \
    X(SnippetSourceMinimized, "snippet.source_minimized", L"Source window is minimized")                         \
    X(SnippetSourceClosed, "snippet.source_closed", L"Source window was closed")                                 \
    X(SnippetNoContent, "snippet.no_content", L"Nothing to show")                                                \
    X(OverlayRegionHint, "overlay.region_hint",                                                                  \
      L"Drag to select a region  ·  Click for the whole window  ·  Esc to cancel")                     \
    X(OverlayPickerHint, "overlay.picker_hint", L"Click a window to choose it  ·  Esc to cancel")           \
    /* ---- notifications ---- */                                                                                \
    X(NotifyRunning, "notify.running",                                                                           \
      L"SnipPin is running in the notification area. Ctrl+right-click any window for options.")                  \
    X(NotifyAccessDenied, "notify.access_denied",                                                                \
      L"This window belongs to an administrator application. Use “Restart as administrator” to "      \
      L"manage it.")                                                                                             \
    X(NotifyUnsupported, "notify.unsupported",                                                                   \
      L"This window draws its own transparency, so its opacity cannot be changed. You can still pin or snip "  \
      L"it.")                                                                                                    \
    X(NotifyNotCandidate, "notify.not_candidate", L"This window cannot be pinned.")                              \
    X(NotifyHotkeyFailed, "notify.hotkey_failed", L"These keyboard shortcuts are used by another program: %s")  \
    X(NotifyRecovered, "notify.recovered", L"Restored windows left modified by a previous session.")             \
    X(NotifyRecallLaunching, "notify.recall_launching", L"Opening “%s”…")                         \
    X(NotifyRecallNotFound, "notify.recall_not_found", L"Could not find or open “%s”.")               \
    X(NotifyNoTarget, "notify.no_target", L"No window selected. Click a window first or use Pick a window.")     \
    X(NotifyRestored, "notify.restored", L"All windows were restored.")                                          \
    X(NotifyAutostartOn, "notify.autostart_on", L"SnipPin will start automatically when you sign in to Windows.")\
    X(NotifyAutostartOff, "notify.autostart_off", L"SnipPin will no longer start automatically.")                \
    X(NotifyAutostartFailed, "notify.autostart_failed", L"The startup setting could not be changed.")            \
    /* ---- stash panel ---- */                                                                                  \
    X(StashTitle, "stash.title", L"SnipPin – Stash")                                                        \
    X(StashColumnWindow, "stash.column_window", L"Window")                                                       \
    X(StashColumnState, "stash.column_state", L"State")                                                          \
    X(StashGroupPinned, "stash.group_pinned", L"Pinned now")                                                     \
    X(StashGroupSnippets, "stash.group_snippets", L"Snippets")                                                   \
    X(StashGroupRecent, "stash.group_recent", L"Recent")                                                         \
    X(StateTopmost, "state.topmost", L"On top")                                                                  \
    X(StateGhost, "state.ghost", L"Ghost")                                                                       \
    X(StateLive, "state.live", L"Live")                                                                          \
    X(StateSnapshot, "state.snapshot", L"Snapshot")                                                              \
    X(StateSnippetCount, "state.snippet_count", L"Snippets: %d")                                                 \
    /* ---- about ---- */                                                                                        \
    X(AboutFormat, "about.format",                                                                               \
      L"SnipPin %s\n\n%s\n\nMode: %s\nSettings: %s\n\nOpen source under the MIT License.\n"                      \
      L"https://github.com/rafsbo/SnipPin")                                                                      \
    X(AboutModeStandard, "about.mode_standard", L"Standard")                                                     \
    X(AboutModeAdmin, "about.mode_admin", L"Administrator")                                                      \
    X(AboutModeUiAccess, "about.mode_uiaccess", L"UIAccess (above other topmost windows)")                       \
    X(AboutPortable, "about.portable", L"%s (portable)")                                                         \
    /* ---- settings dialog ---- */                                                                              \
    X(SettingsTitle, "settings.title", L"SnipPin Settings")                                                      \
    X(SettingsGroupHotkeys, "settings.group_hotkeys", L"Keyboard shortcuts")                                     \
    X(SettingsHkTopmost, "settings.hk_topmost", L"Toggle always on top")                                         \
    X(SettingsHkPick, "settings.hk_pick", L"Pick a window")                                                      \
    X(SettingsHkOpacityDown, "settings.hk_opacity_down", L"Decrease opacity")                                    \
    X(SettingsHkOpacityUp, "settings.hk_opacity_up", L"Increase opacity")                                        \
    X(SettingsHkGhost, "settings.hk_ghost", L"Toggle ghost mode (click-through)")                                \
    X(SettingsHkSnip, "settings.hk_snip", L"Snip a region")                                                      \
    X(SettingsHkStash, "settings.hk_stash", L"Show / hide stash panel")                                          \
    X(SettingsHkPanic, "settings.hk_panic", L"Restore all windows (panic)")                                      \
    X(SettingsGroupGesture, "settings.group_gesture", L"Right-click menu")                                       \
    X(SettingsGestureEnabled, "settings.gesture_enabled",                                                        \
      L"Open the SnipPin menu for any window with a mouse gesture")                                              \
    X(SettingsGestureFormat, "settings.gesture_format", L"%s + right-click")                                     \
    X(SettingsGroupSnippets, "settings.group_snippets", L"Snippets")                                             \
    X(SettingsSnippetLive, "settings.snippet_live", L"New snippets are live mirrors (otherwise frozen snapshots)")\
    X(SettingsSnippetCompat, "settings.snippet_compat",                                                          \
      L"Compatibility rendering (use if live snippets stay empty)")                                              \
    X(SettingsGroupGeneral, "settings.group_general", L"General")                                                \
    X(SettingsLanguage, "settings.language", L"Language")                                                        \
    X(SettingsLanguageAuto, "settings.language_auto", L"Automatic (system language)")                            \
    X(SettingsRunAtStartup, "settings.run_at_startup", L"Start SnipPin automatically when I sign in to Windows") \
    X(SettingsZOrderGuard, "settings.zorder_guard", L"Keep pinned windows above fullscreen apps (z-order guard)")\
    X(SettingsNotifications, "settings.notifications", L"Show informational notifications")                      \
    X(SettingsOpacityStep, "settings.opacity_step", L"Opacity step (%)")                                         \
    X(SettingsGhostOpacity, "settings.ghost_opacity", L"Ghost mode opacity (%)")                                 \
    X(SettingsHistorySize, "settings.history_size", L"History size (entries)")                                   \
    X(SettingsDefaults, "settings.defaults", L"Restore &defaults")                                               \
    X(SettingsOk, "settings.ok", L"OK")                                                                          \
    X(SettingsCancel, "settings.cancel", L"Cancel")

namespace snippin::i18n {

enum class StringId : int {
#define SNIPPIN_STRING_ENUM(id, key, english) id,
    SNIPPIN_STRING_TABLE(SNIPPIN_STRING_ENUM)
#undef SNIPPIN_STRING_ENUM
    Count
};

constexpr int kStringCount = static_cast<int>(StringId::Count);

// Translation key ("menu.exit") and built-in English text of an id.
const char* KeyOf(StringId id);
const wchar_t* EnglishOf(StringId id);
bool FindStringId(std::string_view key, StringId& out);

// Active UI text for |id| (translation or English fallback). The pointer
// stays valid until the next SetActiveCatalog / ResetToEnglish call.
const wchar_t* Tr(StringId id);

}  // namespace snippin::i18n
