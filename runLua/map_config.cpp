#include "map_config.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

std::wstring trim(std::wstring value) {
    constexpr std::wstring_view whitespace = L" \t\r\n";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::wstring::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::wstring lower(std::wstring value) {
    std::ranges::transform(value, value.begin(), [](const wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring decode(const std::vector<char>& bytes) {
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        if ((bytes.size() - 2) % sizeof(wchar_t) != 0) {
            throw std::runtime_error("UTF-16 配置文件长度无效。");
        }
        const auto count = (bytes.size() - 2) / sizeof(wchar_t);
        std::wstring result(count, L'\0');
        std::memcpy(
            result.data(), bytes.data() + 2, count * sizeof(wchar_t));
        return result;
    }

    std::size_t offset = 0;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        offset = 3;
    }

    if (bytes.size() == offset) {
        return {};
    }

    const auto input = bytes.data() + offset;
    const auto input_size = static_cast<int>(bytes.size() - offset);
    int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input, input_size, nullptr, 0);
    UINT code_page = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (count == 0) {
        code_page = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(
            code_page, flags, input, input_size, nullptr, 0);
    }
    if (count <= 0) {
        throw std::runtime_error("配置文件编码无效。");
    }

    std::wstring text(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(
        code_page, flags, input, input_size, text.data(), count);
    return text;
}

std::wstring unescape(std::wstring value) {
    std::wstring result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != L'\\' || index + 1 >= value.size()) {
            result.push_back(value[index]);
            continue;
        }
        const auto next = value[++index];
        switch (next) {
        case L'n':
            result.push_back(L'\n');
            break;
        case L'r':
            result.push_back(L'\r');
            break;
        case L't':
            result.push_back(L'\t');
            break;
        case L'\\':
            result.push_back(L'\\');
            break;
        case L'"':
            result.push_back(L'"');
            break;
        default:
            result.push_back(L'\\');
            result.push_back(next);
            break;
        }
    }
    return result;
}

} // namespace

MapConfig MapConfig::load(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("无法打开配置文件。");
    }
    const std::vector<char> bytes(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    const auto text = decode(bytes);

    MapConfig config;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(L'\n', start);
        auto line = trim(text.substr(
            start,
            end == std::wstring::npos ? std::wstring::npos : end - start));
        if (!line.empty() && line.front() != L'#' && line.front() != L';') {
            const auto equals = line.find(L'=');
            if (equals != std::wstring::npos) {
                auto key = lower(trim(line.substr(0, equals)));
                auto value = trim(line.substr(equals + 1));
                bool quoted = false;
                if (value.size() >= 2 && value.front() == L'"' &&
                    value.back() == L'"') {
                    quoted = true;
                    value = value.substr(1, value.size() - 2);
                }
                if (!key.empty()) {
                    config.values_[std::move(key)] =
                        quoted ? unescape(std::move(value)) : std::move(value);
                }
            }
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }
    return config;
}

bool MapConfig::contains(const std::wstring_view key) const {
    return values_.contains(lower(std::wstring(key)));
}

std::optional<std::wstring> MapConfig::get(const std::wstring_view key) const {
    const auto iterator = values_.find(lower(std::wstring(key)));
    if (iterator == values_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::wstring MapConfig::get_or(
    const std::wstring_view key,
    const std::wstring_view fallback) const {
    const auto value = get(key);
    return value.value_or(std::wstring(fallback));
}
