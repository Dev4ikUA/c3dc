#include "bios_setup.hpp"

#include <conio.h>
#include <cstdio>

BiosSetup::BiosSetup(
    AMD3DC_State& state
)
    : state_(state)
{
}

void BiosSetup::render()
{
    std::printf(
        "\033[2J\033[H"
    );

    std::printf(
        "=====================================================================\n"
        "                    AMD 3DC BIOS SETUP UTILITY\n"
        "                       Cellarium 3DC v0.1\n"
        "=====================================================================\n\n"
    );

    const char* pages[] =
    {
        "Main",
        "CPU",
        "Memory",
        "3D",
        "Quantum",
        "Power",
        "Boot",
        "Exit"
    };

    for (int i = 0; i < 8; ++i)
    {
        std::printf(
            "%c %-8s",
            i == page_ ? '[' : ' ',
            pages[i]
        );

        if (i == page_)
            std::printf("]");

        std::printf(" ");
    }

    std::printf("\n\n");

    switch (page_)
    {
    case 0:
        std::printf(
            "CPU          : AMD 3DC\n"
            "Architecture : 64-bit Big Endian\n"
            "Cores        : %zu\n"
            "Segments     : %zu\n"
            "HBM3         : 24 GiB\n"
            "iGPU         : %s\n",
            state_.core_count,
            state_.segment_count,
            state_.igpu.enabled
            ? "Enabled"
            : "Disabled"
        );
        break;

    case 1:
        std::printf(
            "%c EPIC Execution [%s]\n",
            selected_ == 0 ? '>' : ' ',
            epicEnabled_
            ? "Enabled"
            : "Disabled"
        );

        std::printf(
            "%c Execution Width [%u]\n",
            selected_ == 1 ? '>' : ' ',
            epicWidth_
        );
        break;

    case 2:
        std::printf(
            "%c L1 [%s]\n",
            selected_ == 0 ? '>' : ' ',
            state_.l1.enabled
            ? "Enabled"
            : "Disabled"
        );

        std::printf(
            "%c L2 [%s]\n",
            selected_ == 1 ? '>' : ' ',
            state_.l2.enabled
            ? "Enabled"
            : "Disabled"
        );

        std::printf(
            "%c L3 [%s]\n",
            selected_ == 2 ? '>' : ' ',
            state_.l3.enabled
            ? "Enabled"
            : "Disabled"
        );

        std::printf(
            "%c L4 [%s]\n",
            selected_ == 3 ? '>' : ' ',
            state_.l4.enabled
            ? "Enabled"
            : "Disabled"
        );
        break;

    case 4:
        std::printf(
            "%c Tensor Die [%s]\n",
            selected_ == 0 ? '>' : ' ',
            tensorEnabled_
            ? "Enabled"
            : "Disabled"
        );

        std::printf(
            "%c Quantum Die [%s]\n",
            selected_ == 1 ? '>' : ' ',
            quantumEnabled_
            ? "Enabled"
            : "Disabled"
        );

        std::printf(
            "%c PQCX [%s]\n",
            selected_ == 2 ? '>' : ' ',
            pqcxEnabled_
            ? "Enabled"
            : "Disabled"
        );
        break;

    case 7:
        std::printf(
            "%c Save & Exit\n",
            selected_ == 0 ? '>' : ' '
        );

        std::printf(
            "%c Exit Without Saving\n",
            selected_ == 1 ? '>' : ' '
        );
        break;

    default:
        std::printf(
            "Configuration page.\n"
        );
        break;
    }

    std::printf(
        "\n---------------------------------------------------------------------\n"
        "Arrow Keys: Navigate   Enter: Select   +/-: Change\n"
        "F10: Save & Exit       ESC: Exit\n"
    );
}

void BiosSetup::handleKey(
    int key
)
{
    if (key == 0 ||
        key == 224)
    {
        const int extended = _getch();

        if (extended == 72)
            --selected_;

        if (extended == 80)
            ++selected_;

        if (extended == 75)
        {
            --page_;

            if (page_ < 0)
                page_ = 7;

            selected_ = 0;
        }

        if (extended == 77)
        {
            ++page_;

            if (page_ > 7)
                page_ = 0;

            selected_ = 0;
        }

        if (selected_ < 0)
            selected_ = 0;

        return;
    }

    if (key == 13 ||
        key == '+' ||
        key == '=' ||
        key == '-')
    {
        const bool increase =
            key != '-';

        if (page_ == 1 &&
            selected_ == 0)
        {
            epicEnabled_ =
                !epicEnabled_;
        }
        else if (page_ == 1 &&
            selected_ == 1)
        {
            if (increase && epicWidth_ < 16)
                ++epicWidth_;

            if (!increase && epicWidth_ > 1)
                --epicWidth_;
        }
        else if (page_ == 2)
        {
            switch (selected_)
            {
            case 0:
                state_.l1.enabled =
                    !state_.l1.enabled;
                break;

            case 1:
                state_.l2.enabled =
                    !state_.l2.enabled;
                break;

            case 2:
                state_.l3.enabled =
                    !state_.l3.enabled;
                break;

            case 3:
                state_.l4.enabled =
                    !state_.l4.enabled;
                break;

            default:
                break;
            }
        }
        else if (page_ == 4)
        {
            switch (selected_)
            {
            case 0:
                tensorEnabled_ =
                    !tensorEnabled_;
                break;

            case 1:
                quantumEnabled_ =
                    !quantumEnabled_;
                break;

            case 2:
                pqcxEnabled_ =
                    !pqcxEnabled_;
                break;

            default:
                break;
            }
        }

        return;
    }

    if (key == 27)
    {
        running_ = false;
        return;
    }

    if (key == 0x3E)
    {
        save();
        running_ = false;
    }
}

void BiosSetup::save()
{
    /*
     * Values that already map directly into AMD3DC_State
     * are written here. More firmware configuration fields
     * can be connected as the architecture grows.
     */
    if (state_.cores != nullptr &&
        state_.core_count != 0)
    {
        state_.cores[0].enabled =
            true;
    }
}

void BiosSetup::run()
{
    running_ = true;

    while (running_)
    {
        render();

        const int key = _getch();

        handleKey(key);
    }
}