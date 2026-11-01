#include <cstddef>
#include <chrono>
#include <cstdio>
#include <string>
#include <windows.h>

extern "C"
{
#include "amd3dc.h"
#include "amd3dc_types.h"
#include "bios.h"
#include "gui.h"
#include "igpu.h"
#include "isa.h"
#include "memory.h"
#include "bios_ui.h"
}

#include "bios_rom.hpp"
#include "boot_config.hpp"
#include "bootloader.hpp"

#define AMD3DC_TEST_CORES    2u
#define AMD3DC_TEST_SEGMENTS 4u
#define AMD3DC_TEST_MEMORY   (2u * 1024u * 1024u)

static void print_segments(
    const AMD3DC_State* cpu
)
{
    if (cpu == NULL || cpu->segments == NULL)
        return;

    printf("\nSegments:\n");

    for (size_t i = 0; i < cpu->segment_count; ++i)
    {
        const AMD3DC_Segment* segment =
            &cpu->segments[i];

        printf(
            "  Segment %u: %s",
            segment->id,
            segment->enabled ? "enabled" : "disabled"
        );

        if (segment->faulty)
            printf(" [FAULT]");

        printf("\n");
    }
}

static bool test_instruction_fetch(
    AMD3DC_State* cpu
)
{
    const uint64_t test_instruction =
        0x123456789ABCDEF0ULL;

    AMD3DC_Core* core;
    uint64_t fetched;

    if (cpu == NULL ||
        cpu->cores == NULL ||
        cpu->core_count == 0)
    {
        return false;
    }

    core = &cpu->cores[0];

    printf("\nInstruction Fetch Test\n");
    printf("----------------------\n");

    if (!amd3dc_memory_write64(
        &cpu->memory,
        0,
        test_instruction))
    {
        printf("Memory write failed.\n");
        return false;
    }

    core->pc = 0;

    printf(
        "Core 0 PC before fetch: 0x%016llX\n",
        (unsigned long long)core->pc
    );

    fetched = amd3dc_fetch64(cpu);

    printf(
        "Fetched instruction:    0x%016llX\n",
        (unsigned long long)fetched
    );

    printf(
        "Core 0 PC after fetch:  0x%016llX\n",
        (unsigned long long)core->pc
    );

    if (fetched != test_instruction ||
        core->pc != 8)
    {
        printf("Instruction fetch: FAIL\n");
        return false;
    }

    printf("Instruction fetch: PASS\n");
    return true;
}

static bool test_lae(
    AMD3DC_State* cpu
)
{
    AMD3DC_Core* core;

    AMD3DC_Instruction set_instruction =
    {
        AMD3DC_OP_LAE_SET,
        0,
        1,
        0,
        0
    };

    AMD3DC_Instruction get_instruction =
    {
        AMD3DC_OP_LAE_GET,
        2,
        0,
        0,
        0
    };

    if (cpu == NULL ||
        cpu->cores == NULL ||
        cpu->core_count == 0)
    {
        return false;
    }

    core = &cpu->cores[0];

    printf("\nLAE Test\n");
    printf("--------\n");

    core->registers[1] = 7;

    if (!amd3dc_execute64(
        cpu,
        &set_instruction))
    {
        printf("LAE.SET failed.\n");
        return false;
    }

    printf(
        "Layer after LAE.SET: %llu\n",
        (unsigned long long)core->layer
    );

    if (!amd3dc_execute64(
        cpu,
        &get_instruction))
    {
        printf("LAE.GET failed.\n");
        return false;
    }

    printf(
        "R2 after LAE.GET: %llu\n",
        (unsigned long long)core->registers[2]
    );

    if (core->registers[2] != 7)
    {
        printf("LAE test: FAIL\n");
        return false;
    }

    printf("LAE test: PASS\n");
    return true;
}

static bool test_extended_isa(
    AMD3DC_State* cpu
)
{
    AMD3DC_Core* core;

    AMD3DC_Instruction vzero =
    {
        AMD3DC_OP_CVX_VZERO,
        0,
        0,
        0,
        0
    };

    AMD3DC_Instruction vadd =
    {
        AMD3DC_OP_CVX_VADD,
        2,
        0,
        1,
        0
    };

    AMD3DC_Instruction vdot =
    {
        AMD3DC_OP_CVX_VDOT,
        3,
        2,
        1,
        0
    };

    AMD3DC_Instruction zshift =
    {
        AMD3DC_OP_ZSHIFT,
        0,
        0,
        0,
        3
    };

    AMD3DC_Instruction qmatmul =
    {
        AMD3DC_OP_QMATMUL_P8,
        4,
        2,
        1,
        0
    };

    AMD3DC_Instruction qstate =
    {
        AMD3DC_OP_QSTATE_EMUL,
        5,
        0,
        0,
        3
    };

    AMD3DC_Instruction pqc =
    {
        AMD3DC_OP_PQC_KYBER_ENC,
        6,
        0,
        0,
        0x1234
    };

    AMD3DC_Instruction anonym =
    {
        AMD3DC_OP_MASK_ANONYM,
        7,
        6,
        0,
        0
    };

    if (cpu == NULL ||
        cpu->cores == NULL ||
        cpu->core_count == 0)
    {
        return false;
    }

    core = &cpu->cores[0];

    printf("\nExtended ISA Test\n");
    printf("-----------------\n");

    core->vector_registers[0].x = 1.0f;
    core->vector_registers[0].y = 2.0f;
    core->vector_registers[0].z = 3.0f;
    core->vector_registers[0].w = 4.0f;

    core->vector_registers[1].x = 5.0f;
    core->vector_registers[1].y = 6.0f;
    core->vector_registers[1].z = 7.0f;
    core->vector_registers[1].w = 8.0f;

    if (!amd3dc_execute64(cpu, &vzero))
    {
        printf("CVX.VZERO failed.\n");
        return false;
    }

    if (!amd3dc_execute64(cpu, &vadd))
    {
        printf("CVX.VADD failed.\n");
        return false;
    }

    if (!amd3dc_execute64(cpu, &vdot))
    {
        printf("CVX.VDOT failed.\n");
        return false;
    }

    printf(
        "CVX VDOT result: %llu\n",
        (unsigned long long)core->registers[3]
    );

    if (!amd3dc_execute64(cpu, &zshift))
    {
        printf("CSX.ZSHIFT failed.\n");
        return false;
    }

    printf(
        "CSX layer after ZSHIFT: %llu\n",
        (unsigned long long)core->layer
    );

    if (!amd3dc_execute64(cpu, &qmatmul))
    {
        printf("QCX.QMATMUL.P8 failed.\n");
        return false;
    }

    if (!amd3dc_execute64(cpu, &qstate))
    {
        printf("QCX.QSTATE.EMUL failed.\n");
        return false;
    }

    printf(
        "QCX QMATMUL result: %llu\n",
        (unsigned long long)core->registers[4]
    );

    printf(
        "QCX QSTATE branches: %llu\n",
        (unsigned long long)core->registers[5]
    );

    if (!amd3dc_execute64(cpu, &pqc))
    {
        printf("PQCX.KYBER.ENC failed.\n");
        return false;
    }

    printf(
        "PQCX dispatch result: 0x%016llX\n",
        (unsigned long long)core->registers[6]
    );

    core->registers[6] =
        0xFFFF123456789ABCULL;

    if (!amd3dc_execute64(cpu, &anonym))
    {
        printf("BDX.MASK.ANONYM failed.\n");
        return false;
    }

    printf(
        "BDX anonymized value: 0x%016llX\n",
        (unsigned long long)core->registers[7]
    );

    printf("Extended ISA: PASS\n");
    return true;
}

static bool test_igpu(
    AMD3DC_State* cpu
)
{
    const uint32_t test_color =
        0x00FF0000u;

    uint32_t pixel;

    if (cpu == NULL)
        return false;

    printf("\nVirtio GPU\n");
    printf("----------\n");

    printf(
        "Resolution: %ux%u\n",
        cpu->virtio.width,
        cpu->virtio.height
    );

    printf(
        "Queue Data: %zu bytes\n",
        cpu->virtio.queue_data_size
    );

    if (!amd3dc_virtio_put_pixel(
        &cpu->virtio,
        10,
        10,
        test_color))
    {
        printf("Virtio GPU pixel write failed.\n");
        return false;
    }

    pixel =
        amd3dc_virtio_get_pixel(
            &cpu->virtio,
            10,
            10
        );

    printf(
        "Pixel[10,10]: 0x%08X\n",
        pixel
    );

    if (pixel != test_color)
    {
        printf("Virtio GPU test: FAIL\n");
        return false;
    }

    printf("Virtio GPU test: PASS\n");
    return true;
}

int main(int argc, char** argv)
{
    const bool headless = argc > 1 && std::string(argv[1]) == "--headless";
    AMD3DC_State cpu{};

    printf("AMD 3DC Emulator Starting...\n");
    fflush(stdout);

    if (!amd3dc_init(
        &cpu,
        AMD3DC_TEST_CORES,
        AMD3DC_TEST_SEGMENTS,
        AMD3DC_TEST_MEMORY))
    {
        std::fprintf(
            stderr,
            "AMD3DC initialization failed.\n"
        );

        return 1;
    }

    amd3dc_bios_init(&cpu);

    BiosRom bios;

    std::string bios_path =
        std::string(AMD3DC_FIRMWARE_DIR) + "bios.bin";

    if (!bios.load(bios_path))
    {
        std::fprintf(
            stderr,
            "BIOS ROM load failed at: %s\n",
            bios_path.c_str()
        );

        amd3dc_destroy(&cpu);
        return 1;
    }

    std::printf(
        "AMD 3DC BIOS ROM loaded\n"
        "Version: %u\n"
        "Entry:   0x%016llX\n"
        "Size:    %llu bytes\n\n",
        bios.version(),
        static_cast<unsigned long long>(
            bios.entryPoint()),
        static_cast<unsigned long long>(
            bios.imageSize())
    );

    if (!bios.boot(cpu, 128))
    {
        std::fprintf(
            stderr,
            "BIOS ROM execution failed.\n"
        );

        amd3dc_destroy(&cpu);
        return 1;
    }

    if (!amd3dc_bios_post(&cpu))
    {
        std::fprintf(
            stderr,
            "BIOS POST failed (continuing to setup).\n"
        );
    }
    else
    {
        amd3dc_bios_print(&cpu);
    }

    /*
     * Create the actual VM display FIRST.
     *
     * This creates:
     *     AMD 3DC Virtual Machine
     *
     * and connects it to cpu.virtio.queue_data.
     */
    if (!headless && !amd3dc_gui_init(&cpu))
    {
        std::fprintf(
            stderr,
            "Virtual display initialization failed.\n"
        );

        amd3dc_destroy(&cpu);
        return 1;
    }

    if (!headless)
        printf("Virtual display initialized: %ux%u\n", cpu.virtio.width, cpu.virtio.height);

    /*
     * Initialize BIOS Setup UI.
     *
     * BIOS UI writes directly into the VirtIO framebuffer.
     */
    BiosUI bios_ui{};

    const bool bios_ui_initialized = headless ||
        bios_ui_init(
            &bios_ui,
            &cpu
        );

    if (!bios_ui_initialized)
    {
        std::fprintf(
            stderr,
            "BIOS UI initialization failed.\n"
        );

        amd3dc_gui_shutdown();
        amd3dc_destroy(&cpu);
        return 1;
    }

    /*
     * Main VM loop.
     *
     * IMPORTANT:
     *
     * bios_ui_render()
     *      -> writes framebuffer
     *
     * amd3dc_gui_render()
     *      -> pumps Win32 messages
     *      -> WM_PAINT reads framebuffer
     *      -> window gets updated
     */
    while (!headless && bios_ui.running)
    {
        bios_ui_render(&bios_ui);

        bios_ui_handle_input(&bios_ui);

        if (!amd3dc_gui_render())
        {
            bios_ui.running = false;
            break;
        }

        Sleep(16);
    }

    if (!headless) bios_ui_destroy(&bios_ui);

    printf(
        "BIOS UI exited.\n"
    );

    /*
     * Keep the VM display alive after BIOS Setup exits.
     *
     * This allows the host window to remain usable instead
     * of immediately destroying the Win32 window.
     */

    BootConfig bootConfig;

    if (!bootConfig.load(std::string(AMD3DC_FIRMWARE_DIR) + "../BOOT.LD") &&
        !bootConfig.load("BOOT.LD"))
    {
        std::fprintf(
            stderr,
            "BOOT.LD load failed.\n"
        );

        if (!headless) amd3dc_gui_shutdown();
        amd3dc_destroy(&cpu);

        return 1;
    }

    std::printf(
        "BOOT.LD target: %s\n",
        bootConfig.image().c_str()
    );

    const std::string boot_image_path =
        std::string(AMD3DC_FIRMWARE_DIR) +
        bootConfig.image();

    std::printf(
        "Boot image path: %s\n",
        boot_image_path.c_str()
    );

    Bootloader bootloader;

    if (!bootloader.loadImage(cpu, boot_image_path))
    {
        std::fprintf(
            stderr,
            "Boot image load failed: %s\n",
            bootConfig.image().c_str()
        );

        if (!headless) amd3dc_gui_shutdown();
        amd3dc_destroy(&cpu);

        return 1;
    }

    const std::string& hdd_path = bootConfig.hardDisk();

    if (!hdd_path.empty())
    {
        if (!bootloader.loadHardDisk(hdd_path))
        {
            std::fprintf(stderr, "Hard disk image load failed: %s\n", hdd_path.c_str());
            if (!headless) amd3dc_gui_shutdown();
            amd3dc_destroy(&cpu);
            return 1;
        }
    }

    if (!bootConfig.secondFloppy().empty())
    {
        const std::string floppy_path = std::string(AMD3DC_FIRMWARE_DIR) + bootConfig.secondFloppy();
        if (!bootloader.loadSecondFloppy(floppy_path))
        {
            std::fprintf(stderr, "Second floppy image load failed: %s\\n", bootConfig.secondFloppy().c_str());
            if (!headless) amd3dc_gui_shutdown(); amd3dc_destroy(&cpu); return 1;
        }
    }

    std::printf(
        "Boot image loaded\n"
        "Load address: 0x%016llX\n"
        "Entry point : 0x%016llX\n"
        "Image size  : %llu bytes\n",
        static_cast<unsigned long long>(
            bootloader.loadAddress()),
        static_cast<unsigned long long>(
            bootloader.entryPoint()),
        static_cast<unsigned long long>(
            bootloader.imageSize())
    );

    if (!bootloader.start(cpu))
    {
        std::fprintf(
            stderr,
            "Boot image start failed.\n"
        );

        if (!headless) amd3dc_gui_shutdown();
        amd3dc_destroy(&cpu);

        return 1;
    }

    std::printf(
        "Control transferred to boot image.\n"
    );

    int result = 0;
    if (bootloader.isRealModeImage())
    {
        bool reportedRealModeError = false;
        std::uint64_t debugFrames = 0;
        auto lastDebugOutput = std::chrono::steady_clock::now();
        while (headless || amd3dc_gui_render())
        {
            if (!headless) bootloader.pollRealModeInput();
            // Amortize the host-side loop and virtual dispatch over larger
            // batches. The window message pump still runs once per batch.
            const std::size_t executed = bootloader.runRealModeBatch(2000000);
            if (executed < 2000000)
            {
                if (!bootloader.realModeError().empty() && !reportedRealModeError)
                {
                    std::fprintf(stderr, "x86 real-mode stop: %s\n",
                        bootloader.realModeError().c_str());
                    reportedRealModeError = true;
                }
                if (!bootloader.realModeError().empty())
                    break;
            }
            if (!headless) bootloader.renderRealModeDisplay();

            // Full register and disk traces are expensive and overwhelm the
            // console during DOS I/O. Keep a useful heartbeat, but emit it
            // only twice per second.
            ++debugFrames;
            const auto now = std::chrono::steady_clock::now();
            if (!headless && now - lastDebugOutput >= std::chrono::seconds(2))
            {
                const std::string guestState = bootloader.realModeDebugState();
                std::fprintf(stderr, "Real-mode CPU: %s\n", guestState.c_str());
                if (cpu.memory.data != nullptr && cpu.memory.size >= 0x100000)
                {
                    unsigned cs = 0, ip = 0;
                    std::sscanf(guestState.c_str(), "ES:CS:SS:DS=%*x:%x:%*x:%*x IP=%x",
                        &cs, &ip);
                    std::fprintf(stderr, "Guest instruction bytes at %04X:%04X:", cs, ip);
                    for (unsigned i = 0; i < 16; ++i)
                    {
                        const std::size_t address = ((static_cast<std::size_t>(cs) << 4) +
                            static_cast<std::uint16_t>(ip + i)) & 0xFFFFF;
                        std::fprintf(stderr, " %02X", cpu.memory.data[address]);
                    }
                    std::fprintf(stderr, "\n");
                }
                lastDebugOutput = now;
            }
            if (headless && now - lastDebugOutput >= std::chrono::seconds(2))
            {
                const std::string guestState = bootloader.realModeDebugState();
                std::fprintf(stderr, "Real-mode CPU: %s\n", guestState.c_str());
                unsigned cs = 0, ip = 0;
                if (std::sscanf(guestState.c_str(),
                    "ES:CS:SS:DS=%*x:%x:%*x:%*x IP=%x", &cs, &ip) == 2 &&
                    cpu.memory.data != nullptr)
                {
                    std::fprintf(stderr, "Guest bytes at %04X:%04X:", cs, ip);
                    for (unsigned i = 0; i < 16; ++i)
                    {
                        const std::size_t address = ((static_cast<std::size_t>(cs) << 4) +
                            static_cast<std::uint16_t>(ip + i)) & 0xFFFFF;
                        std::fprintf(stderr, " %02X", cpu.memory.data[address]);
                    }
                    std::fprintf(stderr, "\n");
                }
                if (cpu.memory.data != nullptr && cpu.memory.size >= 0xB8000 + 4000)
                {
                    for (unsigned row = 0; row < 25; ++row)
                    {
                        char line[81];
                        for (unsigned col = 0; col < 80; ++col)
                            line[col] = static_cast<char>(cpu.memory.data[
                                0xB8000 + (row * 80 + col) * 2]);
                        line[80] = '\0';
                        for (int col = 79; col >= 0 && line[col] == ' '; --col)
                            line[col] = '\0';
                        if (line[0] != '\0') std::fprintf(stderr, "VGA[%02u]: %s\n", row, line);
                    }
                }
                lastDebugOutput = now;
            }

            // Yield occasionally to keep the host responsive without paying
            // a scheduler handoff on every emulation quantum.
            if (!headless && (debugFrames & 7u) == 0)
                Sleep(0);
            if (headless && !bootloader.realModeError().empty()) {
                std::fprintf(stderr, "x86 real-mode stop: %s\n", bootloader.realModeError().c_str());
                break;
            }
        }
    }
    else
    {
        result = amd3dc_gui_run(&cpu);
    }

    if (!headless) amd3dc_gui_shutdown();

    amd3dc_destroy(&cpu);

    return result;
}
