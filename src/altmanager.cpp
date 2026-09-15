#include "altmanager.h"
#include <Windows.h>
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <ctime>
#include <shlobj.h>

AltManager* g_AltMgr = nullptr;

static std::string DataDir() {
    char path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::string p = path;
        p += "\\MuvixoClient";
        CreateDirectoryA(p.c_str(), nullptr);
        return p;
    }
    return ".";
}

static std::string DataFile() { return DataDir() + "\\alts.txt"; }

// ── Load / Save (plain text: name|uuid|token|note|isMS) ────────────
void AltManager::Load() {
    accounts.clear();
    std::ifstream f(DataFile());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        AltAccount a{};
        std::string tmp;
        std::getline(ss, a.username, '|');
        std::getline(ss, a.uuid,     '|');
        std::getline(ss, a.token,    '|');
        std::getline(ss, a.note,     '|');
        std::getline(ss, tmp,        '|');
        a.isMicrosoft = (tmp == "1");
        if (!a.username.empty()) accounts.push_back(a);
    }
}

void AltManager::Save() {
    std::ofstream f(DataFile(), std::ios::trunc);
    for (auto& a : accounts) {
        f << a.username << '|'
          << a.uuid     << '|'
          << a.token    << '|'
          << a.note     << '|'
          << (a.isMicrosoft ? 1 : 0) << '\n';
    }
}

// ── Add / Remove ───────────────────────────────────────────────────
void AltManager::Add(const std::string& name, bool ms) {
    AltAccount a{};
    a.username    = name;
    // Fake UUID derived from name so it stays consistent between saves
    unsigned long h = 5381;
    for (char c : name) h = h * 33 + (unsigned char)c;
    char buf[64];
    std::snprintf(buf, sizeof(buf),
        "%08lx-0000-0000-0000-%012lx", h & 0xffffffff, h & 0xffffffffffff);
    a.uuid        = buf;
    a.token       = "";
    a.note        = newNote;
    a.isMicrosoft = ms;
    accounts.push_back(a);
    Save();
    SetStatus(("Added " + name).c_str());
}

void AltManager::Remove(int index) {
    if (index < 0 || index >= (int)accounts.size()) return;
    std::string n = accounts[index].username;
    accounts.erase(accounts.begin() + index);
    Save();
    if (selected >= (int)accounts.size()) selected = -1;
    SetStatus(("Removed " + n).c_str());
}

void AltManager::SetStatus(const char* msg) {
    std::snprintf(status, sizeof(status), "%s", msg);
    statusTimer = 3.f;
}

// ── Render ─────────────────────────────────────────────────────────
static const ImVec4 A_PURPLE = ImVec4(0.64f, 0.42f, 1.00f, 1.00f);
static const ImVec4 A_PINK   = ImVec4(1.00f, 0.42f, 0.72f, 1.00f);
static const ImVec4 A_DIM    = ImVec4(0.55f, 0.55f, 0.62f, 1.00f);
static const ImVec4 A_BG     = ImVec4(0.08f,  0.08f,  0.10f,  0.97f);
static const ImVec4 A_CARD   = ImVec4(0.11f,  0.11f,  0.14f,  0.90f);
static const ImVec4 A_HOVER  = ImVec4(0.16f,  0.16f,  0.20f,  0.95f);

void AltManager::Render() {
    if (statusTimer > 0.f) {
        // will be decremented in hooks.cpp via dt — safe fallback here
        statusTimer -= ImGui::GetIO().DeltaTime;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 460), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 300,
               ImGui::GetIO().DisplaySize.y * 0.5f - 230),
        ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, A_BG);

    ImGui::Begin("Alt Manager  —  Created by Muvixo", nullptr,
                 ImGuiWindowFlags_NoCollapse);

    // ── Header ────────────────────────────────────────────────
    ImGui::TextColored(A_PURPLE, "Alt Manager");
    ImGui::SameLine();
    ImGui::TextDisabled("(%d account%s)",
                        (int)accounts.size(),
                        accounts.size() == 1 ? "" : "s");
    ImGui::Separator();

    // ── Add row ───────────────────────────────────────────────
    ImGui::PushItemWidth(180);
    ImGui::InputTextWithHint("##name", "Username", newName, sizeof(newName));
    ImGui::SameLine();
    ImGui::InputTextWithHint("##note", "Note (optional)", newNote, sizeof(newNote));
    ImGui::SameLine();

    if (ImGui::Button("Add (Offline)", ImVec2(120, 0)) && newName[0]) {
        Add(newName, false);
        newName[0] = 0;
        newNote[0] = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add (MS)", ImVec2(90, 0)) && newName[0]) {
        Add(newName, true);
        newName[0] = 0;
        newNote[0] = 0;
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();

    // ── Two columns: list | details ──────────────────────────
    ImGui::BeginChild("##list", ImVec2(240, 0), true);
    for (int i = 0; i < (int)accounts.size(); ++i) {
        auto& a = accounts[i];
        ImGui::PushID(i);

        bool isSel = (selected == i);
        ImVec4 bg = isSel ? A_HOVER : A_CARD;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::BeginChild("##row", ImVec2(0, 40), false,
                          ImGuiWindowFlags_NoScrollbar);

        ImVec4 col = a.isMicrosoft ? A_PINK : A_PURPLE;
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", a.isMicrosoft ? "MS" : "CR");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", a.username.c_str());

        if (!a.note.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("· %s", a.note.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) selected = i;

        // Right-click context menu
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem("Apply")) SetStatus(("Applied " + a.username).c_str());
            if (ImGui::MenuItem("Copy UUID")) {
                ImGui::SetClipboardText(a.uuid.c_str());
                SetStatus("UUID copied");
            }
            if (ImGui::MenuItem("Delete")) { Remove(i); ImGui::EndPopup(); ImGui::PopID(); break; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##detail", ImVec2(0, 0), true);
    if (selected >= 0 && selected < (int)accounts.size()) {
        auto& a = accounts[selected];
        ImGui::TextColored(A_PURPLE, "%s", a.username.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", a.isMicrosoft ? "Microsoft" : "Offline");
        ImGui::Separator();

        ImGui::TextDisabled("UUID");
        ImGui::TextWrapped("%s", a.uuid.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Token");
        ImGui::TextWrapped("%s", a.token.empty() ? "(none — offline)" : a.token.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Note");
        ImGui::TextWrapped("%s", a.note.empty() ? "(none)" : a.note.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Apply to Minecraft", ImVec2(160, 0))) {
            SetStatus(("Switched to " + a.username).c_str());
            // The actual account swap needs a JNI call into
            // net.minecraft.client.Minecraft.getInstance().setSession()
            // which is MC-version specific. Placeholder here.
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy UUID")) {
            ImGui::SetClipboardText(a.uuid.c_str());
            SetStatus("UUID copied to clipboard");
        }
    } else {
        ImGui::TextDisabled("Select an account on the left.");
        ImGui::Spacing();
        ImGui::TextWrapped("Add offline alts by typing a username above. "
                           "Microsoft accounts need a real OAuth flow that "
                           "this build doesn't ship — those entries are "
                           "placeholders you can rename freely.");
    }
    ImGui::EndChild();

    // ── Status ────────────────────────────────────────────────
    ImGui::Separator();
    if (statusTimer > 0.f)
        ImGui::TextColored(A_PINK, "%s", status);
    else
        ImGui::TextDisabled("Ready.  %zu account(s) saved to %%APPDATA%%\\MuvixoClient\\alts.txt",
                            accounts.size());

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
