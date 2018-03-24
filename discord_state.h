#pragma once

#include "play_manager.h"
#include "discord_gateway.h"

class DiscordState {

private:
    playapi::config discordConf;
    PlayManager& playManager;

public:
    discord::Api api;
    discord::gateway::Connection conn;

    DiscordState(PlayManager& playManager);

    void onMessage(discord::gateway::Message const& m);

    void loop();

    void storeSessionInfo();

};