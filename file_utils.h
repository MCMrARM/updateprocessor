#pragma once

#include <string>
#include <fcntl.h>

class FileUtils {

public:

    static std::string getParent(std::string const& path);

    static void mkdirs(std::string const& path, unsigned int chmod = 0700);

    static void deleteDir(std::string const& path, int fd = AT_FDCWD);

};