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
    const std::string& hardDisk() const noexcept;
    const std::string& secondFloppy() const noexcept;

private:
    std::string image_;
    std::string hard_disk_;
    std::string second_floppy_;
};

#endif
