#pragma once

#include "play_manager.h"
#include "discord_gateway.h"
#include "apk_manager.h"

class DiscordState {

private:
    playapi::config discordConf;
    PlayManager& playManager;
    ApkManager& apkManager;
    std::vector<std::string> broadcastChannels;
    std::set<std::string> operatorList;

    bool checkOp(discord::Message const& m);

public:
    discord::Api api;
    discord::gateway::Connection conn;

    DiscordState(PlayManager& playManager, ApkManager& apkManager);

    void onMessage(discord::Message const& m);

    void onNewVersion(int version, std::string const& changelog, std::string const& variant);

    void loop();

    void storeSessionInfo();

};