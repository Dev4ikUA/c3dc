#include "quantum_die.hpp"

QuantumTensorDie::QuantumTensorDie(
    std::uint32_t tensorCores,
    std::uint32_t quantumCores,
    std::uint32_t pqcEngines
)
    : tensorCores_(tensorCores),
    quantumCores_(quantumCores),
    pqcEngines_(pqcEngines)
{
}

void QuantumTensorDie::reset() noexcept
{
    tensorOperations_ = 0;
    quantumOperations_ = 0;
    pqcOperations_ = 0;
}

bool QuantumTensorDie::enabled() const noexcept
{
    return enabled_;
}

void QuantumTensorDie::setEnabled(
    bool value
) noexcept
{
    enabled_ = value;
}

std::uint32_t QuantumTensorDie::tensorCores() const noexcept
{
    return tensorCores_;
}

std::uint32_t QuantumTensorDie::quantumCores() const noexcept
{
    return quantumCores_;
}

std::uint32_t QuantumTensorDie::pqcEngines() const noexcept
{
    return pqcEngines_;
}

std::uint64_t QuantumTensorDie::tensorOperations() const noexcept
{
    return tensorOperations_;
}

std::uint64_t QuantumTensorDie::quantumOperations() const noexcept
{
    return quantumOperations_;
}

std::uint64_t QuantumTensorDie::pqcOperations() const noexcept
{
    return pqcOperations_;
}

void QuantumTensorDie::accountTensorOperation() noexcept
{
    ++tensorOperations_;
}

void QuantumTensorDie::accountQuantumOperation() noexcept
{
    ++quantumOperations_;
}

void QuantumTensorDie::accountPqcOperation() noexcept
{
    ++pqcOperations_;
}