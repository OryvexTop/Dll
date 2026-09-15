#pragma once

// Called when any module is toggled from the GUI
void OnModuleToggled(const char* name, bool state);

// Called every frame from the swap hook (dt in seconds)
void OnFrame(float dt);
