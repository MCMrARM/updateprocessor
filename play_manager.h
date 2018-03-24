#pragma once

#include "play_device.h"

class PlayManager {

private:
    app_config appConfig;
    PlayDevice deviceARM, deviceX86;

public:
    PlayManager() : appConfig("priv/playdl.conf"),
                    deviceARM(appConfig, "priv/device_arm.conf"),
                    deviceX86(appConfig, "priv/device_x86.conf") {
    }

    PlayDevice& getDeviceARM() { return deviceARM; }
    PlayDevice& getDeviceX86() { return deviceX86; }


};