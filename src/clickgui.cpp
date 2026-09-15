#include "clickgui.h"
#include <imgui.h>
#include <imgui_internal.h>
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
    add("Sprint",       "Auto-sprint when moving", Category::Movement, true);
    add("Fullbright",   "Gamma to 100%",           Category::Render,   true);
    add("Coordinates",  "Show XYZ + chunk",        Category::Render,   true);
    add("FPS Counter",  "Minimal FPS overlay",     Category::Render,   true);
    add("Anti AFK",     "Prevent idle kick",       Category::Misc,     true);
}

void ClickGUI::Render(float dt) {
    if (!visible) return;

    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DEEP);

    bool open = true;
    ImGui::Begin("Muvixo Client - Created by Muvixo", &open);

    ImGui::TextColored(ACCENT_A, "Muvixo Client");
    ImGui::SameLine();
    ImGui::TextDisabled("| Created by Muvixo");
    ImGui::Separator();

    if (ImGui::BeginTabBar("##tabs")) {
        const char* tabs[] = { "Combat", "Movement", "Render", "Player", "Misc" };
        for (int i = 0; i < 5; ++i) {
            if (ImGui::BeginTabItem(tabs[i])) {
                activeTab = i;
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
