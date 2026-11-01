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

        constexpr const char* imagePrefix = "IMAGE=";
        constexpr const char* hardDiskPrefix = "HDD=";
        constexpr const char* floppy2Prefix = "FLOPPY2=";

        if (line.rfind(imagePrefix, 0) == 0)
        {
            image_ = line.substr(6);
        }
        else if (line.rfind(hardDiskPrefix, 0) == 0)
        {
            hard_disk_ = line.substr(4);
        }
        else if (line.rfind(floppy2Prefix, 0) == 0)
            second_floppy_ = line.substr(8);
    }

    return !image_.empty();
}

const std::string& BootConfig::secondFloppy() const noexcept { return second_floppy_; }

const std::string& BootConfig::image() const noexcept
{
    return image_;
}

const std::string& BootConfig::hardDisk() const noexcept
{
    return hard_disk_;
}
