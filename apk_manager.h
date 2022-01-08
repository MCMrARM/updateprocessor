#pragma once

#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "play_manager.h"
#include "job_manager.h"

struct ApkVersionInfo {
    int versionCode = -1;
    int lastDownloadedVersionCode = -1;
    std::string versionString;
    std::chrono::system_clock::time_point lastSuccess;

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
    JobManager&jobManager;
    std::vector<NewVersionCallback> newVersionCallback;
    playapi::config versionCheckConfig;
    ApkVersionInfo releaseARMVersionInfo, releaseARM64VersionInfo, releaseX86VersionInfo, releaseX8664VersionInfo;
    ApkVersionInfo betaARMVersionInfo, betaARM64VersionInfo, betaX86VersionInfo, betaX8664VersionInfo;
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

    ApkManager(PlayManager& playManager, JobManager& jobManager);

    ~ApkManager() {
        thread_mutex.lock();
        stopped = true;
        stop_cv.notify_all();
        thread_mutex.unlock();
        if (thread.joinable())
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
    ApkVersionInfo getReleaseARM64VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return releaseARM64VersionInfo;
    }
    ApkVersionInfo getReleaseX8664VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return releaseX8664VersionInfo;
    }
    ApkVersionInfo getBetaARMVersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return betaARMVersionInfo;
    }
    ApkVersionInfo getBetaX86VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return betaX86VersionInfo;
    }
    ApkVersionInfo getBetaARM64VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return betaARM64VersionInfo;
    }
    ApkVersionInfo getBetaX8664VersionInfo() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return betaX8664VersionInfo;
    }
    std::chrono::system_clock::time_point getLastUpdateTime() {
        std::lock_guard<std::mutex> lk(data_mutex);
        return lastVersionUpdate;
    }

    void downloadAndProcessApk(PlayDevice& device, int version, bool onlyNatives = false);

    void requestForceCheck();

};