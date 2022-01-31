#include "play_manager.h"

#include <unistd.h>

void PlayManager::deleteStateData() {
    unlink(getReleaseDeviceARM().getStatePath().c_str());
    unlink(getReleaseDeviceX86().getStatePath().c_str());
    unlink(getReleaseDeviceARM64().getStatePath().c_str());
    unlink(getReleaseDeviceX8664().getStatePath().c_str());
    unlink(getBetaDeviceARM().getStatePath().c_str());
    unlink(getBetaDeviceX86().getStatePath().c_str());
    unlink(getBetaDeviceARM64().getStatePath().c_str());
    unlink(getBetaDeviceX8664().getStatePath().c_str());
}