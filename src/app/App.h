// Application host: hidden top-level window, message loop glue and the single
// command layer shared by hotkeys, the tray menu, the modifier+right-click
// menu, snippet menus and the stash panel.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/HotkeyManager.h"
#include "app/MouseHook.h"
#include "app/TrayIcon.h"
#include "core/RecoveryJournal.h"
#include "core/StashService.h"
#include "core/WindowTracker.h"
#include "core/ZOrderGuard.h"
#include "platform/Settings.h"
#include "snippet/RegionSelector.h"
#include "snippet/SnippetManager.h"
#include "ui/StashPanel.h"
#include "ui/WindowPicker.h"

namespace snippin {

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // |autostart| is true when Windows launched SnipPin at sign-in.
    bool Initialize(HINSTANCE instance, bool autostart);
    int Run();

    // Best-effort restore used by the unhandled-exception filter.
    void EmergencyRestore();

    static constexpr wchar_t kHostClassName[] = L"SnipPin.Host";
    static constexpr wchar_t kActivateMessageName[] = L"SnipPin.Activate";

private:
    // ----- window plumbing
    static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void Shutdown();

    // ----- commands
    void OnHotkey(HotkeyAction action);
    void ExecuteWindowAction(HWND target, int action);
    void ExecuteSnippetAction(SnippetWindow& snippet, int action);
    void StartSnip(HWND target);
    void CreateMirror(HWND target);
    void ToggleGhostSmart();
    void StepOpacitySmart(int direction);
    void RestoreAll();
    void Recall(size_t recentIndex);
    void ApplyStashEntry(HWND hwnd, const StashEntry& entry);
    void OpenSettings();
    void ToggleAutostart();
    void ApplyLanguageNow();
    void ApplySettings();
    void RestartAsAdministrator();
    void ShowAbout();
    void Exit();

    // ----- menus
    void ShowTrayMenu(POINT pt);
    void ShowWindowMenu(HWND target, POINT pt, bool fromGesture);
    void ShowSnippetMenu(SnippetWindow& snippet, POINT pt);
    void ShowRecentMenu(size_t recentIndex, POINT pt);
    void OnContextGesture(POINT pt);
    UINT TrackMenu(HMENU menu, POINT pt);
    void AppendWindowActions(HMENU menu, UINT base, HWND target, bool withShortcuts);
    void AppendSnippetActions(HMENU menu, UINT base, const SnippetWindow& snippet);
    void AppendOpacityMenu(HMENU parent, UINT base, int currentPercent);
    void AppendRecentMenu(HMENU parent);
    void AppendGlobalItems(HMENU menu);
    bool DispatchMenuCommand(UINT id, HWND directWindow, std::uint64_t directSnippet);
    std::wstring ShortcutSuffix(HotkeyAction action) const;

    // ----- helpers
    HWND CurrentTarget() const;
    void OnWindowEvent(DWORD event, HWND hwnd);
    void OnReassert(bool force);
    void ReportResult(OpResult result);
    void Notify(const std::wstring& text, bool warning);
    std::vector<PanelRow> BuildPanelRows();
    void OnPanelActivate(const PanelItemRef& ref);
    void OnPanelContextMenu(const PanelItemRef& ref, POINT pt);
    void RefreshUi();
    std::wstring WindowLabel(HWND hwnd, size_t maxChars = 48) const;

    HINSTANCE instance_ = nullptr;
    HWND host_ = nullptr;
    HICON appIcon_ = nullptr;
    UINT msgTaskbarCreated_ = 0;
    UINT msgActivate_ = 0;

    SettingsStore store_;
    AppSettings settings_;
    RecoveryJournal journal_;
    WindowTracker tracker_;
    ZOrderGuard guard_;
    StashService stash_;
    SnippetManager snippets_;
    RegionSelector selector_;
    WindowPicker picker_;
    StashPanel panel_;
    TrayIcon tray_;
    HotkeyManager hotkeys_;
    MouseHook mouseHook_;

    std::wstring activeLanguage_ = L"en";
    bool autostart_ = false;
    int trayRetries_ = 0;
    HWND lastActive_ = nullptr;
    std::uint64_t ghostSequence_ = 0;
    bool shuttingDown_ = false;
    bool inMenu_ = false;

    // Snapshot of the objects listed in the menu currently being shown.
    std::vector<HWND> menuPinned_;
    std::vector<std::uint64_t> menuSnippets_;

    struct PendingRecall {
        StashEntry entry;
        std::uint64_t deadline = 0;
        bool active = false;
    } pendingRecall_;
};

}  // namespace snippin
