#pragma once

#include "apk_manager.h"
#include "telegram.h"

class TelegramState {

private:
    telegram::Api api;
    playapi::config config;
    std::vector<std::string> broadcastTo;

public:
    TelegramState(ApkManager& apkManager);

    void onNewVersion(int version, std::string const& versionString,
                      std::string const& changelog, std::string const& variant);

    void broadcastMessage(std::string const& text, std::string const& parseMode = std::string());

};