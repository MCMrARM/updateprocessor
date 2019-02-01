#include "apk_uploader.h"
#include "wake_on_lan.h"

#include <curl/curl.h>
#include <fstream>
#include <functional>
#include <sstream>

ApkUploader::ApkUploader() {
    std::ifstream ifs ("priv/apkupload.conf");
    conf.load(ifs);
    std::vector<std::string> files = conf.get_array("files", {});
    this->files = std::set<std::string>(std::make_move_iterator(files.begin()), std::make_move_iterator(files.end()));
    apiAddress = conf.get("url");
    apiToken = conf.get("token");
    WakeOnLan::parseMacAddress(conf.get("wol_mac"), wakeOnLanAddr);

    thread = std::thread(std::bind(&ApkUploader::handleWork, this));
}

ApkUploader::~ApkUploader() {
    mutex.lock();
    stopped = true;
    condvar.notify_all();
    mutex.unlock();
    thread.join();
}

void ApkUploader::handleWork() {
    bool fromWakeOnLan = false;

    std::unique_lock<std::mutex> lk(mutex);
    while (!stopped) {
        auto until = std::chrono::system_clock::now() + std::chrono::minutes(1);

        if (!files.empty()) {
            bool online = checkIsOnline();
            if (!online) {
                try {
                    WakeOnLan::sendWakeOnLan(wakeOnLanAddr);
                } catch (std::exception& e) {
                    printf("Error waking PC: %s\n", e.what());
                }
                fromWakeOnLan = true;

                until = std::chrono::system_clock::now() + std::chrono::seconds(10);
            } else {
                bool deletedAny = false;
                for (auto it = files.begin(); it != files.end(); ) {
                    printf("Upload File: %s\n", it->c_str());
                    try {
                        uploadFile(*it, fromWakeOnLan);
                    } catch (std::exception& e) {
                        printf("Error uploading file %s\n", e.what());
                        break;
                    }
                    remove(it->c_str());
                    it = files.erase(it);
                    deletedAny = true;
                    fromWakeOnLan = false;
                }
                if (deletedAny)
                    saveFileList();
            }
        }

        condvar.wait_until(lk, until);
    }
}

void ApkUploader::saveFileList() {
    std::ofstream ofs ("priv/apkupload.conf");
    std::vector<std::string> v (files.begin(), files.end());
    conf.set_array("files", v);
    conf.save(ofs);
}

void ApkUploader::addFile(std::string const& path) {
    std::unique_lock<std::mutex> lk(mutex);
    files.insert(path);
    saveFileList();
    lk.unlock();
    condvar.notify_all();
}

bool ApkUploader::checkIsOnline() {
    CURL* curl = curl_easy_init();
    if (!curl)
        return false;
    curl_easy_setopt(curl, CURLOPT_URL, (apiAddress + "isOnline").c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 200);
    std::stringstream ss;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)
            [](char* ptr, size_t size, size_t nmemb, void* userdata) {
                ((std::stringstream*) userdata)->write(ptr, size * nmemb);
                return size * nmemb;
            });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ss);
    auto res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        return false;
    curl_easy_cleanup(curl);
    return ss.str() == "ok";
}

void ApkUploader::uploadFile(std::string const& path, bool wol) {
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to init curl");
    curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field;
    field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, path.c_str());
    field = curl_mime_addpart(form);
    curl_mime_name(field, "token");
    curl_mime_data(field, apiToken.c_str(), apiToken.length());
    if (wol) {
        field = curl_mime_addpart(form);
        curl_mime_name(field, "wol");
        curl_mime_data(field, "1", CURL_ZERO_TERMINATED);
    }
    curl_easy_setopt(curl, CURLOPT_URL, (apiAddress + "exec").c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    auto res = curl_easy_perform(curl);
    long res_code = 0L;
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
    curl_easy_cleanup(curl);
    curl_mime_free(form);
    if (res != CURLE_OK)
        throw std::runtime_error(std::string("Upload request failed: ") + curl_easy_strerror(res));
    if (res_code != 200)
        throw std::runtime_error(std::string("Upload request failed with status code ") + std::to_string(res_code));

}