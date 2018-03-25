#include "apk_manager.h"
#include "file_utils.h"

ApkManager::ApkManager(PlayManager& playManager) : playManager(playManager) {
    thread = std::thread(std::bind(&ApkManager::runVersionCheckThread, this));
}

void ApkManager::runVersionCheckThread() {
    std::unique_lock<std::mutex> lk(thread_mutex);
    while (!stopped) {
        updateLatestVersions();

        auto until = std::chrono::system_clock::now() + std::chrono::minutes(10);
        stop_cv.wait_until(lk, until, [this] { return stopped; });
    }
}

void ApkManager::updateLatestVersions() {
    std::unique_lock<std::mutex> lk(data_mutex);
    playapi::api::bulk_details_request r;
    r.name = "com.mojang.minecraftpe";

    r.installed_version_code = armVersionInfo.lastDownloadedVersionCode;
    auto detailsARM = playManager.getDeviceARM().getApi().bulk_details({r});
    auto appDetailsARM = detailsARM.payload().bulkdetailsresponse().entry(0).doc().details().appdetails();

    r.installed_version_code = x86VersionInfo.lastDownloadedVersionCode;
    auto detailsX86 = playManager.getDeviceARM().getApi().bulk_details({r});
    auto appDetailsX86 = detailsX86.payload().bulkdetailsresponse().entry(0).doc().details().appdetails();

    bool hasNewARMVersion = (armVersionInfo.versionCode != appDetailsARM.versioncode());
    bool hasNewX86Version = (x86VersionInfo.versionCode != appDetailsX86.versioncode());

    armVersionInfo.versionCode = appDetailsARM.versioncode();
    x86VersionInfo.versionCode = appDetailsX86.versioncode();

    if (hasNewARMVersion) {
        auto fullDetailsARM = playManager.getDeviceARM().getApi().details(r.name);
        auto fullAppDetailsARM = fullDetailsARM.payload().detailsresponse().docv2().details().appdetails();
        armVersionString = fullAppDetailsARM.versionstring();
    }

    lastVersionUpdate = std::chrono::system_clock::now();
}

void ApkManager::downloadAndProcessApks() {
    updateLatestVersions();
    downloadAndProcessApk(playManager.getDeviceARM(), getARMVersionInfo().versionCode);
    // downloadAndProcessApk(playManager.getDeviceX86(), x86VersionInfo.versionCode);
}

void ApkManager::downloadAndProcessApk(PlayDevice& device, int version) {
    std::string outp = "priv/apks/com.mojang.minecraftpe " + std::to_string(version) + ".apk";
    FileUtils::mkdirs(FileUtils::getParent(outp));
    device.downloadApk("com.mojang.minecraftpe", version, outp);
    sendApkForAnalytics(outp);
}

void ApkManager::sendApkForAnalytics(std::string const& path) {
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to init curl");
    curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field;
    field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, path.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:7543/ida/exec");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    auto res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        throw std::runtime_error(std::string("Upload request failed: ") + curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    curl_mime_free(form);
}