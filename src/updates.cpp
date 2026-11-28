#include "updates.h"

#include "meltycat-version.h"

#include <windows.h>
#include <roapi.h>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace updates {
namespace {

std::vector<std::int64_t> VersionParts(std::wstring_view version) {
    version = VersionName(version);
    const auto end = version.find_first_not_of(L"0123456789.");
    version = version.substr(0, end);
    std::vector<std::int64_t> parts;
    while (!version.empty()) {
        const auto separator = version.find(L'.');
        const auto part = version.substr(0, separator);
        if (!part.empty()) {
            std::int64_t number = 0;
            for (const auto character : part) {
                const auto digit = character - L'0';
                if (number > (std::numeric_limits<std::int64_t>::max() - digit) / 10) return {};
                number = number * 10 + digit;
            }
            parts.push_back(number);
        }
        if (separator == std::wstring_view::npos) break;
        version.remove_prefix(separator + 1);
    }
    return parts;
}

std::optional<std::string> FetchReleaseData() {
    using InternetHandle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;
    const InternetHandle session(WinHttpOpen(L"meltycat", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
                                 WinHttpCloseHandle);
    constexpr int timeout = 15000;
    if (!session || !WinHttpSetTimeouts(session.get(), timeout, timeout, timeout, timeout)) return std::nullopt;
    const InternetHandle connection(WinHttpConnect(session.get(), L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0),
                                    WinHttpCloseHandle);
    if (!connection) return std::nullopt;
    const InternetHandle request(WinHttpOpenRequest(connection.get(), L"GET",
                                                    L"/repos/viztopia/meltycat/releases/latest",
                                                    nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                    WINHTTP_FLAG_SECURE), WinHttpCloseHandle);
    if (!request) return std::nullopt;
    // Accept only a direct answer from the public endpoint, without cookies or credentials.
    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION;
    constexpr wchar_t accept[] = L"Accept: application/vnd.github+json\r\n";
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
        !WinHttpSendRequest(request.get(), accept, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        return std::nullopt;
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) ||
        status != HTTP_STATUS_OK) {
        return std::nullopt;
    }

    constexpr std::size_t maxReleaseBytes = 4 * 1024 * 1024;
    const auto readDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    std::array<char, 8192> chunk{};
    std::string response;
    for (;;) {
        DWORD read = 0;
        if (std::chrono::steady_clock::now() >= readDeadline ||
            !WinHttpReadData(request.get(), chunk.data(), static_cast<DWORD>(chunk.size()), &read) ||
            response.size() + read > maxReleaseBytes) {
            return std::nullopt;
        }
        if (read == 0) return response;
        response.append(chunk.data(), read);
    }
}

} // namespace

std::wstring_view CurrentVersion() {
    return L"" MELTYCAT_VERSION;
}

std::wstring_view VersionName(std::wstring_view tag) {
    while (!tag.empty() && tag.front() == L'v') tag.remove_prefix(1);
    return tag;
}

bool IsNewer(std::wstring_view tag, std::wstring_view current) {
    const auto latest = VersionParts(tag);
    const auto running = VersionParts(current);
    if (latest.empty() || running.empty()) return false;
    for (std::size_t i = 0; i < std::max(latest.size(), running.size()); ++i) {
        const auto a = i < latest.size() ? latest[i] : 0;
        const auto b = i < running.size() ? running[i] : 0;
        if (a != b) return a > b;
    }
    return false;
}

std::optional<std::wstring> LatestTag(std::string_view json) {
    using namespace winrt::Windows::Data::Json;
    try {
        JsonObject object{nullptr};
        if (!JsonObject::TryParse(winrt::to_hstring(json), object)) return std::nullopt;
        const auto tag = object.GetNamedValue(L"tag_name", nullptr);
        if (!tag || tag.ValueType() != JsonValueType::String) return std::nullopt;
        return std::wstring(tag.GetString());
    } catch (const winrt::hresult_error&) {
        return std::nullopt;
    }
}

std::chrono::seconds Now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
}

bool AutoCheckDue(std::chrono::seconds now, std::optional<std::chrono::seconds> last,
                  bool enabled, std::wstring_view current) {
    return enabled && current != L"0.0.0" && (!last || now - *last >= std::chrono::hours(24));
}

std::optional<std::wstring> FetchLatestTag() {
    const auto response = FetchReleaseData();
    if (!response || FAILED(RoInitialize(RO_INIT_MULTITHREADED))) return std::nullopt;
    struct UninitializeApartment {
        ~UninitializeApartment() { RoUninitialize(); }
    } apartment;
    return LatestTag(*response);
}

} // namespace updates
