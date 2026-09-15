#include "clickgui.h"
#include <imgui.h>

ClickGUI* g_Gui = nullptr;

void ClickGUI::DrawWatermark() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##wm", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav);
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "Muvixo Client");
    ImGui::SameLine();
    ImGui::TextDisabled("| Created by Muvixo");
    ImGui::End();
}

void ClickGUI::Render() {
    DrawWatermark();
    if (!visible) return;
    ImGui::SetNextWindowSize(ImVec2(300, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Muvixo ClickGUI", &visible);
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "Modules");
    ImGui::Separator();
    for (auto& m : modules) ImGui::Checkbox(m.name, &m.enabled);
    ImGui::Spacing(); ImGui::Separator();
    ImGui::TextDisabled("RightShift - toggle menu");
    ImGui::TextDisabled("Created by Muvixo");
    ImGui::End();
}
