#pragma once

#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "play_manager.h"
#include "apk_uploader.h"

struct ApkVersionInfo {
    int versionCode = -1;
    int lastDownloadedVersionCode = -1;
    std::string versionString;

    void loadFromConfig(playapi::config const& config, std::string const& prefix);
    void saveToConfig(playapi::config& config, std::string const& prefix);
};

class ApkManager {

public:
    using NewVersionCallback = std::function<void (int version, std::string const& versionString,
                                                   std::string const& changelog, std::string const& variant)>;

private:

    static const char* PKG_NAME;

    std::thread thread;
    std::mutex thread_mutex, data_mutex, cb_mutex;
    std::condition_variable stop_cv;
    bool stopped = false;

    PlayManager& playManager;
    ApkUploader uploader;
    std::vector<NewVersionCallback> newVersionCallback;
    playapi::config versionCheckConfig;
    ApkVersionInfo releaseARMVersionInfo, releaseX86VersionInfo;
    ApkVersionInfo betaARMVersionInfo, betaX86VersionInfo;
    std::chrono::system_clock::time_point lastVersionUpdate;

    void saveVersionInfo();

    void runVersionCheckThread();

    void updateLatestVersions();

    struct CheckResult {
        int versionCode;
        std::string changelog;
        bool hasNewVersion;
        bool shouldDownload;
    };

    CheckResult updateLatestVersion(PlayDevice& device, ApkVersionInfo& versionInfo);

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

    void addNewVersionCallback(NewVersionCallback callback) {
        std::lock_guard<std::mutex> lk(cb_mutex);
        newVersionCallback.emplace_back(callback);
    }

    ApkVersionInfo getReleaseARMVersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return releaseARMVersionInfo;
    }
    ApkVersionInfo getReleaseX86VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return releaseX86VersionInfo;
    }
    ApkVersionInfo getBetaARMVersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return betaARMVersionInfo;
    }
    ApkVersionInfo getBetaX86VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return betaX86VersionInfo;
    }
    std::chrono::system_clock::time_point getLastUpdateTime() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return lastVersionUpdate;
    }

    void downloadAndProcessApk(PlayDevice& device, int version);

    void requestForceCheck();

};