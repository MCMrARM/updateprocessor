#include "apk_manager.h"
#include "file_utils.h"

void ApkManager::updateLatestVersions() {
    auto detailsARM = playManager.getDeviceARM().getApi().details("com.mojang.minecraftpe");
    auto appDetailsARM = detailsARM.payload().detailsresponse().docv2().details().appdetails();

    auto detailsX86 = playManager.getDeviceX86().getApi().details("com.mojang.minecraftpe");
    auto appDetailsX86 = detailsX86.payload().detailsresponse().docv2().details().appdetails();

    armVersionInfo.versionCode = appDetailsARM.versioncode();
    armVersionInfo.versionString = appDetailsARM.versionstring();

    x86VersionInfo.versionCode = appDetailsX86.versioncode();
    x86VersionInfo.versionString = appDetailsX86.versionstring();
}

void ApkManager::downloadAndProcessApks() {
    maybeUpdateLatestVersions();
    downloadAndProcessApk(playManager.getDeviceARM(), armVersionInfo.versionCode, armVersionInfo.versionString, "arm");
    // downloadAndProcessApk(playManager.getDeviceX86(), x86VersionInfo.versionCode, x86VersionInfo.versionString, "x86");
}

void ApkManager::downloadAndProcessApk(PlayDevice& device, int version, std::string const& versionString,
                                       std::string const& archStr) {
    std::string outp = getOutputFilePath(version, versionString, archStr);
    FileUtils::mkdirs(FileUtils::getParent(outp));
    device.downloadApk("com.mojang.minecraftpe", version, outp);
}

std::string ApkManager::getOutputFilePath(int version, std::string const& versionString, std::string const& archStr) {
    std::stringstream outputDir;
    outputDir << "priv/apks/";
    auto iof = versionString.find('.');
    iof = versionString.find('.', iof + 1);
    if (iof == std::string::npos)
        throw std::runtime_error("Invalid version string");
    outputDir << versionString.substr(0, iof) << ".x/";
    auto iof2 = versionString.find('.', iof + 1);
    if (iof2 == std::string::npos)
        throw std::runtime_error("Invalid version string");
    outputDir << versionString.substr(0, iof2) << ".x/";

    outputDir << "Minecraft " << versionString << " " << archStr << " " << version << ".apk";
    return outputDir.str();
}