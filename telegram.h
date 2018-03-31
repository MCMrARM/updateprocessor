#pragma once

#include <string>
#include <playapi/util/http.h>
#include <nlohmann/json.hpp>

namespace telegram {

class Api {

private:
    std::string url;
    std::string token;

public:
    Api(std::string const& url = "https://api.telegram.org/") : url(url) {}

    void setToken(std::string const& token) {
        this->token = token;
    }

    nlohmann::json
    sendRequest(playapi::http_method method, std::string const& path, std::string const& body = std::string());

    nlohmann::json
    sendRequest(playapi::http_method method, std::string const& path, nlohmann::json body) {
        return sendRequest(method, path, body.dump());
    }

    void sendMessage(std::string const& chatId, std::string const& text, std::string const& parseMode = std::string());

};

};