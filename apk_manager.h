#pragma once

#include "play_manager.h"

struct ApkVersionInfo {
    int versionCode;
    std::string versionString;
};

class ApkManager {

private:

    static std::string getOutputFilePath(int version, std::string const& versionString, std::string const& archStr);

    PlayManager& playManager;
    ApkVersionInfo armVersionInfo, x86VersionInfo;

public:

    ApkManager(PlayManager& playManager) : playManager(playManager) { }

    void updateLatestVersions();
    void maybeUpdateLatestVersions() { updateLatestVersions(); }

    ApkVersionInfo const& getARMVersionInfo() const { return armVersionInfo; }
    ApkVersionInfo const& getX86VersionInfo() const { return x86VersionInfo; }

    void downloadAndProcessApk(PlayDevice& device, int version, std::string const& versionString,
                               std::string const& archStr);

    void downloadAndProcessApks();

    void sendApkForAnalytics(std::string const& path);

};