#include "wake_on_lan.h"

#include <unistd.h>
#include <netinet/in.h>
#include <stdexcept>
#include <cstring>

void WakeOnLan::sendWakeOnLan(const char* mac) {
    // Based on https://github.com/msanders/wol.c/blob/master/wol.c
    // Copyright (c) 2009-2010 Michael Sanders          MIT License
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    const int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
        close(fd);
        throw std::runtime_error("Failed to set socket options");
    }
    char message[102];
    memset(message, 0xff, 6);
    for (int i = 6; i < sizeof(message); i += 6)
        memcpy(&message[i], mac, 6);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0xFFFFFFFF;
    addr.sin_port = htons(60000);
    ssize_t ret = sendto(fd, (char*) message, sizeof(message), 0, (struct sockaddr*) &addr, sizeof(addr));
    close(fd);
    if (ret < 0)
        throw std::runtime_error("sendto() failed");
}

void WakeOnLan::parseMacAddress(std::string const& str, char* binary) {
    int si = 0;
    for (int i = 0; i < 6; i++) {
        if (si + 2 > str.length())
            throw std::runtime_error("Not enough characters");
        binary[i] = (char) (getHexDigit(str[si]) * 16 + getHexDigit(str[si + 1]));

        si += 2;
        if (si < str.length() && str[si] == ':')
            si++;
    }
}

int WakeOnLan::getHexDigit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    throw std::runtime_error("Invalid character");
}