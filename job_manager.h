#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

struct JobMeta {
    std::string uuid;
    std::string dataDir;
};

struct ApkJobDescription {
    int versionCode;
    std::vector<std::pair<std::string, std::string>> apks;
};

class JobManager {

private:
    const std::string dataRoot, pendingRoot, activeRoot;
    std::thread timeOutThread;
    std::mutex timeOutThreadMutex;
    bool timeOutThreadStopped = false;
    std::condition_variable timeOutThreadStopCv;

    void runJobTimeOutThread();

public:
    JobManager();

    ~JobManager();

    void cleanUpDataDir();

    void handleJobTimeOut();

    void startTimeOutThread();

    JobMeta createJob();

    void addApkJob(JobMeta const &meta, ApkJobDescription const &desc);

};