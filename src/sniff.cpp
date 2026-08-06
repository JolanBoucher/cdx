#include "sniff.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>

namespace cdx_toolkit {

namespace {

constexpr std::array<char, 4> CDX_MAGIC = {'C', 'D', 'X', '\x01'};
constexpr std::array<char, 4> GBZ_MAGIC = {'G', 'B', 'Z', ' '};
constexpr std::array<unsigned char, 2> GAM_MAGIC = {0x1F, 0x8B}; // gzip/BGZF

[[nodiscard]] std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

[[nodiscard]] bool ends_with(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

InputType sniff_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return InputType::Unknown;
    }

    std::array<unsigned char, 4> header{};
    in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    const auto bytes_read = in.gcount();

    if (bytes_read >= 4 &&
        std::equal(CDX_MAGIC.begin(), CDX_MAGIC.end(), header.begin(),
                   [](char expected, unsigned char actual) {
                       return static_cast<unsigned char>(expected) == actual;
                   })) {
        return InputType::Cdx;
    }

    if (bytes_read >= 4 &&
        std::equal(GBZ_MAGIC.begin(), GBZ_MAGIC.end(), header.begin(),
                   [](char expected, unsigned char actual) {
                       return static_cast<unsigned char>(expected) == actual;
                   })) {
        return InputType::Gbz;
    }

    if (bytes_read >= 2 && header[0] == GAM_MAGIC[0] && header[1] == GAM_MAGIC[1]) {
        return InputType::Gam;
    }

    return InputType::Unknown;
}

bool file_is_openable(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return static_cast<bool>(in);
}

InputType guess_type_from_extension(const std::string& path) {
    const std::string lower = to_lower(path);

    if (ends_with(lower, ".cdx") || ends_with(lower, ".cdx.zst")) {
        return InputType::Cdx;
    }
    if (ends_with(lower, ".gbz")) {
        return InputType::Gbz;
    }
    if (ends_with(lower, ".gam")) {
        return InputType::Gam;
    }

    return InputType::Unknown;
}

} // namespace cdx_toolkit
