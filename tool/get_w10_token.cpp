#include "../win10_store_manager.h"
#include <iostream>

int main() {
    Win10StoreManager win10Manager;
    win10Manager.init();
    auto token = win10Manager.getMsaToken();
    std::cout << std::endl;
    std::cout << "Token: " << token;
    return 0;
}