#include "file_utils.h"
#include <sys/stat.h>

std::string FileUtils::getParent(std::string const& path) {
    auto iof = path.rfind('/');
    if (iof == std::string::npos)
        return std::string();
    return path.substr(0, iof);
}

void FileUtils::mkdirs(std::string const& path, unsigned int chmod) {
    for (auto iof = path.find('/'); iof != std::string::npos; iof = path.find('/', iof + 1))
        mkdir(path.substr(0, iof).c_str(), chmod);
    mkdir(path.c_str(), chmod);
}