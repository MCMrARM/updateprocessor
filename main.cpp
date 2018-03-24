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

    //PlayManager playManager;
    //playManager.getDeviceARM().downloadApk("com.mojang.minecraftpe", 871021311, "priv/test.apk");
    discord::Api api;
    discord::gateway::Connection conn;
    conn.setToken(discordConf.get("token"));
    conn.connect(api);
    conn.loop();

    return 0;
}