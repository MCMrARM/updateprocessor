#include "play_device.h"

#include <fstream>
#include <iostream>
#include <zlib.h>

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
    login.load_auth_cookies(devState.config.get_array("login_auth_cookies"));
    // login.verify();

    if (devState.checkin_data.android_id == 0) {
        playapi::checkin_api checkin(device);
        checkin.add_auth(login);
        devState.checkin_data = checkin.perform_checkin();
        storeAuthCookies(devState, login);
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
                // deviceConfig.save();
            }
        }
    }
    storeAuthCookies(deviceConfig, login);
    deviceConfig.save();
}

void PlayDevice::storeAuthCookies(device_config& device, playapi::login_api& login) {
    device.config.set_array("login_auth_cookies", login.store_auth_cookies());
}

void PlayDevice::registerMCS() {
    playapi::mcs_registration_api::instance_id_request req;
    req.sender = "932144863878";
    req.app_id = "cefO6sEQcZE";
    req.app_pkg_name = "com.android.vending";
    req.app_ver = "80921100";
    req.app_ver_name = "9.2.11-all+[0]+[PR]+188192317";
    req.app_cert = "38918a453d07199354f8b19af05ec6562ced5788";
    req.client_id = "iid-11910000";
    req.scope = "GCM";
    std::string token = apiMcs.obtain_instance_id_token(req);
    api.upload_device_config(token, false);
}

void PlayDevice::doContentSync() {
    playapi::proto::finsky::contentsync::ContentSyncRequestProto req;
    req.set_unknownalwaystrue(true);
    auto app = req.add_installed();
    app->set_id("com.mojang.minecraftpe");
    app->set_version(871020009);
    req.set_hassystemapps(false);
    api.content_sync(req);
}

static void do_zlib_inflate(z_stream& zs, FILE* file, char* data, size_t len, int flags) {
    char buf[4096];
    int ret;
    zs.avail_in = (uInt) len;
    zs.next_in = (unsigned char*) data;
    zs.avail_out = 0;
    while (zs.avail_out == 0) {
        zs.avail_out = 4096;
        zs.next_out = (unsigned char*) buf;
        ret = inflate(&zs, flags);
        assert(ret != Z_STREAM_ERROR);
        fwrite(buf, 1, sizeof(buf) - zs.avail_out, file);
    }
}

void PlayDevice::downloadApk(std::string const& packageName, int packageVersion, std::string const& downloadTo) {
    auto resp = api.delivery(packageName, packageVersion, std::string());
    auto dd = resp.payload().deliveryresponse().appdeliverydata();
    playapi::http_request req(dd.has_gzippeddownloadurl() ? dd.gzippeddownloadurl() : dd.downloadurl());
    if (dd.has_gzippeddownloadurl())
        req.set_encoding("gzip,deflate");
    req.set_encoding("gzip,deflate");
    req.add_header("Accept-Encoding", "identity");
    auto cookie = dd.downloadauthcookie(0);
    req.add_header("Cookie", cookie.name() + "=" + cookie.value());
    req.set_user_agent("AndroidDownloadManager/" + device.build_version_string + " (Linux; U; Android " +
                       device.build_version_string + "; " + device.build_model + " Build/" + device.build_id + ")");
    req.set_follow_location(true);
    req.set_timeout(0L);

    FILE* file = fopen(downloadTo.c_str(), "w");
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    int ret = inflateInit2(&zs, 31);
    assert(ret == Z_OK);

    if (dd.has_gzippeddownloadurl()) {
        req.set_custom_output_func([file, &zs](char* data, size_t size) {
            do_zlib_inflate(zs, file, data, size, Z_NO_FLUSH);
            return size;
        });
    } else {
        req.set_custom_output_func([file, &zs](char* data, size_t size) {
            fwrite(data, sizeof(char), size, file);
            return size;
        });
    }

    req.set_progress_callback([&req](curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        if (dltotal > 0) {
            printf("\rDownloaded %i%% [%li/%li MiB]", (int) (dlnow * 100 / dltotal), dlnow / 1024 / 1024,
                   dltotal / 1024 / 1024);
            std::cout.flush();
        }
    });
    std::cout << std::endl << "Starting download...";
    req.perform();

    do_zlib_inflate(zs, file, Z_NULL, 0, Z_FINISH);
    inflateEnd(&zs);

    fclose(file);
}