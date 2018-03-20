#include "play_device.h"

#include <fstream>

playapi::device_info PlayDevice::loadDeviceInfo(std::string const& devicePath) {
    std::ifstream devInfoFile(devicePath);
    playapi::device_info device;
    playapi::config conf;
    conf.load(devInfoFile);
    device.load(conf);
    return device;
}

device_config PlayDevice::loadDeviceStateInfo(playapi::device_info& device, std::string const& devicePath) {
    device_config devState(devicePath + ".state");
    devState.load();
    devState.load_device_info_data(device);
    device.generate_fields();
    devState.set_device_info_data(device);
    devState.save();
    return devState;
}

playapi::login_api PlayDevice::loadLoginInfo(playapi::device_info& device, device_config& devState,
                                             app_config const& appConfig) {
    playapi::login_api login(device);
    login.set_token(appConfig.user_email, appConfig.user_token);
    login.set_checkin_data(devState.checkin_data);
    login.verify();

    if (devState.checkin_data.android_id == 0) {
        playapi::checkin_api checkin(device);
        checkin.add_auth(login);
        devState.checkin_data = checkin.perform_checkin();
        devState.save();
    }

    return login;
}

void PlayDevice::checkTos() {
    if (api.toc_cookie.length() == 0 || api.device_config_token.length() == 0) {
        api.fetch_user_settings();
        auto toc = api.fetch_toc();
        if (toc.payload().tocresponse().has_cookie())
            api.toc_cookie = toc.payload().tocresponse().cookie();

        if (api.fetch_toc().payload().tocresponse().requiresuploaddeviceconfig()) {
            auto resp = api.upload_device_config();
            api.device_config_token = resp.payload().uploaddeviceconfigresponse().uploaddeviceconfigtoken();

            toc = api.fetch_toc();
            assert(!toc.payload().tocresponse().requiresuploaddeviceconfig() &&
                   toc.payload().tocresponse().has_cookie());
            api.toc_cookie = toc.payload().tocresponse().cookie();
            if (toc.payload().tocresponse().has_toscontent() && toc.payload().tocresponse().has_tostoken()) {
                auto tos = api.accept_tos(toc.payload().tocresponse().tostoken(), false);
                assert(tos.payload().has_accepttosresponse());
                deviceConfig.set_api_data(login.get_email(), api);
                deviceConfig.save();
            }
        }
    }
}

void PlayDevice::downloadApk(std::string const& packageName) {
    auto details = api.details(packageName).payload().detailsresponse().docv2();
}