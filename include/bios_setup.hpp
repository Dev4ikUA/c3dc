#ifndef AMD3DC_BIOS_SETUP_HPP
#define AMD3DC_BIOS_SETUP_HPP

#include <cstdint>

extern "C"
{
#include "amd3dc_types.h"
}

class BiosSetup
{
public:
    explicit BiosSetup(
        AMD3DC_State& state
    );

    void run();

private:
    AMD3DC_State& state_;

    bool running_ = true;
    int page_ = 0;
    int selected_ = 0;

    bool epicEnabled_ = true;
    std::uint32_t epicWidth_ = 8;

    bool tensorEnabled_ = true;
    bool quantumEnabled_ = true;
    bool pqcxEnabled_ = true;

    void render();
    void handleKey(int key);
    void save();
};

#endif