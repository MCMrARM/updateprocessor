#include "win10_store_manager.h"

#include <fstream>
#include <playapi/util/config.h>

void Win10StoreManager::init() {
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
    cookie.encryptedData = conf.get("cookie.encryptedData");
    cookie.expiration = conf.get("cookie.expiration");
}

void Win10StoreManager::saveConfig() {
    playapi::config conf;
    if (!cookie.encryptedData.empty()) {
        conf.set("cookie.encryptedData", cookie.encryptedData);
        conf.set("cookie.expiration", cookie.expiration);
    }
    std::ofstream ifs("priv/win10.conf");
    conf.save(ifs);
}

void Win10StoreManager::checkVersion() {
    Win10StoreNetwork::syncVersion(cookie);
}