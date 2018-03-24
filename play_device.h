#pragma once

#include <playapi/api.h>
#include <playapi/login.h>
#include <playapi/device_info.h>
#include "playapi/src/config.h"
#include <memory>
#include <playapi/mcs_registration_api.h>

class PlayManager;

class PlayDevice {

private:
    playapi::device_info device;
    device_config deviceConfig;
    playapi::login_api login;
    playapi::api api;
    playapi::mcs_registration_api apiMcs;

    static playapi::device_info loadDeviceInfo(std::string const& devicePath);

    static device_config loadDeviceStateInfo(playapi::device_info& device, std::string const& devicePath);

    static playapi::login_api loadLoginInfo(playapi::device_info& device, device_config& deviceConfig,
                                            app_config const& appConfig);

    void checkTos();

    static void storeAuthCookies(device_config& device, playapi::login_api& login);

public:
    PlayDevice(app_config const& appConfig, std::string const& devicePath) :
            device(loadDeviceInfo(devicePath)), deviceConfig(loadDeviceStateInfo(device, devicePath)),
            login(loadLoginInfo(device, deviceConfig, appConfig)), api(device), apiMcs(device) {
        api.set_auth(login);
        api.set_checkin_data(deviceConfig.checkin_data);
        apiMcs.set_checkin_data(deviceConfig.checkin_data);
        deviceConfig.load_api_data(login.get_email(), api);
        checkTos();
    }

    inline playapi::api& getApi() { return api; }

    inline playapi::login_api& getLoginApi() { return login; }

    inline playapi::mcs_registration_api& getMcsApi() { return apiMcs; }

    inline playapi::checkin_result const& getCheckinData() { return deviceConfig.checkin_data; }

    void registerMCS();

    void doContentSync();

    void downloadApk(std::string const& packageName, int packageVersion, std::string const& downloadTo);

};