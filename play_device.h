#pragma once

#include <playapi/api.h>
#include <playapi/login.h>
#include <playapi/device_info.h>
#include "playapi/src/config.h"
#include <memory>

class PlayManager;

class PlayDevice {

private:
    playapi::device_info device;
    device_config deviceConfig;
    playapi::login_api login;
    playapi::api api;

    static playapi::device_info loadDeviceInfo(std::string const& devicePath);
    static device_config loadDeviceStateInfo(playapi::device_info& device, std::string const& devicePath);
    static playapi::login_api loadLoginInfo(playapi::device_info& device, device_config& deviceConfig,
                                            app_config const& appConfig);

    void checkTos();

public:
    PlayDevice(app_config const& appConfig, std::string const& devicePath) :
            device(loadDeviceInfo(devicePath)), deviceConfig(loadDeviceStateInfo(device, devicePath)),
            login(loadLoginInfo(device, deviceConfig, appConfig)), api(device) {
        api.set_auth(login);
        api.set_checkin_data(deviceConfig.checkin_data);
        deviceConfig.load_api_data(login.get_email(), api);
        checkTos();
    }

    void downloadApk(std::string const& packageName, int packageVersion, std::string const& downloadTo);

};