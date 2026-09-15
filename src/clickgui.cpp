#include "clickgui.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cstring>
#include <cctype>

ClickGUI* g_Gui = nullptr;

// ── Palette ────────────────────────────────────────────────────────
static const ImVec4 ACCENT       = ImVec4(0.42f, 0.65f, 1.00f, 1.00f);
static const ImVec4 ACCENT_DIM   = ImVec4(0.42f, 0.65f, 1.00f, 0.30f);
static const ImVec4 BG_PANEL     = ImVec4(0.075f, 0.082f, 0.100f, 0.94f);
static const ImVec4 BG_HEADER    = ImVec4(0.11f, 0.12f, 0.15f, 0.95f);
static const ImVec4 BG_ROW       = ImVec4(0.13f, 0.14f, 0.17f, 0.85f);
static const ImVec4 BG_ROW_HOVER = ImVec4(0.18f, 0.20f, 0.25f, 0.95f);
static const ImVec4 TEXT_MAIN    = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
static const ImVec4 TEXT_DIM     = ImVec4(0.55f, 0.57f, 0.62f, 1.00f);
static const ImVec4 TRACK_OFF    = ImVec4(0.22f, 0.23f, 0.27f, 1.00f);

// ── Constructor ────────────────────────────────────────────────────
ClickGUI::ClickGUI() {
    auto add = [&](const char* n, const char* d, Category c, bool on) {
        modules[moduleCount++] = new Module(n, d, c, on);
    };

    // Combat
    add("Hit Color",      "Tint damage particles",     Category::Combat, true);
    add("Hit Sound",      "Custom hit feedback",       Category::Combat, false);
    add("Reach Circle",   "Draw your attack radius",   Category::Combat, false);

    // Movement
    add("Sprint",         "Auto-sprint while moving",  Category::Movement, true);
    add("Sneak Toggle",   "Lock sneak on/off",         Category::Movement, false);
    add("FOV Boost",      "Slightly wider FOV",        Category::Movement, false);

    // Render
    add("Fullbright",     "Gamma to 100%",             Category::Render, false);
    add("Coordinates",    "Show XYZ + chunk",          Category::Render, true);
    add("FPS Counter",    "Minimal FPS overlay",       Category::Render, true);
    add("Clock",          "Real-time clock",           Category::Render, false);
    add("Ping",           "Server latency",            Category::Render, true);

    // Player
    add("Armor HUD",      "Armor durability",          Category::Player, false);
    add("Potion Timers",  "Active effect timers",      Category::Player, false);
    add("Item Counter",   "Count key items",           Category::Player, false);

    // Misc
    add("Chat Timestamp", "HH:MM prefix in chat",      Category::Misc, false);
    add("Copy IP",        "Copy server address",       Category::Misc, false);
    add("Screenshot",     "Clean HUD screenshot",      Category::Misc, false);
    add("Anti AFK",       "Prevent idle kick",         Category::Misc, false);
}

// ── Easing ─────────────────────────────────────────────────────────
float ClickGUI::EaseOutCubic(float t) {
    float f = 1.f - t;
    return 1.f - f * f * f;
}
float ClickGUI::EaseInOutQuad(float t) {
    return t < 0.5f ? 2.f * t * t : 1.f - std::pow(-2.f * t + 2.f, 2.f) / 2.f;
}

// ── Search filter ──────────────────────────────────────────────────
bool ClickGUI::MatchesSearch(Module* m) {
    if (search[0] == 0) return true;
    char lower[64];
    size_t i = 0;
    for (; i < sizeof(lower) - 1 && search[i]; ++i)
        lower[i] = (char)std::tolower((unsigned char)search[i]);
    lower[i] = 0;

    char nameLower[64];
    for (i = 0; m->name[i] && i < sizeof(nameLower) - 1; ++i)
        nameLower[i] = (char)std::tolower((unsigned char)m->name[i]);
    nameLower[i] = 0;

    return std::strstr(nameLower, lower) != nullptr;
}

// ── Watermark ──────────────────────────────────────────────────────
void ClickGUI::DrawWatermark() {
    watermarkAnim += (1.f - watermarkAnim) * 0.06f;

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.30f * watermarkAnim);
    ImGui::Begin("##wm", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);

    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, watermarkAnim));
    ImGui::Text("Muvixo");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(TEXT_MAIN.x, TEXT_MAIN.y, TEXT_MAIN.z, watermarkAnim * 0.9f));
    ImGui::Text("Client");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(TEXT_DIM.x, TEXT_DIM.y, TEXT_DIM.z, watermarkAnim * 0.7f));
    ImGui::Text("| Created by Muvixo");
    ImGui::PopStyleColor();

    ImGui::End();
}

// ── Tabs ───────────────────────────────────────────────────────────
void ClickGUI::DrawTabs() {
    const char* names[] = { "Combat", "Movement", "Render", "Player", "Misc" };
    const int tabCount  = (int)Category::COUNT;

    float target = (float)activeTab;
    tabIndicator += (target - tabIndicator) * 0.22f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 base = ImGui::GetCursorScreenPos();
    float  tabH = 32.f;
    float  tabW = ImGui::GetContentRegionAvail().x / (float)tabCount;

    dl->AddRectFilled(base, ImVec2(base.x + tabW * tabCount, base.y + tabH),
                      ImGui::GetColorU32(BG_HEADER));

    for (int i = 0; i < tabCount; ++i) {
        ImVec2 tl = ImVec2(base.x + tabW * i, base.y);
        ImVec2 br = ImVec2(tl.x + tabW, tl.y + tabH);

        ImGui::SetCursorScreenPos(tl);
        ImGui::InvisibleButton(names[i], ImVec2(tabW, tabH));

        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) activeTab = i;

        ImVec4 col = (activeTab == i)
            ? ACCENT
            : (hovered ? ImVec4(TEXT_MAIN.x, TEXT_MAIN.y, TEXT_MAIN.z, 0.85f) : TEXT_DIM);

        ImVec2 sz = ImGui::CalcTextSize(names[i]);
        dl->AddText(ImVec2(tl.x + (tabW - sz.x) * 0.5f,
                           tl.y + (tabH - sz.y) * 0.5f),
                    ImGui::GetColorU32(col), names[i]);
    }

    float ux = base.x + tabW * tabIndicator;
    float uy = base.y + tabH - 2.f;
    dl->AddRectFilled(ImVec2(ux + 8.f, uy),
                      ImVec2(ux + tabW - 8.f, uy + 2.f),
                      ImGui::GetColorU32(ACCENT));

    ImGui::SetCursorScreenPos(ImVec2(base.x, base.y + tabH));
    ImGui::Dummy(ImVec2(0, 6));
}

// ── Search ─────────────────────────────────────────────────────────
void ClickGUI::DrawSearch() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        BG_ROW);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, BG_ROW_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Border,         ACCENT_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search modules...",
                             search, sizeof(search));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    ImGui::Dummy(ImVec2(0, 6));
}

// ── Module rows ────────────────────────────────────────────────────
void ClickGUI::DrawModules() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rowH = 46.f;
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

        m->hoverAnim  += ((hovered ? 1.f : 0.f) - m->hoverAnim) * 0.20f;
        float toggleTarget = m->enabled ? 1.f : 0.f;
        m->toggleAnim += (toggleTarget - m->toggleAnim) * 0.25f;

        if (clicked) m->enabled = !m->enabled;

        ImVec4 rowCol = ImVec4(
            BG_ROW.x + (BG_ROW_HOVER.x - BG_ROW.x) * m->hoverAnim,
            BG_ROW.y + (BG_ROW_HOVER.y - BG_ROW.y) * m->hoverAnim,
            BG_ROW.z + (BG_ROW_HOVER.z - BG_ROW.z) * m->hoverAnim,
            BG_ROW.w + (BG_ROW_HOVER.w - BG_ROW.w) * m->hoverAnim);
        dl->AddRectFilled(tl, br, ImGui::GetColorU32(rowCol), 6.f);

        if (m->toggleAnim > 0.01f) {
            ImVec4 bar = ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, m->toggleAnim);
            dl->AddRectFilled(ImVec2(tl.x, tl.y + 6.f),
                              ImVec2(tl.x + 3.f, br.y - 6.f),
                              ImGui::GetColorU32(bar), 2.f);
        }

        ImVec2 namePos = ImVec2(tl.x + 14.f, tl.y + 8.f);
        dl->AddText(namePos, ImGui::GetColorU32(TEXT_MAIN), m->name);

        ImVec2 descPos = ImVec2(tl.x + 14.f, tl.y + 26.f);
        dl->AddText(descPos, ImGui::GetColorU32(TEXT_DIM), m->description);

        float swW = 36.f, swH = 18.f;
        ImVec2 swTL = ImVec2(br.x - swW - 14.f, tl.y + (rowH - swH) * 0.5f);
        ImVec2 swBR = ImVec2(swTL.x + swW, swTL.y + swH);

        ImVec4 track = ImVec4(
            TRACK_OFF.x + (ACCENT.x - TRACK_OFF.x) * m->toggleAnim,
            TRACK_OFF.y + (ACCENT.y - TRACK_OFF.y) * m->toggleAnim,
            TRACK_OFF.z + (ACCENT.z - TRACK_OFF.z) * m->toggleAnim,
            1.f);
        dl->AddRectFilled(swTL, swBR, ImGui::GetColorU32(track), swH * 0.5f);

        float knobR = swH * 0.5f - 2.f;
        float knobX = swTL.x + knobR + 2.f
                    + (swW - swH) * m->toggleAnim;
        float knobY = swTL.y + swH * 0.5f;
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR,
                            ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 24);

        ImGui::Dummy(ImVec2(0, 2));
    }
}

// ── Main render ────────────────────────────────────────────────────
void ClickGUI::Render(float dt) {
    DrawWatermark();

    float target = visible ? 1.f : 0.f;
    openAnim += (target - openAnim) * 0.20f;
    if (openAnim < 0.001f && !visible) return;
    if (openAnim > 0.999f && visible)  openAnim = 1.f;

    float ease = EaseOutCubic(openAnim);

    ImVec2 panelSize = ImVec2(360.f, 520.f);
    ImVec2 center    = ImGui::GetIO().DisplaySize;
    center.x = (center.x - panelSize.x) * 0.5f;
    center.y = (center.y - panelSize.y) * 0.5f - 40.f + (1.f - ease) * 24.f;

    ImGui::SetNextWindowPos(center, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(BG_PANEL.w * ease);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border,   ACCENT_DIM);

    bool pOpen = true;
    ImGui::Begin("##muvixo", &pOpen,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove);

    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::Text("Muvixo Client");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("v1.0");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    DrawTabs();
    DrawSearch();

    ImGui::BeginChild("##list", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoBackground);
    DrawModules();
    ImGui::EndChild();

    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    if (!pOpen) visible = false;
}
