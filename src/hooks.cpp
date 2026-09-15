#include "hooks.h"
#include "clickgui.h"
#include "modules.h"
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
static WNDPROC o_WndProc = nullptr;
static HWND    g_hwnd   = nullptr;
static ULONGLONG g_LastTick = 0;

// ── Mouse-lock fix ────────────────────────────────────────────────
// Vanilla Minecraft continuously re-centers the cursor (SetCursorPos)
// while the game window is focused. That fights ImGui. When the menu
// is open we clip the cursor so MC stops recentering it, and we also
// restore the real desktop cursor so the user can click.

static void UpdateMouseMode() {
    bool guiOpen = g_Gui && g_Gui->visible;
    if (guiOpen) {
        // Show OS cursor + clip it to the game window
        ImGui::GetIO().MouseDrawCursor = true;
        if (g_hwnd) {
            RECT r; GetClientRect(g_hwnd, &r);
            POINT tl{ r.left, r.top }, br{ r.right, r.bottom };
            ClientToScreen(g_hwnd, &tl);
            ClientToScreen(g_hwnd, &br);
            RECT clip{ tl.x, tl.y, br.x, br.y };
            ClipCursor(&clip);
        }
        // Stop MC from stealing focus for recenter by pushing a "no capture" hint.
        // ImGui_ImplWin32 uses WM_SETCURSOR / WM_MOUSEMOVE; the trick is to
        // let mouse messages reach ImGui while the GUI is open. That's handled
        // in WndProc below.
    } else {
        ImGui::GetIO().MouseDrawCursor = false;
        ClipCursor(nullptr);
    }
}

// ── WndProc: only swallow input when GUI is visible ───────────────
static LRESULT CALLBACK hk_WndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    if (g_ImGuiReady && g_Gui && g_Gui->visible) {
        // Eat mouse/keyboard messages so Minecraft doesn't also process them
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, w, l))
            return true;

        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
            case WM_KEYDOWN: case WM_KEYUP:
            case WM_CHAR:
            case WM_SETCURSOR:
                return true;   // ← stop MC from reacting to input
        }
    }
    return CallWindowProc(o_WndProc, hWnd, msg, w, l);
}

// ── Swap hook ─────────────────────────────────────────────────────
static BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
    static bool init = false;
    if (!init) {
        g_hwnd = WindowFromDC(hdc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.MouseDrawCursor = false;

        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 14.f;
        s.FrameRounding     = 8.f;
        s.GrabRounding      = 8.f;
        s.ScrollbarRounding = 8.f;
        s.WindowBorderSize  = 1.f;
        s.FrameBorderSize   = 1.f;
        s.WindowPadding     = ImVec2(16, 16);
        s.FramePadding      = ImVec2(12, 8);
        s.ItemSpacing       = ImVec2(8, 6);

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplOpenGL3_Init("#version 130");

        o_WndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hk_WndProc);

        g_Gui = new ClickGUI();
        g_ImGuiReady = true;
        init = true;
    }

    if (g_ImGuiReady) {
        ULONGLONG now = GetTickCount64();
        float dt = g_LastTick ? (float)(now - g_LastTick) / 1000.f : 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        g_LastTick = now;

        UpdateMouseMode();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        OnFrame(dt);          // modules HUD
        g_Gui->Render(dt);    // clickgui

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
    ClipCursor(nullptr);
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
