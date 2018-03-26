#pragma once

#include <string>

class WakeOnLan {

private:

    static int getHexDigit(char c);

public:

    static void sendWakeOnLan(const char mac[6]);

    static void parseMacAddress(std::string const& str, char binary[6]);


};