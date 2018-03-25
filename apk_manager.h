#pragma once

#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "play_manager.h"

struct ApkVersionInfo {
    int versionCode = -1;
    int lastDownloadedVersionCode = -1;

    void loadFromConfig(playapi::config const& config, std::string const& prefix);
    void saveToConfig(playapi::config& config, std::string const& prefix);
};

class ApkManager {

private:

    std::thread thread;
    std::mutex thread_mutex, data_mutex;
    std::condition_variable stop_cv;
    bool stopped = false;

    PlayManager& playManager;
    playapi::config versionCheckConfig;
    ApkVersionInfo armVersionInfo, x86VersionInfo;
    std::string armVersionString;
    std::chrono::system_clock::time_point lastVersionUpdate;

    void saveVersionInfo();

    void runVersionCheckThread();

    void updateLatestVersions();

public:

    ApkManager(PlayManager& playManager);

    ~ApkManager() {
        thread_mutex.lock();
        stopped = true;
        thread_mutex.unlock();
        thread.join();
    }

    std::string getVersionString() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return armVersionString;
    }
    ApkVersionInfo getARMVersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return armVersionInfo;
    }
    ApkVersionInfo getX86VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return x86VersionInfo;
    }
    std::chrono::system_clock::time_point getLastUpdateTime() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return lastVersionUpdate;
    }

    void downloadAndProcessApk(PlayDevice& device, int version);

    void downloadAndProcessApks();

    void sendApkForAnalytics(std::string const& path);

};