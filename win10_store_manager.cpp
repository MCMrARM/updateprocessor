#include "win10_store_manager.h"

#include <fstream>
#include <playapi/util/config.h>

void Win10StoreManager::init() {
    std::lock_guard<std::mutex> dataLock (dataMutex);
    loadConfig();

    if (cookie.encryptedData.empty()) {
        cookie = Win10StoreNetwork::fetchCookie();
        saveConfig();
    }
}

void Win10StoreManager::loadConfig() {
    playapi::config conf;
    std::ifstream ifs("priv/win10.conf");
    conf.load(ifs);
    cookie.encryptedData = conf.get("cookie.encrypted_data");
    cookie.expiration = conf.get("cookie.expiration");
    for (std::string const& v : conf.get_array("known_versions"))
        knownVersions.insert(v);
}

void Win10StoreManager::saveConfig() {
    playapi::config conf;
    if (!cookie.encryptedData.empty()) {
        conf.set("cookie.encrypted_data", cookie.encryptedData);
        conf.set("cookie.expiration", cookie.expiration);
    }
    std::vector<std::string> knownVersionsV;
    std::copy(knownVersions.begin(), knownVersions.end(), std::back_inserter(knownVersionsV));
    conf.set_array("known_versions", std::move(knownVersionsV));
    std::ofstream ifs("priv/win10.conf");
    conf.save(ifs);
}

void Win10StoreManager::checkVersion() {
    std::unique_lock<std::mutex> dataLock (dataMutex);
    auto res = Win10StoreNetwork::syncVersion(cookie);
    bool hasAnyNewVersions = false;
    std::vector<Win10StoreNetwork::UpdateInfo> newUpdates;
    for (auto const& e : res.newUpdates) {
        if (strncmp(e.packageMoniker.c_str(), "Microsoft.MinecraftUWP_", sizeof("Microsoft.MinecraftUWP_") - 1) == 0) {
            std::string mergedString = e.serverId + " " + e.updateId + " " + e.packageMoniker;
            if (knownVersions.count(mergedString) > 0)
                continue;
            printf("New UWP version: %s\n", mergedString.c_str());
            hasAnyNewVersions = true;
            knownVersions.insert(mergedString);
            newUpdates.push_back(e);
        }
    }
    std::sort(newUpdates.begin(), newUpdates.end(), [](Win10StoreNetwork::UpdateInfo const& a,
            Win10StoreNetwork::UpdateInfo const& b) {
        return a.packageMoniker < b.packageMoniker;
    });
    if (!res.newCookie.encryptedData.empty())
        cookie = res.newCookie;
    dataLock.unlock();
    if (hasAnyNewVersions) {
        std::lock_guard<std::mutex> lk(newVersionMutex);
        for (NewVersionCallback const &cb : newVersionCallback)
            cb(newUpdates);
    }
    dataLock.lock();
    saveConfig();
    dataLock.unlock();
}

void Win10StoreManager::startChecking() {
    thread = std::thread(std::bind(&Win10StoreManager::runVersionCheckThread, this));
}

void Win10StoreManager::runVersionCheckThread() {
    std::unique_lock<std::mutex> lk(threadMutex);
    while (!stopped) {
        checkVersion();

        auto until = std::chrono::system_clock::now() + std::chrono::minutes(10);
        stopCv.wait_until(lk, until);
    }
}
