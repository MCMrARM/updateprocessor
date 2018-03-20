#include <iostream>

#include "play_manager.h"

int main() {
    std::cout << "Starting up!" << std::endl;

    PlayManager playManager;
    playManager.getDeviceARM().downloadApk("com.mojang.minecraftpe");

    return 0;
}