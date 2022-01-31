#pragma once

#include "play_device.h"

class PlayManager {

private:
    app_config releaseAppConfig;
    PlayDevice releaseDeviceARM, releaseDeviceX86, releaseDeviceARM64, releaseDeviceX8664;

    app_config betaAppConfig;
    PlayDevice betaDeviceARM, betaDeviceX86, betaDeviceARM64, betaDeviceX8664;

public:
    PlayManager() : releaseAppConfig("priv/playdl_release.conf"),
                    releaseDeviceARM(releaseAppConfig, "priv/device_arm.conf", "priv/device_arm_release_state.conf"),
                    releaseDeviceX86(releaseAppConfig, "priv/device_x86.conf", "priv/device_x86_release_state.conf"),
                    releaseDeviceARM64(releaseAppConfig, "priv/device_arm64.conf", "priv/device_arm64_release_state.conf"),
                    releaseDeviceX8664(releaseAppConfig, "priv/device_x86_64.conf", "priv/device_x86_64_release_state.conf"),
                    betaAppConfig("priv/playdl_beta.conf"),
                    betaDeviceARM(betaAppConfig, "priv/device_arm.conf", "priv/device_arm_beta_state.conf"),
                    betaDeviceX86(betaAppConfig, "priv/device_x86.conf", "priv/device_x86_beta_state.conf"),
                    betaDeviceARM64(betaAppConfig, "priv/device_arm64.conf", "priv/device_arm64_beta_state.conf"),
                    betaDeviceX8664(betaAppConfig, "priv/device_x86_64.conf", "priv/device_x86_64_beta_state.conf") {
    }

    PlayDevice& getReleaseDeviceARM() { return releaseDeviceARM; }
    PlayDevice& getReleaseDeviceX86() { return releaseDeviceX86; }
    PlayDevice& getReleaseDeviceARM64() { return releaseDeviceARM64; }
    PlayDevice& getReleaseDeviceX8664() { return releaseDeviceX8664; }

    PlayDevice& getBetaDeviceARM() { return betaDeviceARM; }
    PlayDevice& getBetaDeviceX86() { return betaDeviceX86; }
    PlayDevice& getBetaDeviceARM64() { return betaDeviceARM64; }
    PlayDevice& getBetaDeviceX8664() { return betaDeviceX8664; }

    void deleteStateData();

};