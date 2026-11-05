# Cellarium 3DC

*A Theoretical 3D Matrix Processor Architecture and High-Performance Emulator*

## Overview

**Cellarium 3DC** is an experimental processor architecture concept accompanied by a dedicated emulator and development toolchain.

The project explores volumetric computing models that extend beyond traditional two-dimensional microarchitectural layouts. Its architecture is based on organizing processing units, memory structures, cache layers, and interconnects within a three-dimensional matrix.

The repository provides public access to the project's source code, architectural documentation, instruction set architecture (ISA) definitions, HEX16 instruction encoding specifications, and hybrid C/C++ emulation environment.

Cellarium 3DC is an independent research and development project focused on processor architecture experimentation, instruction execution models, hardware emulation, and volumetric computing concepts.

## Key Features

- **Volumetric Interconnect Grid:** Experimental three-dimensional mesh topology for communication between processing units and architectural layers.
- **HEX16 Instruction Encoding:** Custom 16-bit hexadecimal instruction encoding and assembly format.
- **Hybrid C/C++ Engine:** Low-level execution components implemented in C, with modular runtime and emulation infrastructure in C++.
- **Instruction Simulation:** Execution models for matrix transformations, vector operations, and memory interactions.
- **Modular Architecture:** Designed to support experimentation with different processing configurations and architectural components.
- **Storage Emulation:** BIOS-compatible disk interfaces and ATA PIO emulation.
- **Boot Media Support:** Configurable boot images, hard disk images, and floppy disk images.

## Technical Stack

| Component | Technology |
|---|---|
| Primary languages | C, C++ |
| Language standards | C17, C++20 |
| Instruction encoding | HEX16 |
| Architecture | 3D Matrix Architecture |
| Primary operating system | Microsoft Windows |
| Linux compatibility | Wine / Proton |
| Build system | CMake / Make |

## Platform Compatibility

Cellarium 3DC is primarily developed and tested on Microsoft Windows.

Linux users can run compatible Windows builds through the following compatibility layers:

- **Wine:** Windows application compatibility on Linux.
- **Proton:** Compatibility environment for supported Windows applications on Linux.

Native Linux support is not currently provided.

## Prerequisites

A Windows-compatible C/C++ development environment is required.

Recommended tools:

- Microsoft Visual Studio or a compatible C/C++ toolchain.
- GCC or Clang where applicable.
- CMake.
- Make or Ninja.

Verify your development environment:

gcc --version
clang --version
cmake --version
