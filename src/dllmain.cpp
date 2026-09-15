#include <Windows.h>
#include "hooks.h"
#include "clickgui.h"

static HMODULE g_Self = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    while (!GetModuleHandleA("opengl32.dll")) Sleep(100);
    Sleep(2000);
    if (!InstallHooks()) {
        MessageBoxA(nullptr, "Failed to install hooks.", "MuvixoClient", MB_ICONERROR);
        FreeLibraryAndExitThread(g_Self, 1);
    }

    bool prevRs = false, prevAlt = false;
    while (true) {
        bool nowRs  = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
        bool nowAlt = (GetAsyncKeyState(VK_F6)     & 0x8000) != 0;

        if (nowRs  && !prevRs  && g_Gui) g_Gui->Toggle();
        if (nowAlt && !prevAlt && g_Gui) g_Gui->ToggleAlt();
        prevRs = nowRs; prevAlt = nowAlt;

        if (GetAsyncKeyState(VK_END) & 1) break;
        Sleep(25);
    }

    UninstallHooks();
    FreeLibraryAndExitThread(g_Self, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_Self = hMod;
        DisableThreadLibraryCalls(hMod);
        HANDLE h = CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
