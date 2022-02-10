#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <set>
#include <thread>
#include <condition_variable>
#include <msa/simple_storage_manager.h>
#include <msa/login_manager.h>
#include <msa/account_manager.h>
#include "win10_store_network.h"

enum class Win10VersionType {
    Release, Beta, Preview
};

class Win10StoreManager {

public:
    using NewVersionCallback = std::function<void (std::vector<Win10StoreNetwork::UpdateInfo> const& update,
            Win10VersionType versionType, bool hasAnyNewPackageMoniker)>;

private:
    static const char* const MINECRAFT_APP_ID;
    static const char* const MINECRAFT_PREVIEW_APP_ID;

    std::thread thread;
    std::mutex threadMutex, dataMutex;
    std::condition_variable stopCv;
    bool stopped = false;
    std::set<std::string> knownVersions;
    std::set<std::string> knownVersionsWithAccount;
    std::set<std::string> knownPackageMonikers;
    std::mutex newVersionMutex;
    std::vector<NewVersionCallback> newVersionCallback;
    msa::SimpleStorageManager msaStorage;
    msa::LoginManager msaLoginManager;
    msa::AccountManager msaAccountManager;
    std::shared_ptr<msa::Account> msaAccount;
    Win10StoreNetwork wuAnonymous;
    Win10StoreNetwork wuWithAccount;
    Win10StoreNetwork::CookieData cookieAnonymous;
    Win10StoreNetwork::CookieData cookieWithAccount;
    std::chrono::system_clock::time_point lastSuccessfulCheck;

    void loadConfig();

    void saveConfig();

    void runVersionCheckThread();

    void checkVersion(Win10StoreNetwork& net, Win10StoreNetwork::CookieData& cookie,
            std::set<std::string>& knownVersions, Win10VersionType versionType);

public:
    Win10StoreManager() : msaStorage("priv/msa/"), msaLoginManager(&msaStorage), msaAccountManager(msaStorage) {}

    ~Win10StoreManager() {
        threadMutex.lock();
        stopped = true;
        stopCv.notify_all();
        threadMutex.unlock();
        if (thread.joinable())
            thread.join();
    }

    void addNewVersionCallback(NewVersionCallback callback) {
        std::lock_guard<std::mutex> lk(newVersionMutex);
        newVersionCallback.emplace_back(callback);
    }

    void init();

    void startChecking();

    std::string getMsaToken();

    std::string getDownloadUrl(std::string const& updateId, int revisionNumber);

    std::chrono::system_clock::time_point getLastSuccessfulCheck() {
        std::lock_guard<std::mutex> lk(dataMutex);
        return lastSuccessfulCheck;
    }

};