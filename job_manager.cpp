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