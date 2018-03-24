#include "discord_gateway.h"

using namespace playapi;
using namespace nlohmann;
using namespace discord::gateway;

Connection::Connection() {
    hub.onConnection([this](uWS::WebSocket<uWS::CLIENT>* ws, uWS::HttpRequest req) {
        printf("Connected!\n");
        std::unique_lock<std::mutex> lock(dataMutex);
        this->ws = ws;
    });
    hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
        printf("Disconnected!\n");
        std::unique_lock<std::mutex> lock(dataMutex);
        this->ws = nullptr;
    });
    hub.onMessage([this](uWS::WebSocket<uWS::CLIENT>* ws, char* message, size_t length, uWS::OpCode opCode) {
        printf("Got message: %.*s\n", (int) length, message);
        json j = json::parse(detail::input_adapter(message, length));
        Payload payload;
        payload.op = j["op"];
        payload.data = j["d"];
        if (j.count("s") > 0 && j["s"] != nullptr)
            payload.sequenceNumber = j["s"];
        if (j.count("t") > 0 && j["t"] != nullptr)
            payload.eventName = j["t"];
        handlePayload(payload);
    });
}

void Connection::connect(std::string const& url) {
    printf("Connecting: %s\n", url.c_str());
    hub.connect(url, nullptr);
}

void Connection::sendPayload(Payload const& payload) {
    nlohmann::json pk;
    pk["op"] = payload.op;
    pk["d"] = payload.data;
    if (payload.op == Payload::Op::Dispatch) {
        pk["s"] = payload.sequenceNumber;
        pk["t"] = payload.eventName;
    }
    std::string data = pk.dump();
    std::unique_lock<std::mutex> lock(dataMutex);
    if (ws == nullptr)
        throw std::runtime_error("No connection available");
    printf("Send message: %s\n", data.c_str());
    ws->send(data.c_str(), data.length(), uWS::OpCode::TEXT);
}

void Connection::handlePayload(Payload const& payload) {
    if (payload.op == Payload::Op::Hello)
        handleHelloPayload(payload);
}

void Connection::handleHelloPayload(Payload const& payload) {
    std::unique_lock<std::mutex> lock(dataMutex);
    Payload reply;
    reply.op = Payload::Op::Identify;
    reply.data["token"] = token;
    reply.data["properties"]["$os"] = "linux";
    reply.data["properties"]["$browser"] = "updateprocesser";
    reply.data["properties"]["device"] = "updateprocesser";
    reply.data["compress"] = true;
    reply.data["large_threshold"] = 50;
    reply.data["presence"] = status.toJson();
    lock.unlock();
    sendPayload(reply);
}