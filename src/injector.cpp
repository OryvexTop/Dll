#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

static DWORD FindProcess(const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) }; DWORD pid = 0;
    if (Process32FirstW(s, &pe)) do {
        if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(s, &pe));
    CloseHandle(s); return pid;
}

static bool AlreadyInjected(DWORD pid, const wchar_t* dllName) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{ sizeof(me) }; bool found = false;
    if (Module32FirstW(s, &me)) do {
        if (!_wcsicmp(me.szModule, dllName)) { found = true; break; }
    } while (Module32NextW(s, &me));
    CloseHandle(s); return found;
}

static bool Inject(DWORD pid, const std::wstring& dllPath) {
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) return false;
    SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID r = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!r) { CloseHandle(h); return false; }
    WriteProcessMemory(h, r, dllPath.c_str(), bytes, nullptr);
    auto loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    HANDLE t = CreateRemoteThread(h, nullptr, 0, loadLib, r, 0, nullptr);
    if (!t) { VirtualFreeEx(h, r, 0, MEM_RELEASE); CloseHandle(h); return false; }
    WaitForSingleObject(t, 10000);
    DWORD ec = 0; GetExitCodeThread(t, &ec);
    CloseHandle(t); VirtualFreeEx(h, r, 0, MEM_RELEASE); CloseHandle(h);
    return ec != 0;
}

int wmain() {
    SetConsoleTitleW(L"MuvixoClient Auto-Injector");
    wchar_t exePath[MAX_PATH]{}; GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path dllPath = fs::path(exePath).parent_path() / L"MuvixoClient.dll";
    fs::path dllName = dllPath.filename();
    if (!fs::exists(dllPath)) {
        std::wcerr << L"[!] MuvixoClient.dll missing next to Injector.exe\n";
        system("pause"); return 1;
    }
    std::wcout << L"[*] MuvixoClient injector\n";
    std::wcout << L"[*] Waiting for javaw.exe (Minecraft)...\n";
    std::wcout << L"[*] RightShift = open menu  |  END = unload\n\n";

    DWORD lastPid = 0;
    while (true) {
        DWORD pid = FindProcess(L"javaw.exe");
        if (pid && pid != lastPid) {
            std::wcout << L"[+] Found javaw.exe PID " << pid << L"\n";
            Sleep(10000);
            if (AlreadyInjected(pid, dllName.c_str())) {
                std::wcout << L"[=] Already injected\n";
            } else if (Inject(pid, dllPath.wstring())) {
                std::wcout << L"[+] Injected! Press RightShift in-game.\n";
            } else {
                std::wcerr << L"[-] Injection failed (run as admin)\n";
            }
            lastPid = pid;
        } else if (!pid) {
            lastPid = 0;
        }
        Sleep(2000);
    }
}
