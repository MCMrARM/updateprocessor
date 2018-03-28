#pragma once

#include "play_device.h"

class PlayManager {

private:
    app_config releaseAppConfig;
    PlayDevice releaseDeviceARM, releaseDeviceX86;

    app_config betaAppConfig;
    PlayDevice betaDeviceARM, betaDeviceX86;

public:
    PlayManager() : releaseAppConfig("priv/playdl_release.conf"),
                    releaseDeviceARM(releaseAppConfig, "priv/device_arm_release.conf"),
                    releaseDeviceX86(releaseAppConfig, "priv/device_x86_release.conf"),
                    betaAppConfig("priv/playdl_beta.conf"),
                    betaDeviceARM(betaAppConfig, "priv/device_arm_beta.conf"),
                    betaDeviceX86(betaAppConfig, "priv/device_x86_beta.conf") {
    }

    PlayDevice& getReleaseDeviceARM() { return releaseDeviceARM; }
    PlayDevice& getReleaseDeviceX86() { return releaseDeviceX86; }

    PlayDevice& getBetaDeviceARM() { return betaDeviceARM; }
    PlayDevice& getBetaDeviceX86() { return betaDeviceX86; }


};