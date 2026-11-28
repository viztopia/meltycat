#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace updates {

inline constexpr wchar_t downloadPage[] = L"https://github.com/viztopia/meltycat/releases/latest";

std::wstring_view CurrentVersion();
std::wstring_view VersionName(std::wstring_view tag);
bool IsNewer(std::wstring_view tag, std::wstring_view current);
std::optional<std::wstring> LatestTag(std::string_view json);

std::chrono::seconds Now();
bool AutoCheckDue(std::chrono::seconds now, std::optional<std::chrono::seconds> last,
                  bool enabled, std::wstring_view current);
std::optional<std::wstring> FetchLatestTag();

} // namespace updates
