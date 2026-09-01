#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

class HashResolver final {
  public:
    explicit HashResolver(const std::filesystem::path& packageIndexDirectory, int requestedBitLength);
    [[nodiscard]] uint64_t mask(uint64_t hash) const noexcept { return hash & hashMask; }

    [[nodiscard]] const std::string* resolve(uint64_t hash) const noexcept;

  private:
    uint64_t hashMask = ~uint64_t{0};
    std::unordered_map<uint64_t, std::string> entries;
};
