#include "app/App.h"

#include <shellapi.h>

#include <algorithm>
#include <cwchar>

#include "Version.h"
#include "app/AppInfo.h"
#include "i18n/Catalog.h"
#include "logic/Geometry.h"
#include "platform/ApiShim.h"
#include "platform/Localization.h"
#include "platform/Log.h"
#include "platform/WinUtil.h"
#include "resource.h"
#include "ui/SettingsDialog.h"

namespace snippin {

using i18n::StringId;
using i18n::Tr;
using i18n::TrFormat;
namespace {

// ---------------------------------------------------------------- messages
constexpr UINT kMsgTray = WM_APP + 1;
constexpr UINT kMsgContextGesture = WM_APP + 2;  // wParam = x, lParam = y
constexpr UINT kMsgSnippetWheel = WM_APP + 3;    // wParam = id, lParam = MAKELPARAM(delta, ctrl)
constexpr UINT kMsgCloseSnippet = WM_APP + 4;    // wParam = id
constexpr UINT_PTR kRecallTimer = 0x5B01;
constexpr UINT_PTR kTrayRetryTimer = 0x5B02;  // tray not ready yet (early autostart)
constexpr int kMaxTrayRetries = 30;

// ---------------------------------------------------------------- menu ids
// Per-window and per-snippet blocks of kBlockSize ids; the offset inside a
// block selects the action.
constexpr UINT kBlockSize = 32;
constexpr int kMaxMenuObjects = 40;

enum WindowAction : int {
    kWaTopmost = 0,
    kWaGhost,
    kWaSnip,
    kWaMirror,
    kWaRelease,
    kWaFocus,
    kWaOpacityFirst = 8,  // 8..17 -> 100 %, 90 %, ... 10 %
};

enum SnippetAction : int {
    kSaGhost = 0,
    kSaLive,
    kSaSnapshot,
    kSaRefresh,
    kSaZoom100,
    kSaFocusSource,
    kSaReselect,
    kSaClose,
    kSaOpacityFirst = 8,
};

constexpr UINT kIdPick = 100;
constexpr UINT kIdStash = 101;
constexpr UINT kIdRestoreAll = 102;
constexpr UINT kIdSettings = 103;
constexpr UINT kIdAdmin = 104;
constexpr UINT kIdAbout = 105;
constexpr UINT kIdExit = 106;
constexpr UINT kIdClearHistory = 107;
constexpr UINT kIdCloseAllSnippets = 108;
constexpr UINT kIdAutostart = 109;

constexpr UINT kDirectWindowBase = 1000;
constexpr UINT kDirectSnippetBase = 1100;
constexpr UINT kPinnedBase = 2000;
constexpr UINT kSnippetBase = 4000;
constexpr UINT kRecentBase = 6000;  // 2 ids per entry
constexpr UINT kRecentEnd = 6400;

bool InBlock(UINT id, UINT base, int count, int& index, int& action) {
    if (id < base || id >= base + kBlockSize * static_cast<UINT>(count)) {
        return false;
    }
    index = static_cast<int>((id - base) / kBlockSize);
    action = static_cast<int>((id - base) % kBlockSize);
    return true;
}

int OpacityForAction(int action, int first) { return 100 - (action - first) * 10; }

App* g_app = nullptr;

}  // namespace

App::App() : tracker_(journal_) { g_app = this; }

App::~App() {
    Shutdown();
    if (g_app == this) {
        g_app = nullptr;
    }
}

// ======================================================================
// Initialization
// ======================================================================

bool App::Initialize(HINSTANCE instance, bool autostart) {
    instance_ = instance;
    autostart_ = autostart;
    store_.Initialize();
    settings_ = store_.Load();
    log::Initialize(store_.FilePath(L"snippin.log"), settings_.logToFile);
    SNIPPIN_LOG(L"SnipPin %s starting (data: %s%s)", SNIPPIN_VERSION_WSTRING, store_.DataDirectory().c_str(),
                autostart ? L", started with Windows" : L"");

    activeLanguage_ = l10n::ApplyLanguage(instance, settings_.language);
    SettingsStore::RepairRunAtStartupPath();

    journal_.Initialize(store_.FilePath(L"journal.ini"));
    const int recovered = journal_.RestoreOrphans();

    stash_.Initialize(store_.FilePath(L"history.ini"), settings_.historySize);

    appIcon_ = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                               ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0));
    if (!appIcon_) {
        appIcon_ = ::LoadIconW(nullptr, IDI_APPLICATION);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::HostProc;
    wc.hInstance = instance;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.lpszClassName = kHostClassName;
    if (!::RegisterClassExW(&wc)) {
        SNIPPIN_LOG(L"RegisterClassExW(host) failed (error %lu)", ::GetLastError());
        return false;
    }
    // A hidden top-level window (not HWND_MESSAGE): message-only windows do
    // not receive the "TaskbarCreated" broadcast.
    host_ = ::CreateWindowExW(WS_EX_TOOLWINDOW, kHostClassName, kAppName, WS_POPUP, 0, 0, 0, 0, nullptr,
                              nullptr, instance, this);
    if (!host_) {
        SNIPPIN_LOG(L"CreateWindowExW(host) failed (error %lu)", ::GetLastError());
        return false;
    }

    msgTaskbarCreated_ = ::RegisterWindowMessageW(L"TaskbarCreated");
    msgActivate_ = ::RegisterWindowMessageW(kActivateMessageName);
    // Let lower-integrity processes (Explorer, a second SnipPin instance)
    // reach us when we run elevated.
    ::ChangeWindowMessageFilterEx(host_, msgTaskbarCreated_, MSGFLT_ALLOW, nullptr);
    ::ChangeWindowMessageFilterEx(host_, msgActivate_, MSGFLT_ALLOW, nullptr);

    tracker_.onChanged = [this](const TrackedWindow& tw) {
        stash_.Remember(tw.identity, tw.topmost, tw.opacityPercent, tw.ghost);
    };
    tracker_.onListChanged = [this] { RefreshUi(); };
    stash_.onChanged = [this] { RefreshUi(); };

    snippets_.Initialize(
        instance, host_, kMsgCloseSnippet, [this](SnippetWindow& s, POINT pt) { ShowSnippetMenu(s, pt); },
        [this] { RefreshUi(); });

    panel_.Create(
        instance, [this] { return BuildPanelRows(); }, [this](const PanelItemRef& r) { OnPanelActivate(r); },
        [this](const PanelItemRef& r, POINT pt) { OnPanelContextMenu(r, pt); });

    guard_.Start(
        host_, [this](DWORD event, HWND hwnd) { OnWindowEvent(event, hwnd); },
        [this](bool force) { OnReassert(force); });

    mouseHook_.Install(instance, host_, kMsgContextGesture, [this](POINT pt, int delta, bool ctrl) {
        // Runs inside the low-level hook: keep it cheap.
        SnippetWindow* s = snippets_.HitTest(pt);
        if (!s) {
            return false;
        }
        ::PostMessageW(host_, kMsgSnippetWheel, static_cast<WPARAM>(s->Id()),
                       MAKELPARAM(static_cast<WORD>(static_cast<short>(delta)), ctrl ? 1 : 0));
        return true;
    });

    if (!tray_.Add(host_, kMsgTray, appIcon_, kAppName)) {
        // At sign-in SnipPin can start before Explorer's notification area
        // exists. "TaskbarCreated" normally re-adds the icon; retry as well.
        SNIPPIN_LOG(L"Tray icon not added yet, retrying");
        ::SetTimer(host_, kTrayRetryTimer, 2000, nullptr);
    }
    ApplySettings();

    lastActive_ = win::GetTopLevel(::GetForegroundWindow());
    if (!win::IsCandidateWindow(lastActive_)) {
        lastActive_ = nullptr;
    }

    if (recovered > 0) {
        Notify(Tr(StringId::NotifyRecovered), false);
    } else if (!settings_.firstRunDone) {
        Notify(Tr(StringId::NotifyRunning), false);
    }
    if (!settings_.firstRunDone) {
        settings_.firstRunDone = true;
        store_.Save(settings_);
    }
    return true;
}

int App::Run() {
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    Shutdown();
    return static_cast<int>(msg.wParam);
}

void App::Shutdown() {
    if (shuttingDown_) {
        return;
    }
    shuttingDown_ = true;
    guard_.Stop();
    mouseHook_.Uninstall();
    hotkeys_.UnregisterAll();
    picker_.Cancel();
    selector_.Cancel();
    snippets_.CloseAll();
    tracker_.ReleaseAll();
    journal_.Clear();
    tray_.Remove();
    panel_.Destroy();
    if (host_) {
        HWND h = host_;
        host_ = nullptr;
        ::DestroyWindow(h);
    }
}

void App::EmergencyRestore() {
    // Only touch other processes' windows; avoid anything heavy.
    tracker_.ReleaseAll();
    journal_.Clear();
    tray_.Remove();
}

// ======================================================================
// Window procedure
// ======================================================================

LRESULT CALLBACK App::HostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->host_ = hwnd;
    } else {
        self = reinterpret_cast<App*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self || self->host_ != hwnd) {
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msgTaskbarCreated_ && msg == msgTaskbarCreated_) {
        tray_.Readd();
        return 0;
    }
    if (msgActivate_ && msg == msgActivate_) {
        panel_.Show();
        return 0;
    }

    switch (msg) {
        case WM_HOTKEY: {
            HotkeyAction action;
            if (HotkeyManager::ActionFromId(static_cast<int>(wParam), action)) {
                OnHotkey(action);
            }
            return 0;
        }
        case kMsgTray:
            switch (LOWORD(lParam)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    panel_.Toggle();
                    break;
                case WM_CONTEXTMENU:
                    ShowTrayMenu(POINT{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)});
                    break;
                default:
                    break;
            }
            return 0;
        case kMsgContextGesture:
            OnContextGesture(POINT{static_cast<LONG>(wParam), static_cast<LONG>(lParam)});
            return 0;
        case kMsgSnippetWheel:
            if (SnippetWindow* s = snippets_.FindById(static_cast<std::uint64_t>(wParam))) {
                s->OnWheel(static_cast<short>(LOWORD(lParam)), HIWORD(lParam) != 0, settings_.opacityStepPercent,
                           settings_.minOpacityPercent);
            }
            return 0;
        case kMsgCloseSnippet:
            snippets_.ProcessClose(static_cast<std::uint64_t>(wParam));
            return 0;
        case WM_TIMER:
            if (guard_.OnTimer(wParam)) {
                return 0;
            }
            if (wParam == kTrayRetryTimer) {
                if (tray_.Readd() || ++trayRetries_ >= kMaxTrayRetries) {
                    ::KillTimer(host_, kTrayRetryTimer);
                }
                return 0;
            }
            if (wParam == kRecallTimer) {
                if (!pendingRecall_.active) {
                    ::KillTimer(host_, kRecallTimer);
                    return 0;
                }
                if (HWND w = StashService::FindLiveWindow(pendingRecall_.entry)) {
                    ::KillTimer(host_, kRecallTimer);
                    pendingRecall_.active = false;
                    ApplyStashEntry(w, pendingRecall_.entry);
                } else if (::GetTickCount64() > pendingRecall_.deadline) {
                    ::KillTimer(host_, kRecallTimer);
                    pendingRecall_.active = false;
                    Notify(TrFormat(StringId::NotifyRecallNotFound, {pendingRecall_.entry.identity.title}), true);
                }
                return 0;
            }
            break;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam) {
                Shutdown();  // restore every window before Windows logs off
            }
            return 0;
        case WM_DWMCOMPOSITIONCHANGED:
            for (const auto& s : snippets_.All()) {
                s->OnCompositionChanged();
            }
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(host_, msg, wParam, lParam);
}

// ======================================================================
// Events from the z-order guard
// ======================================================================

void App::OnWindowEvent(DWORD event, HWND hwnd) {
    switch (event) {
        case EVENT_SYSTEM_FOREGROUND: {
            HWND root = win::GetTopLevel(hwnd);
            if (win::IsCandidateWindow(root)) {
                lastActive_ = root;
            }
            break;
        }
        case EVENT_OBJECT_DESTROY:
            if (tracker_.Find(hwnd)) {
                tracker_.Forget(hwnd);
            }
            snippets_.OnSourceDestroyed(hwnd);
            if (hwnd == lastActive_) {
                lastActive_ = nullptr;
            }
            break;
        default:
            break;
    }
}

void App::OnReassert(bool force) {
    // Never raise windows above an active picker/selector overlay or menu.
    if (picker_.IsActive() || selector_.IsActive() || inMenu_) {
        return;
    }
    tracker_.ReassertTopmost(force);
    if (force) {
        snippets_.ReassertTopmost();
    }
}

// ======================================================================
// Commands
// ======================================================================

HWND App::CurrentTarget() const {
    HWND fg = win::GetTopLevel(::GetForegroundWindow());
    if (win::IsCandidateWindow(fg)) {
        return fg;
    }
    if (lastActive_ && win::IsCandidateWindow(lastActive_)) {
        return lastActive_;
    }
    return nullptr;
}

void App::OnHotkey(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::ToggleTopmost:
            if (HWND target = CurrentTarget()) {
                ExecuteWindowAction(target, kWaTopmost);
            } else {
                Notify(Tr(StringId::NotifyNoTarget), true);
            }
            break;
        case HotkeyAction::PickWindow:
            picker_.Begin(instance_, [this](HWND picked, POINT pt) { ShowWindowMenu(picked, pt, false); });
            break;
        case HotkeyAction::OpacityDown:
            StepOpacitySmart(-1);
            break;
        case HotkeyAction::OpacityUp:
            StepOpacitySmart(+1);
            break;
        case HotkeyAction::ToggleGhost:
            ToggleGhostSmart();
            break;
        case HotkeyAction::SnipRegion:
            if (HWND target = CurrentTarget()) {
                StartSnip(target);
            } else {
                Notify(Tr(StringId::NotifyNoTarget), true);
            }
            break;
        case HotkeyAction::ToggleStash:
            panel_.Toggle();
            break;
        case HotkeyAction::PanicRestore:
            RestoreAll();
            break;
        default:
            break;
    }
}

void App::StepOpacitySmart(int direction) {
    const int delta = direction * settings_.opacityStepPercent;
    POINT cursor;
    ::GetCursorPos(&cursor);
    if (SnippetWindow* s = snippets_.HitTest(cursor)) {
        s->SetOpacity(s->OpacityPercent() + delta, settings_.minOpacityPercent);
        return;
    }
    HWND target = CurrentTarget();
    if (!target) {
        Notify(Tr(StringId::NotifyNoTarget), true);
        return;
    }
    ReportResult(tracker_.StepOpacity(target, delta, settings_.minOpacityPercent));
}

void App::ToggleGhostSmart() {
    // 1. The active window is a ghost -> bring it back.
    HWND target = CurrentTarget();
    if (const TrackedWindow* tw = target ? tracker_.Find(target) : nullptr; tw && tw->ghost) {
        ReportResult(tracker_.SetGhost(target, false, settings_.ghostOpacityPercent, settings_.minOpacityPercent));
        return;
    }
    // 2. Something is ghosted (it cannot be clicked any more) -> restore the
    //    most recently ghosted window or snippet.
    HWND ghostWindow = tracker_.MostRecentGhost();
    SnippetWindow* ghostSnippet = snippets_.MostRecentGhost();
    const std::uint64_t windowSeq = ghostWindow ? tracker_.Find(ghostWindow)->ghostSequence : 0;
    const std::uint64_t snippetSeq = ghostSnippet ? ghostSnippet->GhostSequence() : 0;
    if (ghostSnippet && snippetSeq > windowSeq) {
        ghostSnippet->SetGhost(false, settings_.ghostOpacityPercent, settings_.minOpacityPercent, 0);
        return;
    }
    if (ghostWindow) {
        ReportResult(tracker_.SetGhost(ghostWindow, false, settings_.ghostOpacityPercent, settings_.minOpacityPercent));
        return;
    }
    // 3. Nothing ghosted: ghost the snippet under the cursor or the active window.
    POINT cursor;
    ::GetCursorPos(&cursor);
    if (SnippetWindow* s = snippets_.HitTest(cursor)) {
        s->SetGhost(true, settings_.ghostOpacityPercent, settings_.minOpacityPercent, ++ghostSequence_);
        return;
    }
    if (!target) {
        Notify(Tr(StringId::NotifyNoTarget), true);
        return;
    }
    ReportResult(tracker_.SetGhost(target, true, settings_.ghostOpacityPercent, settings_.minOpacityPercent));
}

void App::ExecuteWindowAction(HWND target, int action) {
    if (!target || !::IsWindow(target)) {
        return;
    }
    if (action >= kWaOpacityFirst && action < kWaOpacityFirst + 10) {
        ReportResult(tracker_.SetOpacity(target, OpacityForAction(action, kWaOpacityFirst), settings_.minOpacityPercent));
        return;
    }
    const TrackedWindow* tw = tracker_.Find(target);
    switch (action) {
        case kWaTopmost:
            ReportResult(tracker_.ToggleTopmost(target));
            break;
        case kWaGhost:
            ReportResult(tracker_.SetGhost(target, !(tw && tw->ghost), settings_.ghostOpacityPercent,
                                           settings_.minOpacityPercent));
            break;
        case kWaSnip:
            StartSnip(target);
            break;
        case kWaMirror:
            CreateMirror(target);
            break;
        case kWaRelease:
            tracker_.Release(target);
            break;
        case kWaFocus:
            win::ForceForeground(target);
            break;
        default:
            break;
    }
}

void App::ExecuteSnippetAction(SnippetWindow& s, int action) {
    if (action >= kSaOpacityFirst && action < kSaOpacityFirst + 10) {
        s.SetOpacity(OpacityForAction(action, kSaOpacityFirst), settings_.minOpacityPercent);
        return;
    }
    switch (action) {
        case kSaGhost:
            s.SetGhost(!s.IsGhost(), settings_.ghostOpacityPercent, settings_.minOpacityPercent, ++ghostSequence_);
            break;
        case kSaLive:
            s.SetLive(true);
            break;
        case kSaSnapshot:
            s.SetLive(false);
            break;
        case kSaRefresh:
            s.RefreshSnapshot();
            break;
        case kSaZoom100:
            s.SetZoom(1.0);
            break;
        case kSaFocusSource:
            s.FocusSource();
            break;
        case kSaReselect: {
            HWND source = s.Source();
            const std::uint64_t id = s.Id();
            if (::IsWindow(source)) {
                selector_.Begin(instance_, source, [this, id](HWND target, logic::Rect region, logic::Rect screen) {
                    snippets_.RequestClose(id);
                    if (snippets_.Create(target, region, screen, settings_.snippetLiveByDefault)) {
                        stash_.RememberSnippet(win::GetIdentity(target), region);
                    }
                });
            }
            break;
        }
        case kSaClose:
            snippets_.RequestClose(s.Id());
            break;
        default:
            break;
    }
}

void App::StartSnip(HWND target) {
    if (!target || !::IsWindow(target)) {
        return;
    }
    picker_.Cancel();
    selector_.Begin(instance_, target, [this](HWND source, logic::Rect region, logic::Rect screen) {
        if (snippets_.Create(source, region, screen, settings_.snippetLiveByDefault)) {
            stash_.RememberSnippet(win::GetIdentity(source), region);
        }
    });
}

void App::CreateMirror(HWND target) {
    if (!target || !::IsWindow(target) || ::IsIconic(target)) {
        return;
    }
    RECT window;
    ::GetWindowRect(target, &window);
    const logic::Rect screen = ToRect(win::GetVisibleFrame(target));
    const logic::Rect region = logic::OffsetRect(screen, -window.left, -window.top);
    if (snippets_.Create(target, region, screen, true)) {
        stash_.RememberSnippet(win::GetIdentity(target), region);
    }
}

void App::RestoreAll() {
    picker_.Cancel();
    selector_.Cancel();
    tracker_.ReleaseAll();
    for (const auto& s : snippets_.All()) {
        if (s->IsGhost()) {
            s->SetGhost(false, settings_.ghostOpacityPercent, settings_.minOpacityPercent, 0);
        }
        if (s->OpacityPercent() < 100) {
            s->SetOpacity(100, settings_.minOpacityPercent);
        }
    }
    Notify(Tr(StringId::NotifyRestored), false);
}

void App::Recall(size_t recentIndex) {
    if (recentIndex >= stash_.Entries().size()) {
        return;
    }
    const StashEntry entry = stash_.Entries()[recentIndex];
    if (HWND live = StashService::FindLiveWindow(entry)) {
        ApplyStashEntry(live, entry);
        return;
    }
    const std::wstring& name = entry.identity.title.empty() ? entry.identity.exePath : entry.identity.title;
    if (!StashService::Launch(entry)) {
        Notify(TrFormat(StringId::NotifyRecallNotFound, {name}), true);
        return;
    }
    Notify(TrFormat(StringId::NotifyRecallLaunching, {name}), false);
    pendingRecall_.entry = entry;
    pendingRecall_.deadline = ::GetTickCount64() + 15000;
    pendingRecall_.active = true;
    ::SetTimer(host_, kRecallTimer, 500, nullptr);
}

void App::ApplyStashEntry(HWND hwnd, const StashEntry& entry) {
    if (!entry.ghost) {
        win::ForceForeground(hwnd);
    }
    if (entry.topmost) {
        ReportResult(tracker_.SetTopmost(hwnd, true));
    }
    if (entry.opacityPercent < 100) {
        tracker_.SetOpacity(hwnd, entry.opacityPercent, settings_.minOpacityPercent);
    }
    if (entry.ghost) {
        tracker_.SetGhost(hwnd, true, settings_.ghostOpacityPercent, settings_.minOpacityPercent);
    }
    if (!entry.snippetRegions.empty() && !::IsIconic(hwnd)) {
        RECT window;
        ::GetWindowRect(hwnd, &window);
        const logic::Rect bounds{0, 0, window.right - window.left, window.bottom - window.top};
        for (const auto& stored : entry.snippetRegions) {
            const logic::Rect region = logic::ClampRect(stored, bounds);
            if (region.IsEmpty()) {
                continue;
            }
            snippets_.Create(hwnd, region, logic::OffsetRect(region, window.left, window.top),
                             settings_.snippetLiveByDefault);
        }
    }
}

void App::OpenSettings() {
    static bool open = false;
    if (open) {
        return;
    }
    open = true;
    // Free the shortcuts so the hotkey controls can capture them.
    hotkeys_.UnregisterAll();
    SettingsDialogResult data;
    data.settings = settings_;
    const bool wasRunAtStartup = SettingsStore::IsRunAtStartupEnabled();
    data.runAtStartup = wasRunAtStartup;
    data.languages = l10n::AvailableLanguages(instance_);
    win::ForceForeground(host_);
    if (ShowSettingsDialog(instance_, nullptr, data)) {
        const bool languageChanged = data.settings.language != settings_.language;
        settings_ = data.settings;
        store_.Save(settings_);
        if (data.runAtStartup != wasRunAtStartup && !SettingsStore::SetRunAtStartup(data.runAtStartup)) {
            Notify(Tr(StringId::NotifyAutostartFailed), true);
        }
        if (languageChanged) {
            ApplyLanguageNow();
        }
    }
    ApplySettings();
    open = false;
}

void App::ToggleAutostart() {
    const bool enable = !SettingsStore::IsRunAtStartupEnabled();
    if (!SettingsStore::SetRunAtStartup(enable)) {
        Notify(Tr(StringId::NotifyAutostartFailed), true);
        return;
    }
    Notify(Tr(enable ? StringId::NotifyAutostartOn : StringId::NotifyAutostartOff), false);
}

void App::ApplyLanguageNow() {
    activeLanguage_ = l10n::ApplyLanguage(instance_, settings_.language);
    panel_.ApplyLanguage();  // menus and overlays read the catalogue when shown
}

void App::ApplySettings() {
    std::wstring failures;
    if (!hotkeys_.RegisterAll(host_, settings_, failures)) {
        Notify(TrFormat(StringId::NotifyHotkeyFailed, {failures}), true);
    }
    mouseHook_.Configure(settings_.contextMenuEnabled, settings_.contextMenuModifiers);
    guard_.SetEnabled(settings_.zOrderGuardEnabled, settings_.zOrderGuardIntervalMs);
    SnippetOptions options;
    options.compatibilityMode = settings_.snippetCompatibilityMode;
    options.fallbackIntervalMs = settings_.snippetFallbackIntervalMs;
    options.live = settings_.snippetLiveByDefault;
    options.opacityStepPercent = settings_.opacityStepPercent;
    options.minOpacityPercent = settings_.minOpacityPercent;
    snippets_.SetOptions(options);
    stash_.SetMaxEntries(settings_.historySize);
}

void App::RestartAsAdministrator() {
    const std::wstring exe = win::GetModulePath();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpParameters = L"--replace";
    info.nShow = SW_SHOWNORMAL;
    if (::ShellExecuteExW(&info)) {
        Exit();  // the elevated instance waits for our mutex to be released
    }
}

void App::ShowAbout() {
    std::wstring mode = Tr(StringId::AboutModeStandard);
    if (win::IsUiAccessProcess()) {
        mode = Tr(StringId::AboutModeUiAccess);
    } else if (win::IsProcessElevated()) {
        mode = Tr(StringId::AboutModeAdmin);
    }
    std::wstring data = store_.DataDirectory();
    if (store_.IsPortable()) {
        data = TrFormat(StringId::AboutPortable, {data});
    }
    const std::wstring text =
        TrFormat(StringId::AboutFormat, {SNIPPIN_VERSION_WSTRING, Tr(StringId::AppTagline), mode, data});
    win::ForceForeground(host_);
    ::MessageBoxW(host_, text.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}

void App::Exit() {
    Shutdown();
    ::PostQuitMessage(0);
}

// ======================================================================
// Menus
// ======================================================================

std::wstring App::ShortcutSuffix(HotkeyAction action) const {
    const logic::Hotkey& hk = settings_.hotkeys[static_cast<int>(action)];
    return hk.IsEnabled() ? L"\t" + logic::FormatHotkey(hk) : std::wstring();
}

std::wstring App::WindowLabel(HWND hwnd, size_t maxChars) const {
    std::wstring title = win::GetWindowTextString(hwnd);
    if (title.empty()) {
        title = win::GetClassNameString(hwnd);
    }
    return win::EscapeMenuText(win::Ellipsize(title, maxChars));
}

UINT App::TrackMenu(HMENU menu, POINT pt) {
    // The owner must be foreground or the menu will not close on outside
    // clicks (documented TrackPopupMenu behaviour).
    inMenu_ = true;
    win::ForceForeground(host_);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY;
    flags |= ::GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const UINT id = static_cast<UINT>(::TrackPopupMenuEx(menu, flags, pt.x, pt.y, host_, nullptr));
    ::PostMessageW(host_, WM_NULL, 0, 0);
    inMenu_ = false;
    return id;
}

void App::AppendOpacityMenu(HMENU parent, UINT base, int currentPercent) {
    HMENU sub = ::CreatePopupMenu();
    for (int i = 0; i < 10; ++i) {
        const int percent = 100 - i * 10;
        if (percent < settings_.minOpacityPercent) {
            break;
        }
        const std::wstring label = std::to_wstring(percent) + L" %";
        ::AppendMenuW(sub, MF_STRING, base + static_cast<UINT>(i), label.c_str());
    }
    // Radio mark on the closest step.
    const int closest = std::clamp((100 - currentPercent + 5) / 10, 0, 9);
    ::CheckMenuRadioItem(sub, base, base + 9, base + static_cast<UINT>(closest), MF_BYCOMMAND);
    std::wstring label = Tr(StringId::MenuOpacity);
    label += L" (" + std::to_wstring(currentPercent) + L" %)";
    ::AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), label.c_str());
}

void App::AppendWindowActions(HMENU menu, UINT base, HWND target, bool withShortcuts) {
    const TrackedWindow* tw = tracker_.Find(target);
    auto suffix = [&](HotkeyAction a) { return withShortcuts ? ShortcutSuffix(a) : std::wstring(); };

    ::AppendMenuW(menu, MF_STRING | ((tw && tw->topmost) ? MF_CHECKED : 0), base + kWaTopmost,
                  (std::wstring(Tr(StringId::MenuAlwaysOnTop)) + suffix(HotkeyAction::ToggleTopmost)).c_str());
    AppendOpacityMenu(menu, base + kWaOpacityFirst, tw ? tw->opacityPercent : 100);
    ::AppendMenuW(menu, MF_STRING | ((tw && tw->ghost) ? MF_CHECKED : 0), base + kWaGhost,
                  (std::wstring(Tr(StringId::MenuGhost)) + suffix(HotkeyAction::ToggleGhost)).c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, base + kWaSnip,
                  (std::wstring(Tr(StringId::MenuSnipRegion)) + suffix(HotkeyAction::SnipRegion)).c_str());
    ::AppendMenuW(menu, MF_STRING, base + kWaMirror, Tr(StringId::MenuMirrorWindow));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, base + kWaFocus, Tr(StringId::MenuBringToFront));
    ::AppendMenuW(menu, MF_STRING | (tw ? 0 : MF_GRAYED), base + kWaRelease, Tr(StringId::MenuRestoreWindow));
}

void App::AppendSnippetActions(HMENU menu, UINT base, const SnippetWindow& s) {
    ::AppendMenuW(menu, MF_STRING, base + kSaLive, Tr(StringId::SnippetLive));
    ::AppendMenuW(menu, MF_STRING, base + kSaSnapshot, Tr(StringId::SnippetSnapshot));
    ::CheckMenuRadioItem(menu, base + kSaLive, base + kSaSnapshot, base + (s.IsLive() ? kSaLive : kSaSnapshot),
                         MF_BYCOMMAND);
    ::AppendMenuW(menu, MF_STRING | ((s.IsLive() || s.IsSourceClosed()) ? MF_GRAYED : 0), base + kSaRefresh,
                  Tr(StringId::SnippetRefresh));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendOpacityMenu(menu, base + kSaOpacityFirst, s.OpacityPercent());
    ::AppendMenuW(menu, MF_STRING | (s.IsGhost() ? MF_CHECKED : 0), base + kSaGhost, Tr(StringId::MenuGhost));
    ::AppendMenuW(menu, MF_STRING, base + kSaZoom100, Tr(StringId::SnippetZoom100));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING | (s.IsSourceClosed() ? MF_GRAYED : 0), base + kSaFocusSource,
                  Tr(StringId::SnippetShowSource));
    ::AppendMenuW(menu, MF_STRING | (s.IsSourceClosed() ? MF_GRAYED : 0), base + kSaReselect, Tr(StringId::SnippetReselect));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, base + kSaClose, Tr(StringId::SnippetClose));
}

void App::AppendRecentMenu(HMENU parent) {
    HMENU recent = ::CreatePopupMenu();
    const auto& entries = stash_.Entries();
    if (entries.empty()) {
        ::AppendMenuW(recent, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuNone));
    }
    for (size_t i = 0; i < entries.size() && kRecentBase + i * 2 + 1 < kRecentEnd; ++i) {
        const auto& id = entries[i].identity;
        std::wstring label = id.title.empty() ? id.exePath : id.title;
        label = win::EscapeMenuText(win::Ellipsize(label, 48));
        HMENU item = ::CreatePopupMenu();
        ::AppendMenuW(item, MF_STRING, kRecentBase + static_cast<UINT>(i * 2), Tr(StringId::MenuRecall));
        ::AppendMenuW(item, MF_STRING, kRecentBase + static_cast<UINT>(i * 2 + 1), Tr(StringId::MenuRemoveFromHistory));
        ::AppendMenuW(recent, MF_POPUP, reinterpret_cast<UINT_PTR>(item), label.c_str());
    }
    if (!entries.empty()) {
        ::AppendMenuW(recent, MF_SEPARATOR, 0, nullptr);
        ::AppendMenuW(recent, MF_STRING, kIdClearHistory, Tr(StringId::MenuClearHistory));
    }
    ::AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(recent), Tr(StringId::MenuRecent));
}

void App::AppendGlobalItems(HMENU menu) {
    ::AppendMenuW(menu, MF_STRING, kIdPick, (std::wstring(Tr(StringId::MenuPickWindow)) + ShortcutSuffix(HotkeyAction::PickWindow)).c_str());
    ::AppendMenuW(menu, MF_STRING | (panel_.IsVisible() ? MF_CHECKED : 0), kIdStash,
                  (std::wstring(Tr(StringId::MenuShowStash)) + ShortcutSuffix(HotkeyAction::ToggleStash)).c_str());
    ::AppendMenuW(menu, MF_STRING, kIdRestoreAll,
                  (std::wstring(Tr(StringId::MenuRestoreAll)) + ShortcutSuffix(HotkeyAction::PanicRestore)).c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kIdSettings, Tr(StringId::MenuSettings));
    if (!win::IsProcessElevated() && !win::IsUiAccessProcess()) {
        ::AppendMenuW(menu, MF_STRING, kIdAdmin, Tr(StringId::MenuRunAsAdmin));
    }
    ::AppendMenuW(menu, MF_STRING, kIdAbout, Tr(StringId::MenuAbout));
    ::AppendMenuW(menu, MF_STRING, kIdExit, Tr(StringId::MenuExit));
}

void App::ShowTrayMenu(POINT pt) {
    if (inMenu_) {
        return;
    }
    HMENU menu = ::CreatePopupMenu();
    menuPinned_.clear();
    menuSnippets_.clear();

    // Last active window (the tray click itself activates the taskbar).
    HWND last = (lastActive_ && win::IsCandidateWindow(lastActive_)) ? lastActive_ : nullptr;
    if (last) {
        HMENU sub = ::CreatePopupMenu();
        AppendWindowActions(sub, kDirectWindowBase, last, true);
        const std::wstring label = std::wstring(Tr(StringId::MenuLastActive)) + L": " + WindowLabel(last, 40);
        ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), label.c_str());
    }

    // Pinned windows.
    HMENU pinned = ::CreatePopupMenu();
    std::vector<TrackedWindow> windows = tracker_.Windows();
    std::sort(windows.begin(), windows.end(),
              [](const TrackedWindow& a, const TrackedWindow& b) { return a.pinSequence < b.pinSequence; });
    for (const auto& tw : windows) {
        if (static_cast<int>(menuPinned_.size()) >= kMaxMenuObjects) {
            break;
        }
        HMENU sub = ::CreatePopupMenu();
        AppendWindowActions(sub, kPinnedBase + static_cast<UINT>(menuPinned_.size()) * kBlockSize, tw.hwnd, false);
        menuPinned_.push_back(tw.hwnd);
        std::wstring label = WindowLabel(tw.hwnd);
        if (tw.ghost) {
            label += L"  [" + std::wstring(Tr(StringId::StateGhost)) + L"]";
        }
        ::AppendMenuW(pinned, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), label.c_str());
    }
    if (menuPinned_.empty()) {
        ::AppendMenuW(pinned, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuNone));
    }
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(pinned), Tr(StringId::MenuPinnedWindows));

    // Snippets.
    HMENU snippetsMenu = ::CreatePopupMenu();
    for (const auto& s : snippets_.All()) {
        if (static_cast<int>(menuSnippets_.size()) >= kMaxMenuObjects) {
            break;
        }
        HMENU sub = ::CreatePopupMenu();
        AppendSnippetActions(sub, kSnippetBase + static_cast<UINT>(menuSnippets_.size()) * kBlockSize, *s);
        menuSnippets_.push_back(s->Id());
        std::wstring label = win::EscapeMenuText(win::Ellipsize(s->SourceTitle(), 40));
        if (s->IsGhost()) {
            label += L"  [" + std::wstring(Tr(StringId::StateGhost)) + L"]";
        }
        ::AppendMenuW(snippetsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), label.c_str());
    }
    if (menuSnippets_.empty()) {
        ::AppendMenuW(snippetsMenu, MF_STRING | MF_GRAYED, 0, Tr(StringId::MenuNone));
    } else {
        ::AppendMenuW(snippetsMenu, MF_SEPARATOR, 0, nullptr);
        ::AppendMenuW(snippetsMenu, MF_STRING, kIdCloseAllSnippets, Tr(StringId::MenuCloseAllSnippets));
    }
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(snippetsMenu), Tr(StringId::MenuSnippets));

    AppendRecentMenu(menu);
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING | (SettingsStore::IsRunAtStartupEnabled() ? MF_CHECKED : 0), kIdAutostart,
                  Tr(StringId::MenuStartWithWindows));
    AppendGlobalItems(menu);

    const UINT id = TrackMenu(menu, pt);
    ::DestroyMenu(menu);
    if (id) {
        DispatchMenuCommand(id, last, 0);
    }
}

void App::ShowWindowMenu(HWND target, POINT pt, bool fromGesture) {
    if (inMenu_ || !target || !::IsWindow(target)) {
        return;
    }
    menuPinned_.clear();
    menuSnippets_.clear();
    HMENU menu = ::CreatePopupMenu();
    ::AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, WindowLabel(target, 56).c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendWindowActions(menu, kDirectWindowBase, target, true);
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendRecentMenu(menu);
    AppendGlobalItems(menu);

    const UINT id = TrackMenu(menu, pt);
    ::DestroyMenu(menu);
    // Give focus back to the window the user right-clicked, unless they are
    // about to interact with something else.
    if (fromGesture && id != kIdSettings && id != kIdAbout && id != kIdStash && id != kIdPick &&
        !(id == kDirectWindowBase + kWaGhost) && ::IsWindow(target)) {
        win::ForceForeground(target);
    }
    if (id) {
        DispatchMenuCommand(id, target, 0);
    }
}

void App::ShowSnippetMenu(SnippetWindow& snippet, POINT pt) {
    if (inMenu_) {
        return;
    }
    const std::uint64_t id = snippet.Id();
    HMENU menu = ::CreatePopupMenu();
    const std::wstring header =
        TrFormat(StringId::SnippetTitle, {win::EscapeMenuText(win::Ellipsize(snippet.SourceTitle(), 48))});
    ::AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, header.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendSnippetActions(menu, kDirectSnippetBase, snippet);
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendGlobalItems(menu);
    const UINT cmd = TrackMenu(menu, pt);
    ::DestroyMenu(menu);
    // The snippet may have been closed while the menu was open.
    if (cmd && snippets_.FindById(id)) {
        DispatchMenuCommand(cmd, nullptr, id);
    } else if (cmd) {
        DispatchMenuCommand(cmd, nullptr, 0);
    }
}

void App::ShowRecentMenu(size_t recentIndex, POINT pt) {
    if (inMenu_ || recentIndex >= stash_.Entries().size()) {
        return;
    }
    HMENU menu = ::CreatePopupMenu();
    ::AppendMenuW(menu, MF_STRING, kRecentBase + static_cast<UINT>(recentIndex * 2), Tr(StringId::MenuRecall));
    ::AppendMenuW(menu, MF_STRING, kRecentBase + static_cast<UINT>(recentIndex * 2 + 1), Tr(StringId::MenuRemoveFromHistory));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kIdClearHistory, Tr(StringId::MenuClearHistory));
    const UINT id = TrackMenu(menu, pt);
    ::DestroyMenu(menu);
    if (id) {
        DispatchMenuCommand(id, nullptr, 0);
    }
}

void App::OnContextGesture(POINT pt) {
    // Ghosted (click-through) objects are invisible to WindowFromPoint, but
    // the gesture is the natural way to get them back: check them first.
    for (auto it = snippets_.All().rbegin(); it != snippets_.All().rend(); ++it) {
        RECT rc;
        if ((*it)->IsGhost() && ::GetWindowRect((*it)->Hwnd(), &rc) && ::PtInRect(&rc, pt)) {
            ShowSnippetMenu(**it, pt);
            return;
        }
    }
    const TrackedWindow* bestGhost = nullptr;
    for (const auto& tw : tracker_.Windows()) {
        if (!tw.ghost) {
            continue;
        }
        const RECT rc = win::GetVisibleFrame(tw.hwnd);
        if (::PtInRect(&rc, pt) && (!bestGhost || tw.ghostSequence > bestGhost->ghostSequence)) {
            bestGhost = &tw;
        }
    }
    if (bestGhost) {
        ShowWindowMenu(bestGhost->hwnd, pt, true);
        return;
    }

    HWND root = win::GetTopLevel(::WindowFromPoint(pt));
    if (SnippetWindow* s = snippets_.FindByHwnd(root)) {
        ShowSnippetMenu(*s, pt);
    } else if (win::IsCandidateWindow(root)) {
        ShowWindowMenu(root, pt, true);
    } else if (!win::IsOwnWindow(root)) {
        ShowTrayMenu(pt);  // desktop / taskbar: global menu
    }
}

bool App::DispatchMenuCommand(UINT id, HWND directWindow, std::uint64_t directSnippet) {
    int index = 0;
    int action = 0;
    if (InBlock(id, kDirectWindowBase, 1, index, action)) {
        ExecuteWindowAction(directWindow, action);
        return true;
    }
    if (InBlock(id, kDirectSnippetBase, 1, index, action)) {
        if (SnippetWindow* s = snippets_.FindById(directSnippet)) {
            ExecuteSnippetAction(*s, action);
        }
        return true;
    }
    if (InBlock(id, kPinnedBase, kMaxMenuObjects, index, action)) {
        if (index < static_cast<int>(menuPinned_.size())) {
            ExecuteWindowAction(menuPinned_[index], action);
        }
        return true;
    }
    if (InBlock(id, kSnippetBase, kMaxMenuObjects, index, action)) {
        if (index < static_cast<int>(menuSnippets_.size())) {
            if (SnippetWindow* s = snippets_.FindById(menuSnippets_[index])) {
                ExecuteSnippetAction(*s, action);
            }
        }
        return true;
    }
    if (id >= kRecentBase && id < kRecentEnd) {
        const size_t entry = (id - kRecentBase) / 2;
        if ((id - kRecentBase) % 2 == 0) {
            Recall(entry);
        } else {
            stash_.Remove(entry);
        }
        return true;
    }
    switch (id) {
        case kIdPick:
            OnHotkey(HotkeyAction::PickWindow);
            return true;
        case kIdStash:
            panel_.Toggle();
            return true;
        case kIdRestoreAll:
            RestoreAll();
            return true;
        case kIdSettings:
            OpenSettings();
            return true;
        case kIdAdmin:
            RestartAsAdministrator();
            return true;
        case kIdAbout:
            ShowAbout();
            return true;
        case kIdExit:
            Exit();
            return true;
        case kIdAutostart:
            ToggleAutostart();
            return true;
        case kIdClearHistory:
            stash_.Clear();
            return true;
        case kIdCloseAllSnippets:
            for (const auto& s : snippets_.All()) {
                snippets_.RequestClose(s->Id());
            }
            return true;
        default:
            return false;
    }
}

// ======================================================================
// Stash panel
// ======================================================================

std::vector<PanelRow> App::BuildPanelRows() {
    std::vector<PanelRow> rows;
    std::vector<TrackedWindow> windows = tracker_.Windows();
    std::sort(windows.begin(), windows.end(),
              [](const TrackedWindow& a, const TrackedWindow& b) { return a.pinSequence < b.pinSequence; });
    for (const auto& tw : windows) {
        PanelRow row;
        row.ref.group = PanelGroup::Pinned;
        row.ref.hwnd = tw.hwnd;
        row.title = win::GetWindowTextString(tw.hwnd);
        if (row.title.empty()) {
            row.title = tw.identity.title;
        }
        std::wstring state;
        if (tw.topmost) state = Tr(StringId::StateTopmost);
        if (tw.opacityPercent < 100) state += (state.empty() ? L"" : L" \u00B7 ") + std::to_wstring(tw.opacityPercent) + L"%";
        if (tw.ghost) state += (state.empty() ? L"" : L" \u00B7 ") + std::wstring(Tr(StringId::StateGhost));
        row.state = state;
        row.icon = win::GetWindowSmallIcon(tw.hwnd);
        rows.push_back(std::move(row));
    }
    for (const auto& s : snippets_.All()) {
        PanelRow row;
        row.ref.group = PanelGroup::Snippets;
        row.ref.snippetId = s->Id();
        row.title = s->SourceTitle();
        std::wstring state = s->IsLive() ? Tr(StringId::StateLive) : Tr(StringId::StateSnapshot);
        if (s->OpacityPercent() < 100) state += L" \u00B7 " + std::to_wstring(s->OpacityPercent()) + L"%";
        if (s->IsGhost()) state += L" \u00B7 " + std::wstring(Tr(StringId::StateGhost));
        row.state = state;
        if (::IsWindow(s->Source())) {
            row.icon = win::GetWindowSmallIcon(s->Source());
        }
        rows.push_back(std::move(row));
    }
    const auto& entries = stash_.Entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        PanelRow row;
        row.ref.group = PanelGroup::Recent;
        row.ref.recentIndex = i;
        row.title = e.identity.title.empty() ? e.identity.exePath : e.identity.title;
        if (!e.snippetRegions.empty()) {
            row.state = TrFormat(StringId::StateSnippetCount, {std::to_wstring(e.snippetRegions.size())});
        }
        const std::wstring& iconSource = e.identity.folderPath.empty() ? e.identity.exePath : e.identity.folderPath;
        SHFILEINFOW info{};
        if (!iconSource.empty() &&
            ::SHGetFileInfoW(iconSource.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_SMALLICON)) {
            row.icon = info.hIcon;
            row.ownsIcon = true;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

void App::OnPanelActivate(const PanelItemRef& ref) {
    switch (ref.group) {
        case PanelGroup::Pinned:
            if (::IsWindow(ref.hwnd)) {
                win::ForceForeground(ref.hwnd);
            }
            break;
        case PanelGroup::Snippets:
            if (SnippetWindow* s = snippets_.FindById(ref.snippetId)) {
                s->FocusSource();
            }
            break;
        case PanelGroup::Recent:
            Recall(ref.recentIndex);
            break;
    }
}

void App::OnPanelContextMenu(const PanelItemRef& ref, POINT pt) {
    switch (ref.group) {
        case PanelGroup::Pinned:
            ShowWindowMenu(ref.hwnd, pt, false);
            break;
        case PanelGroup::Snippets:
            if (SnippetWindow* s = snippets_.FindById(ref.snippetId)) {
                ShowSnippetMenu(*s, pt);
            }
            break;
        case PanelGroup::Recent:
            ShowRecentMenu(ref.recentIndex, pt);
            break;
    }
}

void App::RefreshUi() {
    if (!shuttingDown_) {
        panel_.Refresh();
    }
}

// ======================================================================
// Feedback
// ======================================================================

void App::ReportResult(OpResult result) {
    switch (result) {
        case OpResult::AccessDenied:
            Notify(Tr(StringId::NotifyAccessDenied), true);
            break;
        case OpResult::Unsupported:
            Notify(Tr(StringId::NotifyUnsupported), true);
            break;
        case OpResult::NotCandidate:
            Notify(Tr(StringId::NotifyNotCandidate), true);
            break;
        case OpResult::Ok:
            break;
    }
}

void App::Notify(const std::wstring& text, bool warning) {
    if (!warning && !settings_.showNotifications) {
        return;
    }
    tray_.ShowBalloon(kAppName, text.c_str(), warning);
}

}  // namespace snippin
