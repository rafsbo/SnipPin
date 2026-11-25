// SnipPin entry point: single-instance handling, COM/common-controls setup,
// crash safety net and the message loop.
#include <objbase.h>

#include <cwchar>

#include "app/App.h"
#include "platform/ApiShim.h"
#include "platform/Win32.h"

namespace {

constexpr wchar_t kMutexName[] = L"Local\\SnipPin.SingleInstance.{6F1D3A52-6C2B-4E0B-9C61-3B8F0A1E7D24}";

snippin::App* g_app = nullptr;

LONG WINAPI CrashFilter(EXCEPTION_POINTERS*) {
    // Never leave other applications' windows ghosted or translucent. The
    // recovery journal covers the cases where even this cannot run.
    if (g_app) {
        g_app->EmergencyRestore();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

bool HasArgument(const wchar_t* commandLine, const wchar_t* argument) {
    return commandLine && std::wcsstr(commandLine, argument) != nullptr;
}

void ActivateExistingInstance() {
    const UINT activate = ::RegisterWindowMessageW(snippin::App::kActivateMessageName);
    if (HWND existing = ::FindWindowW(snippin::App::kHostClassName, nullptr)) {
        ::PostMessageW(existing, activate, 0, 0);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    snippin::api::Initialize();
    snippin::api::EnableBestDpiAwareness();

    // Single instance. "--replace" is used by "Restart as administrator":
    // wait for the previous instance to exit instead of bailing out.
    HANDLE mutex = ::CreateMutexW(nullptr, TRUE, kMutexName);
    const DWORD mutexError = ::GetLastError();
    if (!mutex || mutexError == ERROR_ALREADY_EXISTS) {
        if (mutex && HasArgument(commandLine, L"--replace")) {
            const DWORD wait = ::WaitForSingleObject(mutex, 10000);
            if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
                ::CloseHandle(mutex);
                return 0;
            }
        } else {
            // Mutex exists (or belongs to an elevated instance we cannot
            // open): bring the running instance's stash panel forward.
            ActivateExistingInstance();
            if (mutex) {
                ::CloseHandle(mutex);
            }
            return 0;
        }
    }

    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_HOTKEY_CLASS | ICC_WIN95_CLASSES;
    ::InitCommonControlsEx(&icc);

    int exitCode = 1;
    {
        snippin::App app;
        g_app = &app;
        ::SetUnhandledExceptionFilter(&CrashFilter);
        if (app.Initialize(instance, HasArgument(commandLine, L"--autostart"))) {
            exitCode = app.Run();
        }
        g_app = nullptr;
    }

    ::CoUninitialize();
    if (mutex) {
        ::ReleaseMutex(mutex);
        ::CloseHandle(mutex);
    }
    return exitCode;
}
