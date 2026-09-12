#ifndef AMD3DC_QUANTUM_DIE_HPP
#define AMD3DC_QUANTUM_DIE_HPP

#include <cstdint>

class QuantumTensorDie
{
public:
    QuantumTensorDie(
        std::uint32_t tensorCores = 4,
        std::uint32_t quantumCores = 2,
        std::uint32_t pqcEngines = 2
    );

    void reset() noexcept;

    bool enabled() const noexcept;
    void setEnabled(bool value) noexcept;

    std::uint32_t tensorCores() const noexcept;
    std::uint32_t quantumCores() const noexcept;
    std::uint32_t pqcEngines() const noexcept;

    std::uint64_t tensorOperations() const noexcept;
    std::uint64_t quantumOperations() const noexcept;
    std::uint64_t pqcOperations() const noexcept;

    void accountTensorOperation() noexcept;
    void accountQuantumOperation() noexcept;
    void accountPqcOperation() noexcept;

private:
    bool enabled_ = true;

    std::uint32_t tensorCores_;
    std::uint32_t quantumCores_;
    std::uint32_t pqcEngines_;

    std::uint64_t tensorOperations_ = 0;
    std::uint64_t quantumOperations_ = 0;
    std::uint64_t pqcOperations_ = 0;
};

#endif