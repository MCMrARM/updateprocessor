#pragma once

#include <string>
#include <vector>

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
    std::string dataRoot, pendingRoot, activeRoot;

public:
    JobManager();

    void cleanUpDataDir();

    JobMeta createJob();

    void addApkJob(JobMeta const &meta, ApkJobDescription const &desc);

};