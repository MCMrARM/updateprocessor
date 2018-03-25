#include "apk_uploader.h"

#include <curl/curl.h>
#include <fstream>
#include <functional>

ApkUploader::ApkUploader() {
    std::ifstream ifs ("priv/apkupload.conf");
    conf.load(ifs);
    std::vector<std::string> files = conf.get_array("files", {});
    this->files = std::set<std::string>(std::make_move_iterator(files.begin()), std::make_move_iterator(files.end()));
    apiAddress = conf.get("url");

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
    std::unique_lock<std::mutex> lk(mutex);
    while (!stopped) {
        for (auto const& file : files) {
            printf("Upload File: %s\n", file.c_str());
            //uploadFile(file);
        }

        auto until = std::chrono::system_clock::now() + std::chrono::minutes(1);
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

void ApkUploader::uploadFile(std::string const& path) {
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to init curl");
    curl_mime* form = curl_mime_init(curl);
    curl_mimepart* field;
    field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, path.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, (apiAddress + "exec").c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    auto res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        throw std::runtime_error(std::string("Upload request failed: ") + curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    curl_mime_free(form);
}