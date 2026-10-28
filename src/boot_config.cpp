#include "boot_config.hpp"

#include <cctype>
#include <fstream>
#include <string>

bool BootConfig::load(
    const std::string& filename
)
{
    std::ifstream file(filename);

    if (!file)
        return false;

    std::string line;

    while (std::getline(file, line))
    {
        while (!line.empty() &&
            std::isspace(static_cast<unsigned char>(line.back())))
            line.pop_back();

        const auto first = line.find_first_not_of(" \t");
        if (first != std::string::npos && first != 0)
            line.erase(0, first);

        if (line.empty())
            continue;

        if (line[0] == '#' ||
            line[0] == ';')
        {
            continue;
        }

        constexpr const char* prefix = "IMAGE=";

        if (line.rfind(prefix, 0) == 0)
        {
            image_ = line.substr(6);

            return !image_.empty();
        }
    }

    return false;
}

const std::string& BootConfig::image() const noexcept
{
    return image_;
}
