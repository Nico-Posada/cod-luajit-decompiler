#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

class HashResolver final {
  public:
    explicit HashResolver(const std::filesystem::path& packageIndexDirectory);

    [[nodiscard]] const std::string* resolve(uint64_t hash) const noexcept;

  private:
    std::unordered_map<uint64_t, std::string> entries;
};
