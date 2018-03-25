#pragma once

#include <string>

class FileUtils {

public:

    static std::string getParent(std::string const& path);

    static void mkdirs(std::string const& path, unsigned int chmod = 0700);

};