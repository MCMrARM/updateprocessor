#pragma once

#include "win10_store_network.h"

class Win10StoreManager {

private:
    Win10StoreNetwork::CookieData cookie;

private:
    void loadConfig();

    void saveConfig();

public:
    void init();

    void checkVersion();

};