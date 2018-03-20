#include <iostream>

#include "play_manager.h"

int main() {
    std::cout << "Starting up!" << std::endl;

    PlayManager playManager;
    playManager.getDeviceARM().downloadApk("com.mojang.minecraftpe", 871021311, "priv/test.apk");

    return 0;
}