#pragma once
#include <Windows.h>

struct Module { const char* name; bool enabled; };

class ClickGUI {
public:
    bool visible = false;
    Module modules[6] = {
        {"Sprint", true}, {"Fullbright", false}, {"ToggleSneak", false},
        {"FPS Boost", true}, {"Coordinates", true}, {"Keystrokes", false},
    };
    void Toggle() { visible = !visible; }
    void Render();
    void DrawWatermark();
};

extern ClickGUI* g_Gui;
