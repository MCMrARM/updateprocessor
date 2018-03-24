#pragma once

#include <string>
#include <playapi/util/http.h>
#include <nlohmann/json.hpp>

namespace discord {

class Api {

private:
    std::string url;

public:
    Api(std::string const& url = "https://discordapp.com/api/v6/") : url(url) {}

    nlohmann::json
    sendRequest(playapi::http_method method, std::string const& path);

    std::string getGatewayUrl();

};

};