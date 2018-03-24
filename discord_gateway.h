#pragma once

#include "discord.h"
#include <uWS.h>

namespace discord {

namespace gateway {

struct Payload {
    enum class Op {
        Dispatch = 0,
        Heartbeat,
        Identify,
        StatusUpdate,
        VoiceStateUpdate,
        VoiceServerPing,
        Resume,
        Reconnect,
        RequestGuildMembers,
        InvalidSession,
        Hello,
        HeartbeatACK
    };

    Op op;
    nlohmann::json data;
    int sequenceNumber;
    std::string eventName;
};

struct Activity {

    enum Type { Game, Streaming, Listening };

    std::string name;
    Type type;
    std::string url;

    nlohmann::json toJson() const {
        nlohmann::json ret;
        ret["name"] = name;
        ret["type"] = type;
        if (!url.empty())
            ret["url"] = url;
        return ret;
    }


};

struct StatusInfo {

    std::chrono::system_clock::time_point since;
    Activity activity;
    std::string status;
    bool afk = false;

    nlohmann::json toJson() const {
        nlohmann::json ret;
        ret["since"] = since.time_since_epoch().count();
        ret["game"] = activity.toJson();
        ret["status"] = status;
        ret["afk"] = afk;
        return ret;
    }

};


class Connection {

public:
    using MessageCallback = std::function<void (Message const&)>;

private:
    uWS::Hub hub;
    uWS::WebSocket<uWS::CLIENT>* ws = nullptr;
    uS::Timer* pingTimer = nullptr;
    z_stream zs;

    std::mutex dataMutex;
    std::string token;
    std::string sessionId;
    StatusInfo status;
    int lastSeqReceived = -1;
    std::string compressedBuffer;
    bool isCompressed = true;
    bool hasReceivedACK = true;
    MessageCallback messageCallback;

    std::string decompress(const char* data, size_t length);

    void sendPayload(Payload const& payload);

    void sendHeartbeat();

    void sendIdentifyRequest();

    void handlePayload(Payload const& payload);

    void handleHelloPayload(Payload const& payload);

    void handleInvalidSessionPayload(Payload const& payload);

    void handleDispatchPayload(Payload const& payload);

    void handleHeartbeat(Payload const& payload);

    void handleHeartbeatACK(Payload const& payload);

    void startHeartbeat(int interval);


    void checkReceivedHeartbeatACK();

public:
    Connection();

    ~Connection();

    void connect(std::string const& url);

    void connect(Api& api) {
        connect(api.getGatewayUrl() + "/?v=6&encoding=json&compress=zlib-stream");
    }

    void setToken(std::string const& token) {
        std::unique_lock<std::mutex> lock(dataMutex);
        this->token = token;
    }

    void setSessionId(std::string const& session, int seq) {
        std::unique_lock<std::mutex> lock(dataMutex);
        sessionId = session;
        lastSeqReceived = seq;
    }

    inline std::string const& getSession() {
        std::unique_lock<std::mutex> lock(dataMutex);
        return sessionId;
    }

    inline int getSessionSeq() {
        std::unique_lock<std::mutex> lock(dataMutex);
        return lastSeqReceived;
    }

    void setStatus(StatusInfo const& status);

    void setMessageCallback(MessageCallback const& callback) {
        std::unique_lock<std::mutex> lock(dataMutex);
        messageCallback = callback;
    }

    void loop() {
        hub.run();
    }

};

};

};