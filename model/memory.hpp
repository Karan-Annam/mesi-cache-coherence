// Backing main memory for the model.
#pragma once
#include <cstdint>
#include <unordered_map>
#include "mesi_types.hpp"

namespace mesi {

class Memory {
public:
    uint32_t read(uint32_t addr) {
        auto it = data_.find(addr);
        return it == data_.end() ? 0u : it->second;
    }
    void write(uint32_t addr, uint32_t value) { data_[addr] = value; }

    uint64_t accesses = 0; // bookkeeping: memory transactions served

private:
    std::unordered_map<uint32_t, uint32_t> data_;
};

} // namespace mesi
