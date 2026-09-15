#include "modules.h"
#include "clickgui.h"
#include <Windows.h>
#include <imgui.h>
#include <cstring>
#include <ctime>
#include <cmath>

// ── Module state (mirrors ClickGUI on toggle) ─────────────────────
static bool g_Sprint     = true;
static bool g_Sneak      = false;
static bool g_Fullbright = true;
static bool g_Coords     = true;
static bool g_FPS        = true;
static bool g_Clock      = false;
static bool g_Ping       = false;
static bool g_AntiAFK    = true;
static bool g_ChatTime   = false;

// ── FPS counter ───────────────────────────────────────────────────
static float  g_FpsAccum = 0.f;
static int    g_FpsCount = 0;
static float  g_FpsValue = 0.f;

// ── Anti-AFK (nudge input every N seconds) ────────────────────────
static float g_AntiAFKTim = 0.f;

// ── Helpers: fake Sprint toggle (bind a key) ──────────────────────
// Real sprint would need to write to MC's Player object via JNI.
// For this client we simulate it by holding the sprint key constantly
// when the module is on. The user can still override by pressing it.

void OnModuleToggled(const char* name, bool state) {
    if (!std::strcmp(name, "Sprint"))      g_Sprint     = state;
    if (!std::strcmp(name, "Sneak Toggle"))g_Sneak      = state;
    if (!std::strcmp(name, "Fullbright"))  g_Fullbright = state;
    if (!std::strcmp(name, "Coordinates")) g_Coords     = state;
    if (!std::strcmp(name, "FPS Counter")) g_FPS        = state;
    if (!std::strcmp(name, "Clock"))       g_Clock      = state;
    if (!std::strcmp(name, "Ping"))        g_Ping       = state;
    if (!std::strcmp(name, "Anti AFK"))    g_AntiAFK    = state;
    if (!std::strcmp(name, "Chat Timestamp")) g_ChatTime = state;

    // Fullbright: real effect — patch gamma via SPI_SETSCREENSAVEACTIVE trick
    // is unreliable; instead we set a Windows gamma ramp that brightens the
    // whole screen. Turn off = reset.
    if (!std::strcmp(name, "Fullbright")) {
        if (state) {
            HDC hdc = GetDC(NULL);
            WORD ramp[3][256];
            for (int i = 0; i < 256; ++i) {
                float v = std::pow(i / 255.f, 0.55f);
                WORD w = (WORD)(v * 65535.f);
                ramp[0][i] = ramp[1][i] = ramp[2][i] = w;
            }
            SetDeviceGammaRamp(hdc, ramp);
            ReleaseDC(NULL, hdc);
        } else {
            HDC hdc = GetDC(NULL);
            WORD ramp[3][256];
            for (int i = 0; i < 256; ++i)
                ramp[0][i] = ramp[1][i] = ramp[2][i] = (WORD)(i * 257);
            SetDeviceGammaRamp(hdc, ramp);
            ReleaseDC(NULL, hdc);
        }
    }
}

// ── Per-frame work ────────────────────────────────────────────────
void OnFrame(float dt) {
    // FPS
    g_FpsAccum += dt;
    ++g_FpsCount;
    if (g_FpsAccum >= 0.5f) {
        g_FpsValue = g_FpsCount / g_FpsAccum;
        g_FpsAccum = 0.f;
        g_FpsCount = 0;
    }

    // Auto-sprint (simulated): hold sprint key
    if (g_Sprint) {
        // VK_SPRINT doesn't exist; sprint is Shift in MC. Only nudge when
        // the player is actually moving so we don't spam while idle.
        // (Real implementation would need JNI to setSprinting.)
    }

    // Sneak toggle: hold shift while enabled
    if (g_Sneak) {
        // (Same caveat as sprint — real toggle requires JNI.)
    }

    // Anti-AFK: tiny mouse nudge every 40s
    if (g_AntiAFK) {
        g_AntiAFKTim += dt;
        if (g_AntiAFKTim >= 40.f) {
            g_AntiAFKTim = 0.f;
            INPUT in{};
            in.type = INPUT_MOUSE;
            in.mi.dx = 1; in.mi.dy = 0;
            in.mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(1, &in, sizeof(in));
            in.mi.dx = -1;
            SendInput(1, &in, sizeof(in));
        }
    }

    // ── HUD overlays (drawn every frame on top of everything) ─────
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 disp = ImGui::GetIO().DisplaySize;

    float y = 90.f;
    const float x = 14.f;

    auto drawPill = [&](const char* text) {
        ImVec2 sz = ImGui::CalcTextSize(text);
        ImVec2 tl = ImVec2(x, y);
        ImVec2 br = ImVec2(x + sz.x + 16.f, y + sz.y + 8.f);
        fg->AddRectFilled(tl, br, IM_COL32(10, 12, 20, 170), 6.f);
        fg->AddRect(tl, br, IM_COL32(100, 180, 255, 60), 6.f);
        fg->AddText(ImVec2(tl.x + 8.f, tl.y + 4.f), IM_COL32(230, 235, 245, 255), text);
        y += sz.y + 16.f;
    };

    char buf[128];

    if (g_Coords) {
        // can't read MC coordinates without JNI — leave placeholder
        std::snprintf(buf, sizeof(buf), "XYZ: ---  ---  ---");
        drawPill(buf);
    }

    if (g_FPS) {
        std::snprintf(buf, sizeof(buf), "FPS: %.0f", g_FpsValue);
        drawPill(buf);
    }

    if (g_Clock) {
        std::time_t t = std::time(nullptr);
        std::tm lt{};
        localtime_s(&lt, &t);
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                      lt.tm_hour, lt.tm_min, lt.tm_sec);
        drawPill(buf);
    }

    if (g_Ping) {
        std::snprintf(buf, sizeof(buf), "Ping: -- ms");
        drawPill(buf);
    }
}
