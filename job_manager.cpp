#include "job_manager.h"
#include "file_utils.h"

#include <uuid/uuid.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <set>
#include <fstream>
#include <nlohmann/json.hpp>

JobManager::JobManager() : dataRoot("priv/jobs/data"), pendingRoot("priv/jobs/pending"), activeRoot("priv/jobs/active") {
    FileUtils::mkdirs(dataRoot);
    FileUtils::mkdirs(pendingRoot);
    FileUtils::mkdirs(activeRoot);
    cleanUpDataDir();
    handleJobTimeOut();
}

JobManager::~JobManager() {
    timeOutThreadMutex.lock();
    timeOutThreadStopped = true;
    timeOutThreadStopCv.notify_all();
    timeOutThreadMutex.unlock();
    if (timeOutThread.joinable())
        timeOutThread.join();
}

void JobManager::cleanUpDataDir() {
    dirent *ent;

    std::set<std::string> actualJobs;

    DIR *d = opendir(pendingRoot.c_str());
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] != '.')
            actualJobs.insert(ent->d_name);
    }
    closedir(d);
    d = opendir(activeRoot.c_str());
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] != '.')
            actualJobs.insert(ent->d_name);
    }
    closedir(d);

    d = opendir(dataRoot.c_str());
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] != '.' && actualJobs.count(ent->d_name) == 0) {
            printf("Found mismatched job directory: %s\n", ent->d_name);
            FileUtils::deleteDir(dataRoot + "/" + ent->d_name);
        }
    }
    closedir(d);
}

JobMeta JobManager::createJob() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char s[37];
    uuid_unparse(uuid, s);

    std::string dir = dataRoot + "/" + s;
    mkdir(dir.c_str(), 0700);
    return {s, dir};
}

void JobManager::addApkJob(JobMeta const &meta, ApkJobDescription const &desc) {
    nlohmann::json descJson = {
            {"type", "updateprocessor/addApkJob"},
            {"versionCode", desc.versionCode},
            {"apks", nlohmann::json::array()}
    };
    auto &versions = descJson["apks"];
    for (auto const &a : desc.apks) {
        versions.push_back({
            {"name", a.first},
            {"path", a.second},
        });
    }
    {
        std::ofstream descWriter(meta.dataDir + "/job.json");
        descWriter << descJson;
    }

    char *dataDirRpath = realpath(meta.dataDir.c_str(), nullptr);
    symlink(dataDirRpath, (pendingRoot + "/" + meta.uuid).c_str());
    free(dataDirRpath);
}

void JobManager::handleJobTimeOut() {
    DIR *d = opendir(activeRoot.c_str());
    dirent *ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.')
            continue;
        struct stat data;
        if (stat((activeRoot + "/" + ent->d_name).c_str(), &data))
            continue;
        if (time(nullptr) - data.st_mtim.tv_sec > 60 * 10) {
            printf("Job timed out: %s\n", ent->d_name);

            char buf[256];
            ssize_t ret = readlink((activeRoot + "/" + ent->d_name).c_str(), buf, sizeof(buf) - 1);
            if (ret < 0 || ret >= sizeof(buf) - 1) {
                printf("readlink failed\n");
                continue;
            }
            buf[ret] = '\0';
            remove((activeRoot + "/" + ent->d_name).c_str());
            symlink(buf, (pendingRoot + "/" + ent->d_name).c_str());
        }
    }
    closedir(d);
}

void JobManager::runJobTimeOutThread() {
    std::unique_lock<std::mutex> lk(timeOutThreadMutex);
    while (!timeOutThreadStopped) {
        handleJobTimeOut();

        auto until = std::chrono::system_clock::now() + std::chrono::minutes(10);
        timeOutThreadStopCv.wait_until(lk, until);
    }
}

void JobManager::startTimeOutThread() {
    timeOutThread = std::thread(std::bind(&JobManager::runJobTimeOutThread, this));
}