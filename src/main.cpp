#include "app/Application.h"

#ifdef _WIN32
#  include <windows.h>

// ── Single-instance guard ─────────────────────────────────────────────────────
// Only one copy of this app may run at a time.  On Windows we use a named
// kernel mutex: if the mutex already exists another instance is running, so we
// bring that window to the foreground and exit immediately.
//
// The mutex handle is intentionally not stored — the OS releases it (and the
// mutex) automatically when the process exits, which allows the next launch to
// acquire it normally.

static constexpr wchar_t kInstanceMutex[] =
    L"StudioLogNDIMIDIBridge_SingleInstance";

static bool activateExistingInstance()
{
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, kInstanceMutex);
    if (!hMutex) return false;

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        // We are the first instance — keep the mutex open for our lifetime.
        return false;
    }

    // Another instance is already running.  Close our duplicate handle, then
    // find and restore its main window.
    CloseHandle(hMutex);

    HWND hWnd = FindWindowW(nullptr, L"StudioLog NDI MIDI Bridge");
    if (hWnd) {
        if (IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE);
        ShowWindow(hWnd, SW_SHOW);
        SetForegroundWindow(hWnd);
    }
    return true;
}
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    if (activateExistingInstance()) return 0;
#endif

    StudioLog::Application app(argc, argv);
    return app.exec();
}
