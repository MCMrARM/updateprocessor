#include <iostream>
#include <fstream>

#include "play_manager.h"
#include "discord.h"
#include "discord_gateway.h"

int main() {
    std::cout << "Starting up!" << std::endl;

    playapi::config discordConf;
    std::ifstream ifs("priv/discord.conf");
    discordConf.load(ifs);

    PlayManager playManager;
    //playManager.getDeviceARM().downloadApk("com.mojang.minecraftpe", 871021311, "priv/test.apk");

    discord::Api api;
    discord::gateway::Connection conn;
    conn.setToken(discordConf.get("token"));
    conn.connect(api);
    conn.setMessageCallback([&playManager](discord::gateway::Message const& m) {
        if (m.content.size() > 0 && m.content[0] == '!') {
            std::string command = m.content;
            auto it = command.find(' ');
            if (it == std::string::npos)
                command = m.content.substr(0, it);
            if (command == "!get_version") {
                auto detailsARM = playManager.getDeviceARM().getApi().details("com.mojang.minecraftpe");
                printf("version = %i\n", detailsARM.payload().detailsresponse().docv2().details().appdetails().versioncode());
            }
        }
    });

    discord::gateway::StatusInfo status;
    status.since = std::chrono::system_clock::now();
    status.status = "online";
    status.activity.name = "over Mojang";
    status.activity.type = (discord::gateway::Activity::Type) 3;
    conn.setStatus(status);

    conn.loop();

    return 0;
}