#pragma once

#include "play_manager.h"
#include "discord_gateway.h"
#include "apk_manager.h"
#include "win10_store_manager.h"

class DiscordState {

private:
    struct JsonNotifyRule {
        std::string channel;
        bool notifyW10Release = false;
        bool notifyW10Beta = false;
    };

    playapi::config discordConf;
    PlayManager& playManager;
    ApkManager& apkManager;
    Win10StoreManager* win10StoreManager = nullptr;
    std::vector<std::string> broadcastChannels;
    std::vector<std::string> broadcastChannelsW10;
    std::set<std::string> operatorList;
    std::vector<JsonNotifyRule> jsonNotifyRules;

    bool checkOp(discord::Message const& m);

    std::string buildVersionFieldString(ApkVersionInfo const& arm, ApkVersionInfo const& x86);

public:
    discord::Api api;
    discord::gateway::Connection conn;

    DiscordState(PlayManager& playManager, ApkManager& apkManager);

    void addWin10StoreMgr(Win10StoreManager& mgr);

    void onMessage(discord::Message const& m);

    void onNewVersion(int version, std::string const& versionString,
                      std::string const& changelog, std::string const& variant);

    void onNewWin10Version(std::vector<Win10StoreNetwork::UpdateInfo> const& u, bool isBeta);

    void loop();

    void storeSessionInfo();

};