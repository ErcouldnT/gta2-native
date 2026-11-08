// Network play: relay connection (net_dplay.cpp) and the touch lobby (net_lobby.cpp)
#pragma once
#include <string>

class NetUi;

namespace port
{
bool NetConnect(const std::string& hostPort, std::string& error);
void NetClose();
bool NetConnected();
void NetKeepAlive();
void NetNotifySessionLost();

// Shown before the game starts: returns true when the player chose multiplayer and connected
bool RunStartScreen();
// Replaces the original Win32 network dialog; true when the game should start
bool RunNetLobby(NetUi* ui);
} // namespace port
