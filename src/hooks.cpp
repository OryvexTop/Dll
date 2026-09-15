#include "hooks.h"
#include "clickgui.h"
#include "modules.h"
#include "altmanager.h"
#include <Windows.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <GL/gl.h>

bool g_ImGuiReady = false;
typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);
static wglSwapBuffers_t o_wglSwapBuffers = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static WNDPROC   o_WndProc = nullptr;
static HWND      g_hwnd    = nullptr;
static ULONGLONG g_LastTick = 0;

static LRESULT CALLBACK hk_WndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    bool guiOpen = g_ImGuiReady && g_Gui && (g_Gui->visible || g_Gui->altManagerOpen);
    if (guiOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, w, l)) return true;
        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEMOVE:   case WM_MOUSEWHEEL:
            case WM_KEYDOWN:     case WM_KEYUP:
            case WM_CHAR:        case WM_SETCURSOR:
            case WM_SYSKEYDOWN:  case WM_SYSKEYUP:
                return true;
        }
    }
    return CallWindowProc(o_WndProc, hWnd, msg, w, l);
}

static BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
    static bool init = false;
    if (!init) {
        g_hwnd = WindowFromDC(hdc);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.MouseDrawCursor = false;

        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 14.f;
        s.FrameRounding     = 8.f;
        s.GrabRounding      = 8.f;
        s.WindowBorderSize  = 1.f;
        s.FrameBorderSize   = 1.f;
        s.WindowPadding     = ImVec2(16, 16);

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplOpenGL3_Init("#version 130");
        o_WndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hk_WndProc);

        g_Gui    = new ClickGUI();
        g_AltMgr = new AltManager();
        g_AltMgr->Load();

        g_ImGuiReady = true;
        init = true;
    }

    if (g_ImGuiReady) {
        ULONGLONG now = GetTickCount64();
        float dt = g_LastTick ? (float)(now - g_LastTick) / 1000.f : 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        g_LastTick = now;

        bool anyOpen = g_Gui->visible || g_Gui->altManagerOpen;
        ImGui::GetIO().MouseDrawCursor = anyOpen;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        OnFrame(dt);
        g_Gui->Render(dt);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    return o_wglSwapBuffers(hdc);
}

bool InstallHooks() {
    if (MH_Initialize() != MH_OK) return false;
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) return false;
    void* target = (void*)GetProcAddress(gl, "wglSwapBuffers");
    if (!target) return false;
    if (MH_CreateHook(target, &hk_wglSwapBuffers,
                      reinterpret_cast<void**>(&o_wglSwapBuffers)) != MH_OK)
        return false;
    return MH_EnableHook(target) == MH_OK;
}

void UninstallHooks() {
    if (g_AltMgr) g_AltMgr->Save();
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
