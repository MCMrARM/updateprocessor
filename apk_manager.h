#pragma once

#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "play_manager.h"
#include "apk_uploader.h"

struct ApkVersionInfo {
    int versionCode = -1;
    int lastDownloadedVersionCode = -1;

    void loadFromConfig(playapi::config const& config, std::string const& prefix);
    void saveToConfig(playapi::config& config, std::string const& prefix);
};

class ApkManager {

public:
    using NewVersionCallback = std::function<void (int version, std::string const& changelog,
                                                   std::string const& variant)>;

private:

    std::thread thread;
    std::mutex thread_mutex, data_mutex, cb_mutex;
    std::condition_variable stop_cv;
    bool stopped = false;

    PlayManager& playManager;
    ApkUploader uploader;
    NewVersionCallback newVersionCallback;
    playapi::config versionCheckConfig;
    ApkVersionInfo armVersionInfo, x86VersionInfo;
    std::string armVersionString;
    std::chrono::system_clock::time_point lastVersionUpdate;

    void saveVersionInfo();

    void runVersionCheckThread();

    void updateLatestVersions();

    void downloadAndProcessApk(PlayDevice& device, int version, ApkVersionInfo& info);

public:

    ApkManager(PlayManager& playManager);

    ~ApkManager() {
        thread_mutex.lock();
        stopped = true;
        stop_cv.notify_all();
        thread_mutex.unlock();
        thread.join();
    }

    void startChecking();

    void setNewVersionCallback(NewVersionCallback callback) {
        std::lock_guard<std::mutex> lk(cb_mutex);
        newVersionCallback = callback;
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

};