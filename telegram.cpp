#include "telegram.h"

using namespace playapi;
using namespace nlohmann;

nlohmann::json telegram::Api::sendRequest(playapi::http_method method, std::string const& path,
                                         std::string const& body) {
    playapi::http_request req (url + path);
    req.set_method(method);
    req.set_body(body);
    req.add_header("Content-Type", "application/json");
    auto resp = req.perform();
    if (!resp)
        throw std::runtime_error("Failed to perform http request");
    return json::parse(resp.get_body());
}

void telegram::Api::sendMessage(std::string const& chatId, std::string const& text, const std::string& parseMode) {
    json data;
    data["chat_id"] = chatId;
    data["text"] = text;
    if (!parseMode.empty())
        data["parse_mode"] = parseMode;
    sendRequest(playapi::http_method::POST, "bot" + token + "/sendMessage", data);
}