// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information

// Main.NetPlay

extern const ConfigInfo<std::string> NETPLAY_TRAVERSAL_SERVER;
extern const ConfigInfo<u16> NETPLAY_TRAVERSAL_PORT;
extern const ConfigInfo<std::string> NETPLAY_TRAVERSAL_CHOICE;
extern const ConfigInfo<std::string> NETPLAY_HOST_CODE;

extern const ConfigInfo<u16> NETPLAY_HOST_PORT;
extern const ConfigInfo<std::string> NETPLAY_ADDRESS;
extern const ConfigInfo<u16> NETPLAY_CONNECT_PORT;
extern const ConfigInfo<u16> NETPLAY_LISTEN_PORT;

extern const ConfigInfo<std::string> NETPLAY_NICKNAME;
extern const ConfigInfo<std::string> NETPLAY_SELECTED_HOST_GAME;
extern const ConfigInfo<bool> NETPLAY_USE_UPNP;

extern const ConfigInfo<bool> NETPLAY_ENABLE_QOS;

extern const ConfigInfo<u32> NETPLAY_BUFFER_SIZE;
extern const ConfigInfo<u32> NETPLAY_CLIENT_BUFFER_SIZE;

extern const ConfigInfo<bool> NETPLAY_WRITE_SAVE_SDCARD_DATA;
extern const ConfigInfo<bool> NETPLAY_LOAD_WII_SAVE;
extern const ConfigInfo<bool> NETPLAY_SYNC_SAVES;
extern const ConfigInfo<bool> NETPLAY_RECORD_INPUTS;
extern const ConfigInfo<bool> NETPLAY_REDUCE_POLLING_RATE;
extern const ConfigInfo<bool> NETPLAY_STRICT_SETTINGS_SYNC;
extern const ConfigInfo<bool> NETPLAY_HOST_INPUT_AUTHORITY;

}  // namespace Config
