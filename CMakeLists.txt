cmake_minimum_required(VERSION 3.20)

project(AMD_3DC
    VERSION 0.2.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(AMD_3DC
    src/main.cpp

    src/amd3dc.c
    src/memory.c
    src/isa.c
    src/igpu.c
    src/gui.c
    src/bios.c
    src/bios_ui.cpp

    src/bios_rom.cpp
    src/bios_gui.cpp
    src/quantum_die.cpp
    src/bootloader.cpp
    src/boot_config.cpp
    src/x86_translator.cpp
    src/x86_compat.cpp
)

target_sources(AMD_3DC PRIVATE
    include/amd3dc.h
    include/amd3dc_types.h
    include/bios.h
    include/bios_gui.hpp
    include/bios_rom.hpp
    include/bios_ui.h
    include/boot_config.hpp
    include/bootloader.hpp
    include/x86_translator.hpp
    include/x86_compat.hpp
    include/gui.h
    include/igpu.h
    include/isa.h
    include/memory.h
    include/quantum_die.hpp
)

target_include_directories(AMD_3DC PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Define the path to firmware directory for BIOS loading
target_compile_definitions(AMD_3DC PRIVATE
    AMD3DC_FIRMWARE_DIR="${PROJECT_SOURCE_DIR}/firmware/"
)

target_link_libraries(AMD_3DC PRIVATE
    user32
    gdi32
)

# Set working directory for debugging
set_target_properties(AMD_3DC PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
