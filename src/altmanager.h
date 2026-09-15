#pragma once
#include <string>
#include <vector>

struct AltAccount {
    std::string username;
    std::string uuid;
    std::string token;
    std::string note;
    bool        isMicrosoft;
};

class AltManager {
public:
    std::vector<AltAccount> accounts;
    int  selected = -1;
    char newName[64] = {0};
    char newNote[128] = {0};
    bool showLoginPopup = false;
    char loginUser[64] = {0};
    char loginPass[64] = {0};
    char status[128] = {0};
    float statusTimer = 0.f;

    void Load();
    void Save();
    void Add(const std::string& name, bool ms);
    void Remove(int index);
    void SetStatus(const char* msg);

    // Callback returns the profile that should be applied (empty = cancel)
    void Render();
};

extern AltManager* g_AltMgr;
