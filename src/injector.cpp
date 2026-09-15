// src/injector.cpp — Win32 GUI injector
#include <Windows.h>
#include <TlHelp32.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstdarg>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

#define ID_PROC_LIST    1001
#define ID_DLL_EDIT     1002
#define ID_BROWSE       1003
#define ID_INJECT       1004
#define ID_EJECT        1005
#define ID_REFRESH      1006
#define ID_AUTO         1007
#define ID_LOG          1008
#define ID_STATUS       1009
#define ID_TIMER_AUTO   2001
#define ID_TIMER_STATUS 2002

static HWND g_hList, g_hDllEdit, g_hBrowse, g_hInject, g_hEject,
            g_hRefresh, g_hAuto, g_hLog, g_hStatus;
static HFONT g_hFont, g_hFontBold;
static HBRUSH g_hLogBrush;
static std::vector<DWORD> g_pids;
static std::wstring g_dllPath;
static bool g_auto = false;
static DWORD g_lastInjectedPid = 0;

static void Log(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    SendMessageW(g_hLog, EM_SCROLLCARET, 0, 0);
}

static void SetStatus(const wchar_t* text) {
    SetWindowTextW(g_hStatus, text);
}

static bool IsElevated() {
    BOOL elev = FALSE;
    HANDLE tok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        TOKEN_ELEVATION te{};
        DWORD sz = sizeof(te);
        if (GetTokenInformation(tok, TokenElevation, &te, sz, &sz))
            elev = te.TokenIsElevated;
        CloseHandle(tok);
    }
    return elev != FALSE;
}

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path().wstring();
}

struct ProcEntry { DWORD pid; std::wstring exe; };

static std::vector<ProcEntry> EnumProcesses() {
    std::vector<ProcEntry> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            HWND w = nullptr;
            struct Ctx { DWORD pid; HWND* out; } ctx{ pe.th32ProcessID, &w };
            EnumWindows([](HWND h, LPARAM lp) -> BOOL {
                auto* c = (Ctx*)lp;
                DWORD pid = 0;
                GetWindowThreadProcessId(h, &pid);
                if (pid == c->pid && IsWindowVisible(h)) {
                    *c->out = h;
                    return FALSE;
                }
                return TRUE;
            }, (LPARAM)&ctx);
            if (w) out.push_back({ pe.th32ProcessID, pe.szExeFile });
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    std::sort(out.begin(), out.end(), [](const ProcEntry& a, const ProcEntry& b) {
        bool aJ = _wcsicmp(a.exe.c_str(), L"javaw.exe") == 0;
        bool bJ = _wcsicmp(b.exe.c_str(), L"javaw.exe") == 0;
        if (aJ != bJ) return aJ;
        return _wcsicmp(a.exe.c_str(), b.exe.c_str()) < 0;
    });
    return out;
}

static void RefreshProcessList() {
    int prevSel = (int)SendMessageW(g_hList, LB_GETCURSEL, 0, 0);
    DWORD prevPid = (prevSel >= 0 && prevSel < (int)g_pids.size())
                    ? g_pids[prevSel] : 0;
    SendMessageW(g_hList, LB_RESETCONTENT, 0, 0);
    g_pids.clear();

    auto procs = EnumProcesses();
    int toSel = -1;
    for (size_t i = 0; i < procs.size(); ++i) {
        wchar_t line[256];
        _snwprintf_s(line, _countof(line), _TRUNCATE,
                     L"%-32s  PID %5lu", procs[i].exe.c_str(), procs[i].pid);
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)line);
        g_pids.push_back(procs[i].pid);
        if (procs[i].pid == prevPid) toSel = (int)i;
    }
    if (toSel >= 0) SendMessageW(g_hList, LB_SETCURSEL, toSel, 0);
    SetStatus(L"Process list refreshed.");
}

static DWORD FindProcess(const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(s, &pe)) do {
        if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(s, &pe));
    CloseHandle(s);
    return pid;
}

static bool IsModuleLoaded(DWORD pid, const std::wstring& dllName) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{ sizeof(me) };
    bool found = false;
    if (Module32FirstW(s, &me)) do {
        if (!_wcsicmp(me.szModule, dllName.c_str())) { found = true; break; }
    } while (Module32NextW(s, &me));
    CloseHandle(s);
    return found;
}

static bool InjectDLL(DWORD pid, const std::wstring& dllPath) {
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) return false;
    SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID r = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!r) { CloseHandle(h); return false; }
    WriteProcessMemory(h, r, dllPath.c_str(), bytes, nullptr);
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    auto loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "LoadLibraryW");
    HANDLE t = CreateRemoteThread(h, nullptr, 0, loadLib, r, 0, nullptr);
    if (!t) { VirtualFreeEx(h, r, 0, MEM_RELEASE); CloseHandle(h); return false; }
    WaitForSingleObject(t, 10000);
    DWORD ec = 0;
    GetExitCodeThread(t, &ec);
    CloseHandle(t);
    VirtualFreeEx(h, r, 0, MEM_RELEASE);
    CloseHandle(h);
    return ec != 0;
}

static bool EjectDLL(DWORD pid, const std::wstring& dllName) {
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) return false;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) { CloseHandle(h); return false; }
    MODULEENTRY32W me{ sizeof(me) };
    uintptr_t remoteBase = 0;
    if (Module32FirstW(s, &me)) do {
        if (!_wcsicmp(me.szModule, dllName.c_str())) {
            remoteBase = (uintptr_t)me.modBaseAddr;
            break;
        }
    } while (Module32NextW(s, &me));
    CloseHandle(s);
    if (!remoteBase) { CloseHandle(h); return false; }
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC freeLib = GetProcAddress(k32, "FreeLibrary");
    HANDLE t = CreateRemoteThread(h, nullptr, 0,
        (LPTHREAD_START_ROUTINE)freeLib, (LPVOID)remoteBase, 0, nullptr);
    if (!t) { CloseHandle(h); return false; }
    WaitForSingleObject(t, 5000);
    CloseHandle(t);
    CloseHandle(h);
    return true;
}

static DWORD GetSelectedPid() {
    int sel = (int)SendMessageW(g_hList, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)g_pids.size()) return 0;
    return g_pids[sel];
}

static void OnInject(bool silent = false) {
    DWORD pid = GetSelectedPid();
    if (!pid) { if (!silent) Log(L"[!] Select a process first."); return; }
    if (!fs::exists(g_dllPath)) { Log(L"[!] DLL not found: %s", g_dllPath.c_str()); return; }
    std::wstring dllName = fs::path(g_dllPath).filename().wstring();
    if (IsModuleLoaded(pid, dllName)) {
        if (!silent) Log(L"[=] Already injected in PID %lu", pid);
        return;
    }
    if (InjectDLL(pid, g_dllPath)) {
        Log(L"[+] Injected into PID %lu", pid);
        SetStatus(L"Injected.");
        g_lastInjectedPid = pid;
    } else {
        Log(L"[-] Injection failed. Run as admin.");
        SetStatus(L"Injection failed.");
    }
}

static void OnEject() {
    DWORD pid = GetSelectedPid();
    if (!pid) { Log(L"[!] Select a process first."); return; }
    std::wstring dllName = fs::path(g_dllPath).filename().wstring();
    if (!IsModuleLoaded(pid, dllName)) { Log(L"[=] Not injected in PID %lu", pid); return; }
    if (EjectDLL(pid, dllName)) {
        Log(L"[+] Ejected from PID %lu", pid);
        SetStatus(L"Ejected.");
    } else {
        Log(L"[-] Eject failed.");
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        g_hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hFontBold = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_hLogBrush = CreateSolidBrush(RGB(18, 20, 26));

        HWND hTitle = CreateWindowW(L"STATIC", L"Muvixo Injector",
            WS_CHILD | WS_VISIBLE, 18, 14, 400, 24, hWnd, nullptr, hi, nullptr);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        g_hStatus = CreateWindowW(L"STATIC", L"Idle",
            WS_CHILD | WS_VISIBLE | SS_RIGHT, 250, 18, 240, 20,
            hWnd, (HMENU)ID_STATUS, hi, nullptr);
        SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        HWND hLbl1 = CreateWindowW(L"STATIC", L"Target process",
            WS_CHILD | WS_VISIBLE, 18, 46, 200, 18, hWnd, nullptr, hi, nullptr);
        SendMessageW(hLbl1, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            18, 66, 480, 180, hWnd, (HMENU)ID_PROC_LIST, hi, nullptr);
        SendMessageW(g_hList, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        HWND hLbl2 = CreateWindowW(L"STATIC", L"DLL to inject",
            WS_CHILD | WS_VISIBLE, 18, 258, 200, 18, hWnd, nullptr, hi, nullptr);
        SendMessageW(hLbl2, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hDllEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            18, 278, 380, 26, hWnd, (HMENU)ID_DLL_EDIT, hi, nullptr);
        SendMessageW(g_hDllEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hBrowse = CreateWindowW(L"BUTTON", L"Browse...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            404, 278, 94, 26, hWnd, (HMENU)ID_BROWSE, hi, nullptr);
        SendMessageW(g_hBrowse, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hRefresh = CreateWindowW(L"BUTTON", L"Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            18, 318, 100, 30, hWnd, (HMENU)ID_REFRESH, hi, nullptr);
        SendMessageW(g_hRefresh, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hAuto = CreateWindowW(L"BUTTON", L"Auto-inject on launch",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            130, 322, 200, 24, hWnd, (HMENU)ID_AUTO, hi, nullptr);
        SendMessageW(g_hAuto, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hInject = CreateWindowW(L"BUTTON", L"Inject",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            336, 316, 80, 32, hWnd, (HMENU)ID_INJECT, hi, nullptr);
        SendMessageW(g_hInject, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hEject = CreateWindowW(L"BUTTON", L"Eject",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            422, 316, 76, 32, hWnd, (HMENU)ID_EJECT, hi, nullptr);
        SendMessageW(g_hEject, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        HWND hLbl3 = CreateWindowW(L"STATIC", L"Log",
            WS_CHILD | WS_VISIBLE, 18, 358, 200, 18, hWnd, nullptr, hi, nullptr);
        SendMessageW(hLbl3, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY,
            18, 378, 480, 150, hWnd, (HMENU)ID_LOG, hi, nullptr);
        SendMessageW(g_hLog, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        
        std::wstring def = ExeDir() + L"\\MuvixoClient.dll";
        g_dllPath = def;
        SetWindowTextW(g_hDllEdit, def.c_str());

        Log(L"[*] Muvixo Injector ready.");
        if (!IsElevated())
            Log(L"[!] Not running as admin — injection will fail for protected processes.");

        RefreshProcessList();

        SetTimer(hWnd, ID_TIMER_AUTO,   2000, nullptr);
        SetTimer(hWnd, ID_TIMER_STATUS, 1000, nullptr);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(230, 232, 240));
        SetBkColor(dc, RGB(28, 30, 38));
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(210, 220, 240));
        SetBkColor(dc, RGB(18, 20, 26));
        return (LRESULT)g_hLogBrush;
    }

    case WM_COMMAND: {
        switch (LOWORD(w)) {
        case ID_REFRESH: RefreshProcessList(); break;
        case ID_BROWSE: {
            OPENFILENAMEW ofn{};
            wchar_t file[MAX_PATH] = L"MuvixoClient.dll";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                g_dllPath = file;
                SetWindowTextW(g_hDllEdit, file);
                Log(L"[*] DLL set to: %s", file);
            }
            break;
        }
        case ID_INJECT: OnInject(); break;
        case ID_EJECT:  OnEject();  break;
        case ID_AUTO:
            g_auto = (SendMessageW(g_hAuto, BM_GETCHECK, 0, 0) == BST_CHECKED);
            Log(g_auto ? L"[*] Auto-inject enabled."
                       : L"[*] Auto-inject disabled.");
            break;
        case ID_PROC_LIST:
            if (HIWORD(w) == LBN_DBLCLK) OnInject();
            break;
        }
        return 0;
    }

    case WM_TIMER:
        if (w == ID_TIMER_AUTO && g_auto) {
            DWORD pid = FindProcess(L"javaw.exe");
            if (pid && pid != g_lastInjectedPid) {
                std::wstring dllName = fs::path(g_dllPath).filename().wstring();
                Sleep(8000);
                if (!IsModuleLoaded(pid, dllName)) {
                    Log(L"[*] Auto-detected javaw.exe PID %lu", pid);
                    for (size_t i = 0; i < g_pids.size(); ++i) {
                        if (g_pids[i] == pid) {
                            SendMessageW(g_hList, LB_SETCURSEL, i, 0);
                            break;
                        }
                    }
                    OnInject(true);
                }
                g_lastInjectedPid = pid;
            }
        } else if (w == ID_TIMER_STATUS) {
            DWORD pid = GetSelectedPid();
            if (pid) {
                std::wstring dllName = fs::path(g_dllPath).filename().wstring();
                wchar_t buf[128];
                _snwprintf_s(buf, _countof(buf), _TRUNCATE,
                    IsModuleLoaded(pid, dllName)
                        ? L"Injected (PID %lu)" : L"Ready (PID %lu)", pid);
                SetStatus(buf);
            } else {
                SetStatus(L"No process selected");
            }
        }
        return 0;

    case WM_CLOSE:   DestroyWindow(hWnd); return 0;
    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_AUTO);
        KillTimer(hWnd, ID_TIMER_STATUS);
        if (g_hFont)     DeleteObject(g_hFont);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hLogBrush) DeleteObject(g_hLogBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(28, 30, 38));
    wc.lpszClassName = L"MuvixoInjectorWnd";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int W = 540, H = 580;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - W) / 2;
    int y = (sh - H) / 2;

    HWND hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"Muvixo Injector v2",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, W, H, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
