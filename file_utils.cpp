#include "file_utils.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

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

void FileUtils::deleteDir(std::string const &path, int fd) {
    int dfd = openat(fd, path.c_str(), O_RDONLY|O_DIRECTORY);
    DIR *d = fdopendir(dfd);
    dirent *ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] != '.') {
            if (ent->d_type == DT_DIR) {
                deleteDir(path, dfd);
            } else {
                unlinkat(dfd, ent->d_name, 0);
            }
        }
    }
    closedir(d);
    close(dfd);
    unlinkat(fd, path.c_str(), AT_REMOVEDIR);
}