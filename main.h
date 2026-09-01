#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#define DEBUG_INFO __func__, __FILE__, __LINE__

constexpr uint64_t DOUBLE_SIGN = 0x8000000000000000;
constexpr uint64_t DOUBLE_EXPONENT = 0x7FF0000000000000;
constexpr uint64_t DOUBLE_FRACTION = 0x000FFFFFFFFFFFFF;
constexpr uint64_t DOUBLE_SPECIAL = DOUBLE_EXPONENT;
constexpr uint64_t DOUBLE_NEGATIVE_ZERO = DOUBLE_SIGN;

// std::string input();
void assert(
    const bool& assertion,
    const std::string& message,
    const std::string& filePath,
    const std::string& function,
    const std::string& source,
    const uint32_t& line
);
std::string byte_to_string(const uint8_t& byte);
#include "hash/hash_resolver.h"

class Bytecode;
class Ast;
class Lua;

#include "bytecode/bytecode.h"
#include "ast/ast.h"
#include "lua/lua.h"
