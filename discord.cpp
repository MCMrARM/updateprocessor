#include "discord.h"

using namespace playapi;
using namespace nlohmann;

nlohmann::json discord::Api::sendRequest(playapi::http_method method, std::string const& path,
                                         std::string const& body) {
    playapi::http_request req (url + path);
    req.set_method(method);
    req.set_body(body);
    if (!authHeader.empty())
        req.add_header("Authorization", authHeader);
    auto resp = req.perform();
    if (!resp)
        throw std::runtime_error("Failed to perform http request");
    return json::parse(resp.get_body());
}

std::string discord::Api::getGatewayUrl() {
    json j = sendRequest(http_method::GET, "gateway");
    return j["url"];
}

discord::Message discord::Api::createMessage(Snowflake const& channel, CreateMessageParams const& message) {
    json j;
    j["content"] = message.content;
    if (!message.nonce.empty())
        j["nonce"] = message.nonce;
    j["tts"] = message.tts;
    if (!message.embed.empty())
        j["embed"] = message.embed;
    auto res = sendRequest(http_method::POST, "channels/" + channel + "/messages", j);
    return Message::fromJson(res);
}