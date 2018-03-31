#include "telegram_state.h"

#include <fstream>
#include <sstream>
#include <regex>

TelegramState::TelegramState(ApkManager& apkManager) {
    std::ifstream ifs("priv/telegram.conf");
    config.load(ifs);

    api.setToken(config.get("token"));
    broadcastTo = config.get_array("broadcast", {});

    using namespace std::placeholders;
    apkManager.addNewVersionCallback(std::bind(&TelegramState::onNewVersion, this, _1, _2, _3, _4));
}

void TelegramState::onNewVersion(int version, std::string const& versionString, std::string const& changelog,
                                 std::string const& variant) {
    static std::regex regexNewlines ("<br>");
    std::string processedChangelog = std::regex_replace(changelog, regexNewlines, "\n");
    std::stringstream ss;
    ss << "*New version available: " << versionString
       << "* (version code: " << version << ", variant: " << variant << ")\n"
       << processedChangelog;
    broadcastMessage(ss.str(), "Markdown");
}

void TelegramState::broadcastMessage(std::string const& text, const std::string& parseMode) {
    for (std::string const& target : broadcastTo)
        api.sendMessage(target, text, parseMode);
}