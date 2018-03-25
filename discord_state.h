#pragma once

#include "play_manager.h"
#include "discord_gateway.h"
#include "apk_manager.h"

class DiscordState {

private:
    playapi::config discordConf;
    PlayManager& playManager;
    ApkManager& apkManager;

public:
    discord::Api api;
    discord::gateway::Connection conn;

    DiscordState(PlayManager& playManager, ApkManager& apkManager);

    void onMessage(discord::Message const& m);

    void loop();

    void storeSessionInfo();

};