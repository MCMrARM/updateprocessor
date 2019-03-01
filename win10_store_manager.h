#pragma once

#include "win10_store_network.h"

#include <string>
#include <set>

class Win10StoreManager {

private:
    Win10StoreNetwork::CookieData cookie;
    std::set<std::string> knownVersions;

private:
    void loadConfig();

    void saveConfig();

public:
    void init();

    void checkVersion();

};