#include "clickgui.h"
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <cctype>

ClickGUI* g_Gui = nullptr;

static const ImVec4 ACCENT_A  = ImVec4(0.64f, 0.42f, 1.00f, 1.00f);
static const ImVec4 ACCENT_B  = ImVec4(1.00f, 0.42f, 0.72f, 1.00f);
static const ImVec4 BG_DEEP   = ImVec4(0.055f, 0.055f, 0.075f, 0.97f);
static const ImVec4 TEXT_MAIN = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
static const ImVec4 TEXT_DIM  = ImVec4(0.60f, 0.60f, 0.68f, 1.00f);

ClickGUI::ClickGUI() {
    auto add = [&](const char* n, const char* d, Category c, bool on) {
        modules[moduleCount++] = new Module(n, d, c, on);
    };
    add("Hit Color",   "Tint damage particles", Category::Combat,   false);
    add("Reach Circle","Draw attack radius",    Category::Combat,   false);
    add("Sprint",      "Auto-sprint when moving",Category::Movement,true);
    add("Sneak Toggle","Lock sneak on/off",     Category::Movement, false);
    add("Fullbright",  "Gamma to 100%",         Category::Render,   true);
    add("Coordinates", "Show XYZ + chunk",      Category::Render,   true);
    add("FPS Counter", "Minimal FPS overlay",   Category::Render,   true);
    add("Clock",       "Real-time clock HUD",   Category::Render,   false);
    add("Ping",        "Server latency HUD",    Category::Render,   false);
    add("Armor HUD",   "Armor durability",      Category::Player,   false);
    add("Potion Timers","Active effect timers", Category::Player,   false);
    add("Chat Time",   "HH:MM prefix in chat",  Category::Misc,     false);
    add("Anti AFK",    "Prevent idle kick",     Category::Misc,     true);
    add("Auto Reconnect","Reconnect on disconnect",Category::Misc,  false);
}

void ClickGUI::DrawWatermark() {
    watermarkAnim += (1.f - watermarkAnim) * 0.07f;
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f * watermarkAnim);
    ImGui::Begin("##wm", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::TextColored(ImVec4(ACCENT_A.x, ACCENT_A.y, ACCENT_A.z, watermarkAnim),
                       "Muvixo");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(TEXT_MAIN.x, TEXT_MAIN.y, TEXT_MAIN.z, watermarkAnim),
                       "Client");
    ImGui::SameLine();
    ImGui::TextDisabled("| Created by Muvixo");
    ImGui::End();
}

void ClickGUI::Render(float dt) {
    clock += dt;
    DrawWatermark();

    float target = visible ? 1.f : 0.f;
    openAnim += (target - openAnim) * 0.20f;
    if (std::fabs(openAnim - target) < 0.002f) openAnim = target;
    if (openAnim < 0.001f && !visible) return;

    ImVec2 panelSize = ImVec2(420.f, 540.f);
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    float px = (disp.x - panelSize.x) * 0.5f;
    float py = (disp.y - panelSize.y) * 0.5f - 30.f + (1.f - openAnim) * 30.f;

    ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(BG_DEEP.w * openAnim);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DEEP);

    bool open = true;
    ImGui::Begin("##muvixo", &open,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::TextColored(ACCENT_A, "Muvixo Client");
    ImGui::SameLine();
    ImGui::TextDisabled("| Created by Muvixo");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    if (ImGui::BeginTabBar("##tabs")) {
        const char* tabs[] = { "Combat", "Movement", "Render", "Player", "Misc" };
        for (int i = 0; i < 5; ++i) {
            if (ImGui::BeginTabItem(tabs[i])) {
                activeTab = i;
                ImGui::Dummy(ImVec2(0, 4));
                for (int j = 0; j < moduleCount; ++j) {
                    Module* m = modules[j];
                    if ((int)m->category != i) continue;
                    ImGui::Checkbox(m->name, &m->enabled);
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", m->description);
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (!open) visible = false;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) visible = false;
}
