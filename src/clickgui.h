#pragma once
#include <Windows.h>
#include <imgui.h>

enum class Category {
    Combat = 0,
    Movement,
    Render,
    Player,
    Misc,
    COUNT
};

struct Module {
    const char* name;
    const char* description;
    Category    category;
    bool        enabled;
    float       toggleAnim;
    float       hoverAnim;
    Module(const char* n, const char* d, Category c, bool on)
        : name(n), description(d), category(c), enabled(on),
          toggleAnim(on ? 1.f : 0.f), hoverAnim(0.f) {}
};

class ClickGUI {
public:
    bool  visible       = false;
    float openAnim      = 0.f;
    float watermarkAnim = 0.f;
    int   activeTab     = 0;
    float tabIndicator  = 0.f;
    float clock         = 0.f;
    char  search[64]    = {0};

    Module* modules[32];
    int     moduleCount = 0;

    ClickGUI();
    void Toggle() { visible = !visible; }
    void Render(float dt);
    void DrawWatermark();
};

extern ClickGUI* g_Gui;
