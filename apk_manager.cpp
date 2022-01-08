#include "apk_manager.h"
#include "file_utils.h"
#include <fstream>
#include <iostream>

const char* ApkManager::PKG_NAME = "com.mojang.minecraftpe";

ApkManager::ApkManager(PlayManager& playManager, JobManager& jobManager) :
        playManager(playManager), jobManager(jobManager) {
    std::ifstream ifs ("priv/versioninfo.conf");
    versionCheckConfig.load(ifs);
    releaseARMVersionInfo.loadFromConfig(versionCheckConfig, "release.arm.");
    releaseX86VersionInfo.loadFromConfig(versionCheckConfig, "release.x86.");
    releaseARM64VersionInfo.loadFromConfig(versionCheckConfig, "release.arm64.");
    releaseX8664VersionInfo.loadFromConfig(versionCheckConfig, "release.x86_64.");
    betaARMVersionInfo.loadFromConfig(versionCheckConfig, "beta.arm.");
    betaX86VersionInfo.loadFromConfig(versionCheckConfig, "beta.x86.");
    betaARM64VersionInfo.loadFromConfig(versionCheckConfig, "beta.arm64.");
    betaX8664VersionInfo.loadFromConfig(versionCheckConfig, "beta.x86_64.");
}

void ApkManager::requestForceCheck() {
    stop_cv.notify_all();
}

void ApkManager::saveVersionInfo() {
    releaseARMVersionInfo.saveToConfig(versionCheckConfig, "release.arm.");
    releaseX86VersionInfo.saveToConfig(versionCheckConfig, "release.x86.");
    releaseARM64VersionInfo.saveToConfig(versionCheckConfig, "release.arm64.");
    releaseX8664VersionInfo.saveToConfig(versionCheckConfig, "release.x86_64.");
    betaARMVersionInfo.saveToConfig(versionCheckConfig, "beta.arm.");
    betaX86VersionInfo.saveToConfig(versionCheckConfig, "beta.x86.");
    betaARM64VersionInfo.saveToConfig(versionCheckConfig, "beta.arm64.");
    betaX8664VersionInfo.saveToConfig(versionCheckConfig, "beta.x86_64.");
    std::ofstream ofs("priv/versioninfo.conf");
    versionCheckConfig.save(ofs);
}

void ApkVersionInfo::loadFromConfig(playapi::config const& config, std::string const& prefix) {
    versionCode = config.get_int(prefix + "version_code", versionCode);
    lastDownloadedVersionCode = config.get_int(prefix + "last_downloaded_version_code", lastDownloadedVersionCode);
    versionString = config.get(prefix + "version_string", versionString);
}

void ApkVersionInfo::saveToConfig(playapi::config& config, std::string const& prefix) {
    config.set_int(prefix + "version_code", versionCode);
    config.set_int(prefix + "last_downloaded_version_code", lastDownloadedVersionCode);
    config.set(prefix + "version_string", versionString);
}

void ApkManager::startChecking() {
    thread = std::thread(std::bind(&ApkManager::runVersionCheckThread, this));
}

void ApkManager::runVersionCheckThread() {
    std::unique_lock<std::mutex> lk(thread_mutex);
    while (!stopped) {
        updateLatestVersions();

        auto until = std::chrono::system_clock::now() + std::chrono::minutes(10);
        stop_cv.wait_until(lk, until);
    }
}


void ApkManager::updateLatestVersions() {
    std::unique_lock<std::mutex> lk(data_mutex);

    struct VariantInfo {
        std::string variantName;
        PlayDevice& device;
        ApkVersionInfo& versionInfo;
        std::string versionString;
        CheckResult result;
    };
    VariantInfo variants[8] = {
            {"release/arm", playManager.getReleaseDeviceARM(), releaseARMVersionInfo},
            {"release/arm64", playManager.getReleaseDeviceARM64(), releaseARM64VersionInfo},
            {"release/x86", playManager.getReleaseDeviceX86(), releaseX86VersionInfo},
            {"release/x86_64", playManager.getReleaseDeviceX8664(), releaseX8664VersionInfo},
            {"beta/arm", playManager.getBetaDeviceARM(), betaARMVersionInfo},
            {"beta/arm64", playManager.getBetaDeviceARM64(), betaARM64VersionInfo},
            {"beta/x86", playManager.getBetaDeviceX86(), betaX86VersionInfo},
            {"beta/x86_64", playManager.getBetaDeviceX8664(), betaX8664VersionInfo}
    };

    bool hasAnyUpdate = false;
    for (auto& v : variants) {
        v.result = updateLatestVersion(v.device, v.versionInfo);
        v.versionString = v.versionInfo.versionString; // copy it because we need to access it w/o a mutex later
        hasAnyUpdate = (hasAnyUpdate || v.result.hasNewVersion);
    }

    lastVersionUpdate = std::chrono::system_clock::now();

    if (hasAnyUpdate)
        saveVersionInfo();

    lk.unlock();
    if (hasAnyUpdate) {
        std::unique_lock<std::mutex> lk_cb(cb_mutex);
        for (auto const& v : variants) {
            if (!v.result.hasNewVersion)
                continue;
            for (auto const& cb : newVersionCallback) {
                try {
                    cb(v.result.versionCode, v.versionString, v.result.changelog, v.variantName);
                } catch (std::exception& e) {
                    std::cerr << "Error processing callback " << e.what() << "\n";
                }
            }
        }
    }

    for (auto const& v : variants) {
        if (v.result.shouldDownload)
            downloadAndProcessApk(v.device, v.result.versionCode, v.versionInfo);
    }
}

ApkManager::CheckResult ApkManager::updateLatestVersion(PlayDevice& device, ApkVersionInfo& versionInfo) {
    CheckResult ret;
    playapi::api::bulk_details_request r;
    r.name = PKG_NAME;
    r.installed_version_code = versionInfo.lastDownloadedVersionCode;
    playapi::proto::finsky::response::ResponseWrapper details;
    try {
        details = device.getApi().bulk_details({r});
        if (details.payload().bulkdetailsresponse().entry_size() != 1)
            throw std::runtime_error("Invalid response: entry_size() != 1");
        if (!details.payload().bulkdetailsresponse().entry(0).has_doc() ||
                !details.payload().bulkdetailsresponse().entry(0).doc().has_details() ||
                !details.payload().bulkdetailsresponse().entry(0).doc().details().has_appdetails())
            throw std::runtime_error("Invalid response: does not have details");
    } catch (std::exception& e) {
        std::cout << "Error getting details: " << e.what() << "\n";
        ret.hasNewVersion = false;
        ret.shouldDownload = false;
        return ret;
    }
    auto appDetails = details.payload().bulkdetailsresponse().entry(0).doc().details().appdetails();
    ret.hasNewVersion = (versionInfo.versionCode != appDetails.versioncode());
    ret.shouldDownload = (versionInfo.lastDownloadedVersionCode != appDetails.versioncode());
    ret.versionCode = appDetails.versioncode();
    ret.changelog = appDetails.recentchangeshtml();
    versionInfo.versionCode = appDetails.versioncode();
    versionInfo.lastSuccess = std::chrono::system_clock::now();

    if (ret.hasNewVersion) {
        auto fullDetails = device.getApi().details(PKG_NAME);
        auto fullAppDetails = fullDetails.payload().detailsresponse().docv2().details().appdetails();
        if (fullAppDetails.versioncode() != ret.versionCode)
            ret.hasNewVersion = false; // we don't have the version string yet; check it again later
        versionInfo.versionString = fullAppDetails.versionstring();
    }
    return ret;
}

void ApkManager::downloadAndProcessApk(PlayDevice& device, int version, ApkVersionInfo& info) {
    downloadAndProcessApk(device, version);
    std::unique_lock<std::mutex> lk(data_mutex);
    if (version > info.lastDownloadedVersionCode) {
        info.lastDownloadedVersionCode = version;
        saveVersionInfo();
    }
}

void ApkManager::downloadAndProcessApk(PlayDevice& device, int version, bool onlyNatives) {
    auto links = device.getDownloadLinks("com.mojang.minecraftpe", version);
    if (onlyNatives) {
        std::vector<PlayDevice::DownloadLink> linksCopy;
        std::copy_if(links.begin(), links.end(), std::back_inserter(linksCopy), [](PlayDevice::DownloadLink const &l) {
            return l.name == "main" || l.name == "config.x86" || l.name == "config.x86_64" || l.name == "config.armeabi_v7a" || l.name == "config.arm64_v8a";
        });
    }
    if (links.empty())
        return;

    auto job = jobManager.createJob();
    ApkJobDescription apkJob;
    apkJob.versionCode = version;
    for (auto const &l : links) {
        device.downloadApk(l, job.dataDir + "/" + l.name + ".apk");
        apkJob.apks.emplace_back(l.name, l.name + ".apk");
    }
    jobManager.addApkJob(job, apkJob);
}
