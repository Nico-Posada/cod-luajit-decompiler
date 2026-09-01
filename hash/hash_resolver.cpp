#include "hash_resolver.h"

#include "lz4.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <fstream>
#include <limits>
#include <iterator>
#include <print>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
constexpr uint32_t WNI_MAGIC = 0x20494E57;
constexpr int16_t WNI_VERSION = 1;
constexpr std::size_t WNI_MIN_RECORD_SIZE = sizeof(uint64_t) + 1;

int read_bit_length(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("Failed to read bit length file: " + path.string());

    std::string text;
    try {
        text.assign(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to read bit length file: " + path.string());
    }
    if (stream.bad())
        throw std::runtime_error("Failed to read bit length file: " + path.string());

    const std::size_t begin = std::min(text.find_first_not_of(" \t\n\r\f\v"), text.size());
    const std::size_t end =
        begin == text.size() ? begin : text.find_last_not_of(" \t\n\r\f\v") + 1;

    int bitLength = 0;
    const auto [parsedEnd, error] = std::from_chars(text.data() + begin, text.data() + end, bitLength);
    if (begin == end || error != std::errc{} || parsedEnd != text.data() + end || bitLength < 0 || bitLength > 64)
        throw std::runtime_error(
            "Invalid bit length in " + path.string() + ": expected a decimal integer from 0 to 64"
        );
    return bitLength;
}

uint64_t read_little_endian(
    const std::vector<char>& data, std::size_t& offset, const std::size_t width, const char* field
) {
    if (width > data.size() - offset)
        throw std::runtime_error(std::string("truncated ") + field);

    uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<uint64_t>(static_cast<unsigned char>(data[offset + i])) << (i * 8);
    offset += width;
    return value;
}

std::vector<char> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        throw std::runtime_error("failed to open file");

    const std::streampos end = stream.tellg();
    if (end < 0)
        throw std::runtime_error("failed to determine file size");
    if (static_cast<uint64_t>(end) > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("file is too large");

    std::vector<char> data(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!data.empty() && !stream.read(data.data(), static_cast<std::streamsize>(data.size())))
        throw std::runtime_error("failed to read file");
    return data;
}

std::vector<std::pair<uint64_t, std::string>> parse_file(const std::filesystem::path& path) {
    const std::vector<char> file = read_file(path);
    std::size_t offset = 0;

    const uint32_t magic = static_cast<uint32_t>(read_little_endian(file, offset, sizeof(uint32_t), "magic"));
    if (magic != WNI_MAGIC)
        throw std::runtime_error("invalid magic");

    const int16_t version = std::bit_cast<int16_t>(
        static_cast<uint16_t>(read_little_endian(file, offset, sizeof(uint16_t), "version"))
    );
    if (version != WNI_VERSION)
        throw std::runtime_error("unsupported version");

    const int32_t entryCount = std::bit_cast<int32_t>(
        static_cast<uint32_t>(read_little_endian(file, offset, sizeof(uint32_t), "entry count"))
    );
    const int32_t compressedSize = std::bit_cast<int32_t>(
        static_cast<uint32_t>(read_little_endian(file, offset, sizeof(uint32_t), "compressed size"))
    );
    const int32_t decompressedSize = std::bit_cast<int32_t>(
        static_cast<uint32_t>(read_little_endian(file, offset, sizeof(uint32_t), "decompressed size"))
    );

    if (entryCount < 0 || compressedSize < 0 || decompressedSize < 0)
        throw std::runtime_error("negative header field");
    if (compressedSize > std::numeric_limits<int>::max() || decompressedSize > std::numeric_limits<int>::max())
        throw std::runtime_error("buffer size exceeds LZ4 limits");

    const std::size_t count = static_cast<std::size_t>(entryCount);
    const std::size_t outputSize = static_cast<std::size_t>(decompressedSize);
    if (count > outputSize / WNI_MIN_RECORD_SIZE)
        throw std::runtime_error("entry count exceeds decompressed size");
    if (file.size() - offset != static_cast<std::size_t>(compressedSize))
        throw std::runtime_error("compressed payload size mismatch");

    std::vector<char> decompressed(outputSize);
    char emptyOutput = 0;
    const int decodedSize = LZ4_decompress_safe(
        file.data() + offset,
        decompressed.empty() ? &emptyOutput : decompressed.data(),
        compressedSize,
        decompressedSize
    );
    if (decodedSize < 0)
        throw std::runtime_error("malformed LZ4 block");
    if (decodedSize != decompressedSize)
        throw std::runtime_error("decompressed size mismatch");

    std::vector<std::pair<uint64_t, std::string>> records;
    records.reserve(count);
    offset = 0;

    for (std::size_t i = 0; i < count; ++i) {
        const uint64_t hash = read_little_endian(decompressed, offset, sizeof(uint64_t), "record hash");
        const auto terminator = std::find(decompressed.begin() + offset, decompressed.end(), '\0');
        if (terminator == decompressed.end())
            throw std::runtime_error("truncated record string");
        records.emplace_back(hash, std::string(decompressed.begin() + offset, terminator));
        offset = static_cast<std::size_t>(terminator - decompressed.begin()) + 1;
    }

    if (offset != decompressed.size())
        throw std::runtime_error("trailing record data");
    return records;
}
} // namespace

HashResolver::HashResolver(const std::filesystem::path& packageIndexDirectory, const int requestedBitLength) {
    if (!std::filesystem::exists(packageIndexDirectory))
        return;

    const std::filesystem::path bitLengthPath = packageIndexDirectory / ".bit_length";
    int bitLength = requestedBitLength;
    if (std::filesystem::exists(bitLengthPath)) {
        bitLength = read_bit_length(bitLengthPath);
    } else if (bitLength < 0 || bitLength > 64) {
        throw std::runtime_error(
            "Invalid --bit-length value: expected 0-64, got " + std::to_string(bitLength)
        );
    }

    hashMask = bitLength == 64 ? ~uint64_t{0} : (uint64_t{1} << bitLength) - 1;

    std::vector<std::filesystem::path> packageFiles;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(packageIndexDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wni")
            packageFiles.emplace_back(entry.path());
    }

    std::ranges::sort(packageFiles, {}, [](const std::filesystem::path& path) { return path.generic_string(); });

    for (const std::filesystem::path& packageFile : packageFiles) {
        try {
            std::vector<std::pair<uint64_t, std::string>> records = parse_file(packageFile);
            entries.reserve(entries.size() + records.size());
            for (auto& [hash, value] : records)
                entries.insert_or_assign(mask(hash), std::move(value));
        } catch (const std::exception& error) {
            std::println(
                stderr, "Error loading package file {}: {}", packageFile.filename().string(), error.what()
            );
        }
    }
}

const std::string* HashResolver::resolve(const uint64_t hash) const noexcept {
    const auto entry = entries.find(mask(hash));
    return entry == entries.end() ? nullptr : &entry->second;
}
