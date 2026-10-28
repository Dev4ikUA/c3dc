#ifndef AMD3DC_BOOT_CONFIG_HPP
#define AMD3DC_BOOT_CONFIG_HPP

#include <string>

class BootConfig
{
public:
    bool load(
        const std::string& filename
    );

    const std::string& image() const noexcept;

private:
    std::string image_;
};

#endif