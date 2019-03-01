#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <set>
#include <thread>
#include <condition_variable>
#include "win10_store_network.h"

class Win10StoreManager {

public:
    using NewVersionCallback = std::function<void (std::vector<Win10StoreNetwork::UpdateInfo> const& update)>;

private:
    std::thread thread;
    std::mutex threadMutex, dataMutex;
    std::condition_variable stopCv;
    bool stopped = false;
    Win10StoreNetwork::CookieData cookie;
    std::set<std::string> knownVersions;
    std::mutex newVersionMutex;
    std::vector<NewVersionCallback> newVersionCallback;

    void loadConfig();

    void saveConfig();

    void runVersionCheckThread();

    void checkVersion();

public:
    ~Win10StoreManager() {
        threadMutex.lock();
        stopped = true;
        stopCv.notify_all();
        threadMutex.unlock();
        thread.join();
    }

    void addNewVersionCallback(NewVersionCallback callback) {
        std::lock_guard<std::mutex> lk(newVersionMutex);
        newVersionCallback.emplace_back(callback);
    }

    void init();

    void startChecking();

};