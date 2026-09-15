#pragma once
#include <Windows.h>

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
    float       toggleAnim;   // 0..1 slide animation for the switch
    float       hoverAnim;    // 0..1 hover highlight
    Module(const char* n, const char* d, Category c, bool on)
        : name(n), description(d), category(c), enabled(on),
          toggleAnim(on ? 1.f : 0.f), hoverAnim(0.f) {}
};

class ClickGUI {
public:
    bool  visible       = false;
    float openAnim      = 0.f;   // 0..1 whole-panel animation
    float watermarkAnim = 0.f;   // 0..1 watermark fade-in
    int   activeTab     = 0;
    float tabIndicator  = 0.f;   // animated tab underline x-offset
    char  search[64]    = {0};

    Module* modules[32];
    int     moduleCount = 0;

    ClickGUI();
    void Toggle() { visible = !visible; }
    void Render(float dt);
    void DrawWatermark();

private:
    void  DrawTabs();
    void  DrawSearch();
    void  DrawModules();
    float EaseOutCubic(float t);
    float EaseInOutQuad(float t);
    bool  MatchesSearch(Module* m);
};

extern ClickGUI* g_Gui;
