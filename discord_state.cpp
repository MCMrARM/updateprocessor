#include "discord_state.h"

#include <fstream>
#include <regex>

DiscordState::DiscordState(PlayManager& playManager, ApkManager& apkManager) : playManager(playManager),
                                                                               apkManager(apkManager) {
    std::ifstream ifs("priv/discord.conf");
    discordConf.load(ifs);

    broadcastChannels = discordConf.get_array("broadcast_channels", {});

    api.setBothAuth(discordConf.get("token"));
    conn.setToken(discordConf.get("token"));
    conn.setSessionId(discordConf.get("session_id"), discordConf.get_int("session_seq"));
    conn.connect(api);
    conn.setMessageCallback(std::bind(&DiscordState::onMessage, this, std::placeholders::_1));

    discord::gateway::StatusInfo status;
    status.since = std::chrono::system_clock::now();
    status.status = "online";
    status.activity.name = "over Mojang";
    status.activity.type = (discord::gateway::Activity::Type) 3;
    conn.setStatus(status);

    using namespace std::placeholders;
    apkManager.setNewVersionCallback(std::bind(&DiscordState::onNewVersion, this, _1, _2, _3));
}

void DiscordState::onMessage(discord::Message const& m) {
    if (m.content.size() > 0 && m.content[0] == '!') {
        std::string command = m.content;
        auto it = command.find(' ');
        if (it == std::string::npos)
            command = m.content.substr(0, it);
        if (command == "!get_version") {
            std::time_t t = std::chrono::system_clock::to_time_t(apkManager.getLastUpdateTime());
            char tt[512];
            if (!std::strftime(tt, sizeof(tt), "%F %T UTC", std::gmtime(&t)))
                tt[0] = '\0';

            std::stringstream ss;
            ss << "Current Minecraft version: " << apkManager.getVersionString()
               << " (ARM version code: " << apkManager.getARMVersionInfo().versionCode << ", "
               << "X86 version code: " << apkManager.getX86VersionInfo().versionCode << "; "
               << "checked on " << tt << ")";
            api.createMessage(m.channel, ss.str());
        } else if (command == "!force_download") {
            // apkManager.downloadAndProcessApks();
        }
    }
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

void DiscordState::onNewVersion(int version, std::string const& changelog, std::string const& variant) {
    static std::regex regexNewlines ("<br>");

    discord::CreateMessageParams params ("**New version available**");
    params.embed["title"] = apkManager.getVersionString();
    if (variant == "arm")
        params.embed["description"] = std::regex_replace(changelog, regexNewlines, "\n");
    params.embed["footer"]["text"] = "Variant: " + variant + "; version code: " + std::to_string(version);

    if (variant != "arm")
        params.content = "";

    for (std::string const& chan : broadcastChannels) {
        api.createMessage(chan, params);
    }
}