#include "discord_state.h"

#include <fstream>
#include <regex>

DiscordState::DiscordState(PlayManager& playManager, ApkManager& apkManager) : playManager(playManager),
                                                                               apkManager(apkManager) {
    std::ifstream ifs("priv/discord.conf");
    discordConf.load(ifs);

    broadcastChannels = discordConf.get_array("broadcast_channels", {});
    broadcastChannelsW10 = discordConf.get_array("broadcast_channels_w10", {});
    std::vector<std::string> ops = discordConf.get_array("ops", {});
    operatorList = std::set<std::string>(ops.begin(), ops.end());

    for (auto const& s : discordConf.get_array("json_notify", {})) {
        auto j = s.find(';');
        if (j == std::string::npos)
            continue;
        JsonNotifyRule rule;
        rule.channel = s.substr(0, j);
        for (auto i = j + 1; i != std::string::npos; ) {
            j = s.find(';', i);
            auto text = j != std::string::npos ? s.substr(i, j - i) : s.substr(i);
            if (text == "W10Release")
                rule.notifyW10Release = true;
            if (text == "W10Beta")
                rule.notifyW10Beta = true;
            i = j != std::string::npos ? (j + 1) : j;
        }
        jsonNotifyRules.push_back(std::move(rule));
    }

    api.setBothAuth(discordConf.get("token"));
    conn.setToken(discordConf.get("token"));
    conn.setSessionId(discordConf.get("session_id"), discordConf.get_int("session_seq"));
    conn.setMessageCallback(std::bind(&DiscordState::onMessage, this, std::placeholders::_1));
    conn.connect(api);

    discord::gateway::StatusInfo status;
    status.since = std::chrono::system_clock::now();
    status.status = "online";
    status.activity.name = "over Mojang";
    status.activity.type = (discord::gateway::Activity::Type) 3;
    conn.setStatus(status);

    using namespace std::placeholders;
    apkManager.addNewVersionCallback(std::bind(&DiscordState::onNewVersion, this, _1, _2, _3, _4));
}

void DiscordState::addWin10StoreMgr(Win10StoreManager &mgr) {
    using namespace std::placeholders;
    mgr.addNewVersionCallback(std::bind(&DiscordState::onNewWin10Version, this, _1, _2));
    win10StoreManager = &mgr;
}

void DiscordState::onMessage(discord::Message const& m) {
    if (m.content.size() > 0 && m.content[0] == '!') {
        std::string command = m.content;
        auto it = command.find(' ');
        if (it != std::string::npos)
            command = m.content.substr(0, it);
        if (command == "!version" || command == "!get_version") {
            std::time_t t = std::chrono::system_clock::to_time_t(apkManager.getLastUpdateTime());
            char tt[512];
            if (!std::strftime(tt, sizeof(tt), "%F %T UTC", std::gmtime(&t)))
                tt[0] = '\0';

            discord::CreateMessageParams params ("Here's a list of the currently available Minecraft versions:");
            params.embed["title"] = "Minecraft versions";
            params.embed["fields"][0]["name"] = "Release";
            params.embed["fields"][0]["value"] = buildVersionFieldString(apkManager.getReleaseARMVersionInfo(),
                                                                         apkManager.getReleaseX86VersionInfo());
            params.embed["fields"][1]["name"] = "Beta";
            params.embed["fields"][1]["value"] = buildVersionFieldString(apkManager.getBetaARMVersionInfo(),
                                                                         apkManager.getBetaX86VersionInfo());
            params.embed["footer"]["text"] = std::string("Checked on ") + tt;
            try {
                api.createMessage(m.channel, params);
            } catch (std::exception& e) {
            }
        } else if (command == "!force_check" && checkOp(m)) {
            apkManager.requestForceCheck();
            api.createMessage(m.channel, "Did force check!");
        } else if (command == "!force_download_arm" && checkOp(m)) {
            try {
                apkManager.downloadAndProcessApk(playManager.getBetaDeviceARM(), std::stoi(m.content.substr(it + 1)));
            } catch(std::exception& e) {
                api.createMessage(m.channel, "Failed to download the apk");
            }
        } else if (command == "!kill" && checkOp(m)) {
            conn.disconnect();
        } else if (command == "!getip" && checkOp(m)) {
            try {
                playapi::http_request req ("http://api.ipify.org/");
                std::string ip = req.perform().get_body();
                api.createMessage(m.channel, "My IP is: " + ip);
            } catch (std::exception& e) {
                api.createMessage(m.channel, "Error getting IP");
            }
        } else if (command == "!link" && checkOp(m) && win10StoreManager) {
            if (it == std::string::npos || it + 1 >= m.content.size()) {
                api.createMessage(m.channel, "Missing required argument");
                return;
            }
            try {
                auto url = win10StoreManager->getDownloadUrl(m.content.substr(it + 1), 1);
                api.createMessage(m.channel, "Download URL: " + url);
            } catch (std::exception& e) {
                api.createMessage(m.channel, "Error getting URL");
            }
        }
    }
}

std::string DiscordState::buildVersionFieldString(ApkVersionInfo const& arm, ApkVersionInfo const& x86) {
    std::stringstream ss;
    ss << "**" << arm.versionString << "** "
       << " (ARM version code: " << arm.versionCode << ", x86 version code: " << x86.versionCode << ")";
    return ss.str();
}

bool DiscordState::checkOp(discord::Message const& m) {
    if (operatorList.count(m.author_id) > 0)
        return true;
    api.createMessage(m.channel, "Doesn't seem like you have the permissions to use this command, do you? ;)  " +
            std::string(m.author_id));
    return false;
}

void DiscordState::loop() {
    conn.loop();
}

void DiscordState::storeSessionInfo() {
    printf("Storing session information\n");
    discordConf.set("session_id", conn.getSession());
    discordConf.set_int("session_seq", conn.getSessionSeq());
    std::ofstream ofs("priv/discord.conf");
    discordConf.save(ofs);
}

void DiscordState::onNewVersion(int version, std::string const& versionString,
                                std::string const& changelog, std::string const& variant) {
    static std::regex regexNewlines ("<br>");

    bool isBeta = variant.length() >= 5 && memcmp(variant.c_str(), "beta/", 5) == 0;
    bool isARM = variant == "beta/arm" || variant == "release/arm";

    discord::CreateMessageParams params (isBeta ? "**New beta available**" : "**New version available**");
    params.embed["title"] = versionString + (isBeta ? " (beta)" : "");
    if (isARM)
        params.embed["description"] = std::regex_replace(changelog, regexNewlines, "\n");
    params.embed["footer"]["text"] = "Variant: " + variant + "; version code: " + std::to_string(version);

    if (!isARM)
        params.content = "";

    for (std::string const& chan : broadcastChannels) {
        api.createMessage(chan, params);
    }
}

void DiscordState::onNewWin10Version(std::vector<Win10StoreNetwork::UpdateInfo> const &u, bool isBeta) {
    discord::CreateMessageParams params (isBeta ? "**New Windows 10 beta**" : "**New Windows 10 release**");
    std::string desc;
    params.embed["fields"] = nlohmann::json::array();
    nlohmann::json jsonData = nlohmann::json::object();
    jsonData["isBeta"] = isBeta;
    jsonData["updates"] = nlohmann::json::array();
    for (auto const& e : u) {
        desc += "**" + e.packageMoniker + "**\n" + e.updateId + "\n";
        nlohmann::json val;
        val["name"] = e.packageMoniker;
        val["value"] = e.updateId;
        params.embed["fields"].push_back(val);

        nlohmann::json valj;
        valj["packageMoniker"] = e.packageMoniker;
        valj["updateId"] = e.updateId;
        valj["serverId"] = e.serverId;
        if (e.packageMoniker.find(".0_x64_") != std::string::npos)
            valj["downloadUrl"] = win10StoreManager->getDownloadUrl(e.updateId, 1);
        jsonData["updates"].push_back(valj);
    }
//    params.embed["description"] = desc;

    for (std::string const& chan : broadcastChannelsW10) {
        api.createMessage(chan, params);
    }

    for (auto const& r : jsonNotifyRules) {
        if (isBeta ? (!r.notifyW10Beta) : (!r.notifyW10Release))
            continue;

        api.createMessage(r.channel, "`" + jsonData.dump() + "`");
    }
}
