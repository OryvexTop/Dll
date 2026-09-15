#include "clickgui.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cstring>
#include <cctype>
#include <ctime>

ClickGUI* g_Gui = nullptr;

// ── Modern palette ────────────────────────────────────────────────
static const ImVec4 ACCENT_A     = ImVec4(0.36f, 0.72f, 1.00f, 1.00f); // cyan-blue
static const ImVec4 ACCENT_B     = ImVec4(0.68f, 0.42f, 1.00f, 1.00f); // violet
static const ImVec4 ACCENT_GLOW  = ImVec4(0.36f, 0.72f, 1.00f, 0.25f);
static const ImVec4 BG_DEEP      = ImVec4(0.055f, 0.06f, 0.075f, 0.96f);
static const ImVec4 BG_CARD      = ImVec4(0.10f, 0.11f, 0.14f, 0.85f);
static const ImVec4 BG_CARD_HOV  = ImVec4(0.15f, 0.17f, 0.21f, 0.95f);
static const ImVec4 BG_HEADER    = ImVec4(0.08f, 0.09f, 0.115f, 1.00f);
static const ImVec4 TEXT_MAIN    = ImVec4(0.94f, 0.95f, 0.97f, 1.00f);
static const ImVec4 TEXT_DIM     = ImVec4(0.55f, 0.58f, 0.65f, 1.00f);
static const ImVec4 TEXT_FAINT   = ImVec4(0.38f, 0.40f, 0.46f, 1.00f);
static const ImVec4 TRACK_OFF    = ImVec4(0.20f, 0.21f, 0.25f, 1.00f);
static const ImVec4 BORDER       = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);

// ── Constructor ────────────────────────────────────────────────────
ClickGUI::ClickGUI() {
    auto add = [&](const char* n, const char* d, Category c, bool on) {
        modules[moduleCount++] = new Module(n, d, c, on);
    };

    // Combat
    add("Hit Color",      "Tint damage particles",     Category::Combat, false);
    add("Hit Sound",      "Custom hit feedback",       Category::Combat, false);
    add("Reach Circle",   "Draw attack radius",        Category::Combat, false);

    // Movement
    add("Sprint",         "Auto-sprint when moving",   Category::Movement, true);
    add("Sneak Toggle",   "Lock sneak on/off",         Category::Movement, false);
    add("FOV Boost",      "Widen FOV slightly",        Category::Movement, false);

    // Render
    add("Fullbright",     "Gamma to 100%",             Category::Render, true);
    add("Coordinates",    "Show XYZ + chunk",          Category::Render, true);
    add("FPS Counter",    "Minimal FPS overlay",       Category::Render, true);
    add("Clock",          "Real-time clock HUD",       Category::Render, false);
    add("Ping",           "Server latency HUD",        Category::Render, false);
    add("Keystrokes",     "Show pressed keys",         Category::Render, false);

    // Player
    add("Armor HUD",      "Armor durability",          Category::Player, false);
    add("Potion Timers",  "Active effect timers",      Category::Player, false);
    add("Item Counter",   "Count key items",           Category::Player, false);

    // Misc
    add("Chat Timestamp", "HH:MM prefix in chat",      Category::Misc, false);
    add("Copy IP",        "Click to copy server IP",   Category::Misc, false);
    add("Anti AFK",       "Prevent idle kick",         Category::Misc, true);
    add("Auto Reconnect", "Reconnect on disconnect",   Category::Misc, false);
}

// ── Easing ─────────────────────────────────────────────────────────
float ClickGUI::EaseOutExpo(float t) {
    return t >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * t);
}
float ClickGUI::EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
}
float ClickGUI::Lerp(float a, float b, float t) { return a + (b - a) * t; }
ImVec4 ClickGUI::LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(Lerp(a.x,b.x,t), Lerp(a.y,b.y,t), Lerp(a.z,b.z,t), Lerp(a.w,b.w,t));
}

// ── Search ─────────────────────────────────────────────────────────
bool ClickGUI::MatchesSearch(Module* m) {
    if (search[0] == 0) return true;
    char s[64], n[64];
    size_t i;
    for (i = 0; i < 63 && search[i]; ++i) s[i] = (char)std::tolower((unsigned char)search[i]);
    s[i] = 0;
    for (i = 0; i < 63 && m->name[i]; ++i) n[i] = (char)std::tolower((unsigned char)m->name[i]);
    n[i] = 0;
    return std::strstr(n, s) != nullptr;
}

// ── Watermark (top-left, fades in) ─────────────────────────────────
void ClickGUI::DrawWatermark() {
    watermarkAnim += (1.f - watermarkAnim) * 0.07f;

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f * watermarkAnim);
    ImGui::Begin("##wm", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // gradient dot
    float pulse = 0.6f + 0.4f * std::sin(clock * 2.2f);
    ImVec4 dot = LerpColor(ACCENT_A, ACCENT_B, 0.5f + 0.5f * std::sin(clock));
    dot.w = watermarkAnim * pulse;
    dl->AddCircleFilled(ImVec2(p.x + 5.f, p.y + 8.f), 4.f, ImGui::GetColorU32(dot), 16);

    ImGui::Dummy(ImVec2(14, 0));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(TEXT_MAIN.x,TEXT_MAIN.y,TEXT_MAIN.z, watermarkAnim));
    ImGui::Text("Muvixo");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ACCENT_A.x,ACCENT_A.y,ACCENT_A.z, watermarkAnim));
    ImGui::Text("Client");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(TEXT_FAINT.x,TEXT_FAINT.y,TEXT_FAINT.z, watermarkAnim * 0.8f));
    ImGui::Text("· Created by Muvixo");
    ImGui::PopStyleColor();
    ImGui::End();
}

// ── Header ─────────────────────────────────────────────────────────
void ClickGUI::DrawHeader() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float avail = ImGui::GetContentRegionAvail().x;

    // "logo"
    ImVec4 gradA = LerpColor(ACCENT_A, ACCENT_B, 0.5f + 0.5f * std::sin(clock * 1.5f));
    dl->AddRectFilled(p, ImVec2(p.x + 4.f, p.y + 26.f), ImGui::GetColorU32(gradA), 2.f);

    ImGui::Dummy(ImVec2(14, 0));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_MAIN);
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("Muvixo");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("Client");
    ImGui::PopStyleColor();

    // version badge on the right
    ImGui::SameLine(avail - 42.f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ACCENT_A.x, ACCENT_A.y, ACCENT_A.z, 1.f));
    ImGui::Text("v2.0");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));
}

// ── Tabs ───────────────────────────────────────────────────────────
void ClickGUI::DrawTabs() {
    const char* names[] = { "Combat", "Movement", "Render", "Player", "Misc" };
    const int tabCount  = (int)Category::COUNT;

    tabIndicator += ((float)activeTab - tabIndicator) * 0.25f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 base = ImGui::GetCursorScreenPos();
    float tabH = 34.f;
    float totalW = ImGui::GetContentRegionAvail().x;
    float tabW = totalW / (float)tabCount;

    // background strip
    dl->AddRectFilled(base, ImVec2(base.x + totalW, base.y + tabH),
                      ImGui::GetColorU32(BG_HEADER), 8.f);

    for (int i = 0; i < tabCount; ++i) {
        ImVec2 tl = ImVec2(base.x + tabW * i, base.y);
        ImGui::SetCursorScreenPos(tl);
        ImGui::InvisibleButton(names[i], ImVec2(tabW, tabH));

        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) activeTab = i;

        ImVec4 col;
        if (activeTab == i) col = TEXT_MAIN;
        else if (hovered)   col = ImVec4(TEXT_MAIN.x,TEXT_MAIN.y,TEXT_MAIN.z, 0.85f);
        else                col = TEXT_DIM;

        ImVec2 sz = ImGui::CalcTextSize(names[i]);
        dl->AddText(ImVec2(tl.x + (tabW - sz.x) * 0.5f,
                           tl.y + (tabH - sz.y) * 0.5f),
                    ImGui::GetColorU32(col), names[i]);
    }

    // sliding gradient underline
    float ux = base.x + tabW * tabIndicator;
    float pad = 12.f;
    ImVec4 gA = LerpColor(ACCENT_A, ACCENT_B, std::sin(clock * 1.2f) * 0.5f + 0.5f);
    dl->AddRectFilled(ImVec2(ux + pad, base.y + tabH - 3.f),
                      ImVec2(ux + tabW - pad, base.y + tabH - 1.f),
                      ImGui::GetColorU32(gA), 1.f);
    // glow under the underline
    dl->AddRectFilled(ImVec2(ux + pad, base.y + tabH - 1.f),
                      ImVec2(ux + tabW - pad, base.y + tabH + 1.f),
                      ImGui::GetColorU32(ImVec4(gA.x, gA.y, gA.z, 0.25f)), 1.f);

    ImGui::SetCursorScreenPos(ImVec2(base.x, base.y + tabH));
    ImGui::Dummy(ImVec2(0, 10));
}

// ── Search ─────────────────────────────────────────────────────────
void ClickGUI::DrawSearch() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        BG_CARD);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, BG_CARD_HOV);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  BG_CARD_HOV);
    ImGui::PushStyleColor(ImGuiCol_Border,         BORDER);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search modules...", search, sizeof(search));

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
    ImGui::Dummy(ImVec2(0, 8));
}

// ── Modules ────────────────────────────────────────────────────────
void ClickGUI::DrawModules() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rowH  = 52.f;
    float avail = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < moduleCount; ++i) {
        Module* m = modules[i];
        if ((int)m->category != activeTab) continue;
        if (!MatchesSearch(m)) continue;

        ImVec2 tl = ImGui::GetCursorScreenPos();
        ImVec2 br = ImVec2(tl.x + avail, tl.y + rowH);

        ImGui::InvisibleButton(m->name, ImVec2(avail, rowH));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();

        m->hoverAnim  += ((hovered ? 1.f : 0.f) - m->hoverAnim) * 0.22f;
        float tt = m->enabled ? 1.f : 0.f;
        m->toggleAnim += (tt - m->toggleAnim) * 0.28f;
        m->pulseAnim  *= 0.90f;

        if (clicked) {
            m->enabled = !m->enabled;
            m->pulseAnim = 1.f;
            extern void OnModuleToggled(const char* name, bool state);
            OnModuleToggled(m->name, m->enabled);
        }

        // Row background with hover lerp
        ImVec4 rowCol = LerpColor(BG_CARD, BG_CARD_HOV, m->hoverAnim);
        dl->AddRectFilled(tl, br, ImGui::GetColorU32(rowCol), 8.f);

        // Border
        dl->AddRect(tl, br, ImGui::GetColorU32(BORDER), 8.f);

        // Accent left bar (grows when enabled)
        if (m->toggleAnim > 0.01f) {
            float barH = (rowH - 16.f) * m->toggleAnim;
            float barY = tl.y + (rowH - barH) * 0.5f;
            ImVec4 bar = LerpColor(ACCENT_A, ACCENT_B, 0.5f);
            bar.w = m->toggleAnim;
            dl->AddRectFilled(ImVec2(tl.x + 1.f, barY),
                              ImVec2(tl.x + 4.f, barY + barH),
                              ImGui::GetColorU32(bar), 2.f);
        }

        // Pulse ring when clicked
        if (m->pulseAnim > 0.01f) {
            float r = 8.f + (1.f - m->pulseAnim) * 22.f;
            ImVec4 pulse = LerpColor(ACCENT_A, ACCENT_B, 0.5f);
            pulse.w = m->pulseAnim * 0.35f;
            dl->AddCircle(ImVec2(tl.x + 20.f, tl.y + rowH * 0.5f),
                          r, ImGui::GetColorU32(pulse), 24, 2.f);
        }

        // Name
        ImVec2 namePos = ImVec2(tl.x + 18.f, tl.y + 10.f);
        dl->AddText(namePos, ImGui::GetColorU32(TEXT_MAIN), m->name);

        // Description
        ImVec2 descPos = ImVec2(tl.x + 18.f, tl.y + 29.f);
        dl->AddText(descPos, ImGui::GetColorU32(TEXT_FAINT), m->description);

        // Toggle switch (right side)
        float swW = 40.f, swH = 20.f;
        ImVec2 swTL = ImVec2(br.x - swW - 16.f, tl.y + (rowH - swH) * 0.5f);
        ImVec2 swBR = ImVec2(swTL.x + swW, swTL.y + swH);

        ImVec4 track = LerpColor(TRACK_OFF, ACCENT_A, m->toggleAnim);
        track.w = 1.f;
        dl->AddRectFilled(swTL, swBR, ImGui::GetColorU32(track), swH * 0.5f);

        // Track glow when on
        if (m->toggleAnim > 0.05f) {
            ImVec4 glow = track;
            glow.w = 0.30f * m->toggleAnim;
            dl->AddRectFilled(ImVec2(swTL.x - 2.f, swTL.y - 2.f),
                              ImVec2(swBR.x + 2.f, swBR.y + 2.f),
                              ImGui::GetColorU32(glow), (swH + 4.f) * 0.5f);
        }

        // Knob
        float knobR = swH * 0.5f - 2.f;
        float knobX = swTL.x + knobR + 2.f + (swW - swH) * m->toggleAnim;
        float knobY = swTL.y + swH * 0.5f;
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR,
                            ImGui::GetColorU32(ImVec4(1,1,1,1)), 20);

        ImGui::Dummy(ImVec2(0, 4));
    }
}

// ── Footer ─────────────────────────────────────────────────────────
void ClickGUI::DrawFooter() {
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_FAINT);
    ImGui::Text("RightShift");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("close");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.f);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_FAINT);
    ImGui::Text("END");
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("unload");
    ImGui::PopStyleColor();
}

// ── Render ─────────────────────────────────────────────────────────
void ClickGUI::Render(float dt) {
    clock += dt;
    DrawWatermark();

    float target = visible ? 1.f : 0.f;
    // Smooth open/close with slight overshoot
    openAnim = Lerp(openAnim, target, 1.f - std::pow(0.001f, dt * 3.f));
    if (std::fabs(openAnim - target) < 0.002f) openAnim = target;
    if (openAnim < 0.001f && !visible) return;

    float ease = visible ? EaseOutBack(openAnim) : EaseOutExpo(openAnim);

    ImVec2 panelSize = ImVec2(380.f, 540.f);
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    float px = (disp.x - panelSize.x) * 0.5f;
    float py = (disp.y - panelSize.y) * 0.5f - 30.f + (1.f - openAnim) * 30.f;
    float scale = 0.94f + 0.06f * openAnim;

    ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelSize.x, panelSize.y), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(BG_DEEP.w * openAnim);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DEEP);
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(1,1,1,0.08f * openAnim));

    bool open = true;
    ImGui::Begin("##muvixo", &open,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    DrawHeader();
    DrawTabs();
    DrawSearch();

    ImGui::BeginChild("##list", ImVec2(0, -32.f), false,
        ImGuiWindowFlags_NoBackground);
    DrawModules();
    ImGui::EndChild();

    DrawFooter();
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);

    if (!open) visible = false;
}
