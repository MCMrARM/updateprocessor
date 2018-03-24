#include "discord.h"

using namespace playapi;
using namespace nlohmann;

nlohmann::json discord::Api::sendRequest(playapi::http_method method, std::string const& path) {
    playapi::http_request req (url + path);
    req.set_method(method);
    auto resp = req.perform();
    if (!resp)
        throw std::runtime_error("Failed to perform http request");
    return json::parse(resp.get_body());
}

std::string discord::Api::getGatewayUrl() {
    json j = sendRequest(http_method::GET, "gateway");
    return j["url"];
}
