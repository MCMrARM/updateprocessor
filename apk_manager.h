#pragma once

#include "play_manager.h"

struct ApkVersionInfo {
    int versionCode = -1;
    int lastDownloadedVersionCode = -1;
};

class ApkManager {

private:

    PlayManager& playManager;
    ApkVersionInfo armVersionInfo, x86VersionInfo;
    std::string armVersionString;

public:

    ApkManager(PlayManager& playManager) : playManager(playManager) { }

    void updateLatestVersions();
    void maybeUpdateLatestVersions() { updateLatestVersions(); }

    std::string const& getVersionString() const { return armVersionString; }
    ApkVersionInfo const& getARMVersionInfo() const { return armVersionInfo; }
    ApkVersionInfo const& getX86VersionInfo() const { return x86VersionInfo; }

    void downloadAndProcessApk(PlayDevice& device, int version);

    void downloadAndProcessApks();

    void sendApkForAnalytics(std::string const& path);

};