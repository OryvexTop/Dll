#include "hooks.h"
#include "clickgui.h"
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

static ULONGLONG g_LastTick = 0;

static LRESULT CALLBACK hk_WndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    if (g_ImGuiReady && g_Gui && g_Gui->visible) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, w, l))
            return true;
    }
    return CallWindowProc(o_WndProc, hWnd, msg, w, l);
}

static BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
    static bool init = false;
    if (!init) {
        HWND hwnd = WindowFromDC(hdc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.MouseDrawCursor = false;

        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 10.f;
        s.FrameRounding     = 6.f;
        s.GrabRounding      = 6.f;
        s.ScrollbarRounding = 6.f;
        s.WindowBorderSize  = 1.f;
        s.FrameBorderSize   = 0.f;
        s.WindowPadding     = ImVec2(14, 14);
        s.FramePadding      = ImVec2(10, 6);
        s.ItemSpacing       = ImVec2(8, 6);

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplOpenGL3_Init("#version 130");

        o_WndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)hk_WndProc);

        g_Gui = new ClickGUI();
        g_ImGuiReady = true;
        init = true;
    }

    if (g_ImGuiReady) {
        ULONGLONG now = GetTickCount64();
        float dt = g_LastTick ? (float)(now - g_LastTick) / 1000.f : 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        g_LastTick = now;

        ImGui::GetIO().MouseDrawCursor = g_Gui->visible;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

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
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
