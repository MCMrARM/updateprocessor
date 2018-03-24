#include "discord_gateway.h"

using namespace playapi;
using namespace nlohmann;
using namespace discord::gateway;

static const char* ZLIB_SUFFIX = "\x00\x00\xff\xff";

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
        std::unique_lock<std::mutex> lock(dataMutex);
        this->ws = ws;
    });
    hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
        printf("Disconnected!\n");
        std::unique_lock<std::mutex> lock(dataMutex);
        this->ws = nullptr;
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