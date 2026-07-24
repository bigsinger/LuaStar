#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

class MapConfig final {
public:
    static MapConfig load(const std::filesystem::path& file);

    [[nodiscard]] bool contains(std::wstring_view key) const;
    [[nodiscard]] std::optional<std::wstring> get(std::wstring_view key) const;
    [[nodiscard]] std::wstring get_or(
        std::wstring_view key,
        std::wstring_view fallback) const;

private:
    std::map<std::wstring, std::wstring, std::less<>> values_;
};
