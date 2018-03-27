#pragma once

#include <string>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <playapi/util/config.h>

class ApkUploader {

private:
    std::set<std::string> files;
    playapi::config conf;
    std::string apiAddress;
    std::string apiToken;
    char wakeOnLanAddr[6];

    std::thread thread;
    std::mutex mutex;
    std::condition_variable condvar;
    bool stopped = false;

    void sendWakeOnLan(const char* mac);

    bool checkIsOnline();

    void uploadFile(std::string const& path, bool wol = false);

    void saveFileList();

    void handleWork();

public:
    ApkUploader();

    ~ApkUploader();

    void addFile(std::string const& path);

};