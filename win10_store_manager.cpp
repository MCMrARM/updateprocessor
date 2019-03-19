#include "win10_store_manager.h"

#include <fstream>
#include <playapi/util/config.h>
#include <iostream>
#include <base64.h>
#include <msa/token_response.h>
#include <msa/compact_token.h>
#include <codecvt>

void Win10StoreManager::init() {
    std::lock_guard<std::mutex> dataLock (dataMutex);
    loadConfig();

    auto acc = msaAccountManager.getAccounts();
    if (!acc.empty())
        msaAccount = msaAccountManager.findAccount(acc.at(0).getCID());

    if (cookieAnonymous.encryptedData.empty()) {
        cookieAnonymous = wuWithAccount.fetchCookie();
        saveConfig();
    }
    if (cookieWithAccount.encryptedData.empty()) {
        cookieWithAccount = wuWithAccount.fetchCookie();
        saveConfig();
    }
}

void Win10StoreManager::loadConfig() {
    playapi::config conf;
    std::ifstream ifs("priv/win10.conf");
    conf.load(ifs);
    cookieAnonymous.encryptedData = conf.get("cookie.encrypted_data");
    cookieAnonymous.expiration = conf.get("cookie.expiration");
    cookieWithAccount.encryptedData = conf.get("cookie_with_account.encrypted_data");
    cookieWithAccount.expiration = conf.get("cookie_with_account.expiration");
    for (std::string const& v : conf.get_array("known_versions"))
        knownVersions.insert(v);
    for (std::string const& v : conf.get_array("known_versions_with_account"))
        knownVersionsWithAccount.insert(v);
}

void Win10StoreManager::saveConfig() {
    playapi::config conf;
    if (!cookieAnonymous.encryptedData.empty()) {
        conf.set("cookie.encrypted_data", cookieAnonymous.encryptedData);
        conf.set("cookie.expiration", cookieAnonymous.expiration);
    }
    if (!cookieWithAccount.encryptedData.empty()) {
        conf.set("cookie_with_account.encrypted_data", cookieWithAccount.encryptedData);
        conf.set("cookie_with_account.expiration", cookieWithAccount.expiration);
    }
    std::vector<std::string> knownVersionsV;
    std::copy(knownVersions.begin(), knownVersions.end(), std::back_inserter(knownVersionsV));
    conf.set_array("known_versions", std::move(knownVersionsV));
    knownVersionsV.clear();
    std::copy(knownVersionsWithAccount.begin(), knownVersionsWithAccount.end(), std::back_inserter(knownVersionsV));
    conf.set_array("known_versions_with_account", std::move(knownVersionsV));
    {
        std::ofstream ifs("priv/win10.conf.new");
        conf.save(ifs);
    }
    rename("priv/win10.conf.new", "priv/win10.conf");
}

std::string Win10StoreManager::getMsaToken() {
    if (!msaAccount)
        return std::string();
    auto tk = msaAccount->requestTokens(msaLoginManager, {{"scope=service::dcat.update.microsoft.com::MBI_SSL&uaid=287A187A-3BFE-4D0B-B93E-3C51134EBD55&clientid=%7B28520974-CE92-4F36-A219-3F255AF7E61E%7D", "TOKEN_BROKER"}}, "{28520974-CE92-4F36-A219-3F255AF7E61E}");
    if (tk.empty())
        throw std::runtime_error("requestTokens failed");
    auto tke = tk.begin();
    auto tkd = msa::token_pointer_cast<msa::CompactToken>(tke->second.getToken())->getBinaryToken();
    std::string tkdw;
    tkdw.resize(tkd.size() * 2, 0);
    for (int i = 0; i < tkd.size(); i++)
        tkdw[i * 2] = tkd[i];
    return Base64::encode(tkdw);
}

void Win10StoreManager::checkVersion(Win10StoreNetwork& net, Win10StoreNetwork::CookieData& cookie,
        std::set<std::string>& knownVersions, bool isBeta) {
    std::unique_lock<std::mutex> dataLock (dataMutex);
    Win10StoreNetwork::SyncResult res;
    try {
        res = net.syncVersion(cookie);
    } catch (std::exception& e) {
        printf("Win10 version check failed: %s\n", e.what());
        return;
    }
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
            cb(newUpdates, isBeta);
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
        checkVersion(wuAnonymous, cookieAnonymous, knownVersions, false);
        {
            std::lock_guard<std::mutex> dataLock (dataMutex);
            try {
                wuWithAccount.setAuthTokenBase64(getMsaToken());
            } catch (std::exception& e) {
                printf("Win10 token refresh failed: %s\n", e.what());
                continue;
            }
        }
        checkVersion(wuWithAccount, cookieWithAccount, knownVersionsWithAccount, true);

        auto until = std::chrono::system_clock::now() + std::chrono::minutes(10);
        stopCv.wait_until(lk, until);
    }
}

std::string Win10StoreManager::getDownloadUrl(std::string const &updateId, int revisionNumber) {
    std::lock_guard<std::mutex> dataLock (dataMutex);
    wuWithAccount.setAuthTokenBase64(getMsaToken());
    auto resp = wuWithAccount.getDownloadLink(updateId, revisionNumber);
    for (auto const& file : resp.files) {
        const char* baseUrl = "http://tlu.dl.delivery.mp.microsoft.com/";
        if (strncmp(file.url.data(), baseUrl, strlen(baseUrl)) == 0) {
            return file.url;
        }
    }
    return std::string();
}