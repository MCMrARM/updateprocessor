#pragma once

#include <string>
#include <playapi/util/http.h>
#include <nlohmann/json.hpp>

namespace discord {

using Snowflake = std::string;

struct Message {

    Snowflake id;
    Snowflake channel;
    std::string content;
    std::string author_id;

    static Message fromJson(nlohmann::json const& j) {
        Message ret;
        ret.id = j["id"];
        ret.channel = j["channel_id"];
        ret.content = j["content"];
        ret.author_id = j["author"]["id"];
        return ret;
    }

};



struct CreateMessageParams {
    std::string content;
    std::string nonce;
    bool tts = false;
    nlohmann::json embed;

    CreateMessageParams(std::string content) : content(std::move(content)) { }
    CreateMessageParams(const char* content) : content(content) { }

};

class Api {

private:
    std::string url;
    std::string authHeader;

public:

    Api(std::string const& url = "https://discordapp.com/api/v6/") : url(url) {}

    void setBothAuth(std::string const& token) {
        authHeader = "Bot " + token;
    }

    void setOauthAuth(std::string const& token) {
        authHeader = "Bearer " + token;
    }

    nlohmann::json
    sendRequest(playapi::http_method method, std::string const& path, std::string const& body = std::string());

    nlohmann::json
    sendRequest(playapi::http_method method, std::string const& path, nlohmann::json body) {
        return sendRequest(method, path, body.dump());
    }

    std::string getGatewayUrl();

    Message createMessage(Snowflake const& channel, CreateMessageParams const& message);

};

};