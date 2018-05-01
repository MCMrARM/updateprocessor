#include "discord_gateway.h"

using namespace playapi;
using namespace nlohmann;
using namespace discord::gateway;

static const char* ZLIB_SUFFIX = "\x00\x00\xff\xff";

const int Connection::INITIAL_RECONNECT_DELAY;
const int Connection::MAX_RECONNECT_DELAY;

std::string Connection::decompress(const char* data, size_t length) {
    zs.avail_in = (uInt) length;
    zs.next_in = (unsigned char*) data;
    std::string out;
    while(true) {
        out.resize(out.size() + 4096);
        zs.avail_out = 4096;
        zs.next_out = (unsigned char*) (out.data() + out.size() - 4096);
        int ret = inflate(&zs, Z_SYNC_FLUSH);
        assert(ret != Z_STREAM_ERROR);
        if (zs.avail_out != 0) {
            out.resize(out.size() - zs.avail_out);
            break;
        }
    }
    return out;
}

Connection::Connection() {
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    int ret = inflateInit(&zs);
    assert(ret == Z_OK);

    hub.onConnection([this](uWS::WebSocket<uWS::CLIENT>* ws, uWS::HttpRequest req) {
        printf("Connected!\n");
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        this->ws = ws;
        inflateReset(&zs);
        reconnectNumber = 0;
        nextReconnectDelay = INITIAL_RECONNECT_DELAY;
        hasReceivedACK = true;
    });
    hub.onError([this](void*) {
        printf("Unknown error\n");
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        handleDisconnect();
    });
    hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
        printf("Disconnected! %i %s\n", code, std::string(message, length).c_str());
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        this->ws = nullptr;
        handleDisconnect();
    });
    hub.onMessage([this](uWS::WebSocket<uWS::CLIENT>* ws, char* message, size_t length, uWS::OpCode opCode) {
        std::string outp;
        json j;
        if (isCompressed) {
            compressedBuffer.append(message, length);
            if (length > 4 && memcmp(&message[length - 4], ZLIB_SUFFIX, 4) == 0) {
                outp = decompress(compressedBuffer.data(), compressedBuffer.length());
                compressedBuffer.clear();
                printf("Got compressed message: %s\n", outp.c_str());
                j = json::parse(outp);
            }
        } else {
            printf("Got message: %.*s\n", (int) length, message);
            j = json::parse(detail::input_adapter(message, length));
        }
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

Connection::~Connection() {
    inflateEnd(&zs);
}

void Connection::connect(std::string const& uri) {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    printf("Connecting: %s\n", uri.c_str());
    if (ws != nullptr || reconnectNumber != -1)
        throw std::runtime_error("Already connected");
    this->uri = uri;
    reconnectNumber = 0;
    nextReconnectDelay = INITIAL_RECONNECT_DELAY;
    hub.connect(uri, nullptr);

    if (reconnectHandle == nullptr) {
        reconnectHandle = new uS::Async(hub.getLoop());
        reconnectHandle->start([](uS::Async*) {});
    }
}

void Connection::handleDisconnect() {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    long long delay = nextReconnectDelay;
    reconnectNumber++;
    nextReconnectDelay = std::min(nextReconnectDelay * 2, MAX_RECONNECT_DELAY);

    if (pingTimer != nullptr) {
        pingTimer->stop();
        pingTimer->close();
        pingTimer = nullptr;
    }
    if (reconnectTimer == nullptr)
        reconnectTimer = new uS::Timer(hub.getLoop());
    else
        reconnectTimer->stop();
    reconnectTimer->setData(this);
    reconnectTimer->start([](uS::Timer* timer) {
        Connection* conn = ((Connection*) timer->getData());
        std::unique_lock<std::recursive_mutex> lock(conn->dataMutex);
        conn->hub.connect(conn->uri, nullptr);
    }, delay, 0);
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
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    if (ws == nullptr)
        throw std::runtime_error("No connection available");
    printf("Send message: %s\n", data.c_str());
    ws->send(data.c_str(), data.length(), uWS::OpCode::TEXT);
}

void Connection::sendHeartbeat() {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    hasReceivedACK = false;
    Payload ping;
    ping.op = Payload::Op::Heartbeat;
    if (lastSeqReceived != -1)
        ping.data = lastSeqReceived;
    else
        ping.data = nullptr;
    lock.unlock();
    sendPayload(ping);
}

void Connection::sendIdentifyRequest() {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    Payload reply;
    reply.op = Payload::Op::Identify;
    reply.data["token"] = token;
    reply.data["properties"]["$os"] = "linux";
    reply.data["properties"]["$browser"] = "updateprocessor";
    reply.data["properties"]["device"] = "updateprocessor";
    reply.data["compress"] = true;
    reply.data["large_threshold"] = 50;
    reply.data["presence"] = status.toJson();
    lock.unlock();
    sendPayload(reply);
}

void Connection::handlePayload(Payload const& payload) {
    if (payload.op == Payload::Op::Dispatch)
        handleDispatchPayload(payload);
    else if (payload.op == Payload::Op::Heartbeat)
        handleHeartbeat(payload);
    else if (payload.op == Payload::Op::Hello)
        handleHelloPayload(payload);
    else if (payload.op == Payload::Op::HeartbeatACK)
        handleHeartbeatACK(payload);
    else if (payload.op == Payload::Op::InvalidSession)
        handleInvalidSessionPayload(payload);
}

void Connection::handleHelloPayload(Payload const& payload) {
    startHeartbeat(payload.data["heartbeat_interval"]);

    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    if (!sessionId.empty()) {
        Payload reply;
        reply.op = Payload::Op::Resume;
        reply.data["token"] = token;
        reply.data["session_id"] = sessionId;
        reply.data["seq"] = lastSeqReceived;
        lock.unlock();
        sendPayload(reply);
        return;
    }
    lock.unlock();
    sendIdentifyRequest();
}

void Connection::handleInvalidSessionPayload(Payload const& payload) {
    sendIdentifyRequest();
}

void Connection::handleDispatchPayload(Payload const& payload) {
    {
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        lastSeqReceived = payload.sequenceNumber;
    }

    if (payload.eventName == "READY") {
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        sessionId = payload.data["session_id"];
    } else if (payload.eventName == "MESSAGE_CREATE") {
        Message m = Message::fromJson(payload.data);
        std::unique_lock<std::recursive_mutex> lock(dataMutex);
        if (messageCallback)
            messageCallback(m);
    }
}

void Connection::handleHeartbeat(Payload const& payload) {
    sendHeartbeat();
}

void Connection::handleHeartbeatACK(Payload const& payload) {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    hasReceivedACK = true;
}

bool Connection::checkReceivedHeartbeatACK() {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    if (!hasReceivedACK) {
        lock.unlock();
        ws->close(1001);
        return false;
    }
    return true;
}

void Connection::startHeartbeat(int interval) {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    if (pingTimer != nullptr)
        return;
    pingTimer = new uS::Timer(hub.getLoop());
    pingTimer->setData(this);
    pingTimer->start([](uS::Timer* timer) {
        Connection* conn = ((Connection*) timer->getData());
        if (conn->checkReceivedHeartbeatACK())
            conn->sendHeartbeat();
    }, interval, interval);
}

void Connection::setStatus(StatusInfo const& status) {
    std::unique_lock<std::recursive_mutex> lock(dataMutex);
    this->status = status;
    if (ws != nullptr) {
        Payload reply;
        reply.op = Payload::Op::StatusUpdate;
        reply.data = status.toJson();
        lock.unlock();
        sendPayload(reply);
    } else {
        lock.unlock();
    }
}