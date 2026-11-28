#include "../src/updates.h"

#include <roapi.h>

#include <iostream>

namespace {

using namespace std::chrono_literals;

void Check(bool condition, std::string_view label, int& failures) {
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    failures += !condition;
}

void CheckVersions(int& failures) {
    Check(updates::IsNewer(L"v0.2.2", L"0.2.1"), "a later patch is newer", failures);
    Check(updates::IsNewer(L"v0.10.0", L"0.9.9"), "versions compare as numbers", failures);
    Check(updates::IsNewer(L"v1.0", L"0.9.9"), "a short major release can be newer", failures);
    Check(!updates::IsNewer(L"v1.0.0", L"1.0"), "a missing part counts as zero", failures);
    Check(!updates::IsNewer(L"v0.2.1", L"0.2.1"), "the same version is not newer", failures);
    Check(!updates::IsNewer(L"v0.2.1", L"0.2.1-dev"), "a dev build of the same version is not behind", failures);
    Check(!updates::IsNewer(L"v0.2.0", L"0.2.1"), "an older release is not newer", failures);
    Check(!updates::IsNewer(L"nightly", L"0.2.1"), "an invalid tag is never newer", failures);
    Check(updates::VersionName(L"vv0.2.1") == L"0.2.1", "displayed versions omit leading v characters", failures);
}

void CheckReleaseResponse(int& failures) {
    Check(updates::LatestTag(R"({"tag_name": "v0.2.1", "name": "MeltyCat 0.2.1"})") == L"v0.2.1",
          "reads the tag of the latest release", failures);
    Check(!updates::LatestTag(R"({"message": "Not Found"})"), "no tag in an error reply", failures);
    Check(!updates::LatestTag(R"({"tag_name": 21})"), "the tag must be a JSON string", failures);
    Check(!updates::LatestTag("<html>"), "no tag in a reply that isn't JSON", failures);
}

void CheckSchedule(int& failures) {
    constexpr auto now = 1000000s;
    Check(updates::AutoCheckDue(now, std::nullopt, true, L"0.2.1"), "checks on the first launch", failures);
    Check(!updates::AutoCheckDue(now, now - 23h, true, L"0.2.1"), "a check within the last day suppresses a retry", failures);
    Check(updates::AutoCheckDue(now, now - 24h, true, L"0.2.1"), "checks again after exactly 24 hours", failures);
    Check(updates::AutoCheckDue(now, now - 25h, true, L"0.2.1"), "an older check is due", failures);
    Check(!updates::AutoCheckDue(now, std::nullopt, false, L"0.2.1"), "does not check automatically when off", failures);
    Check(!updates::AutoCheckDue(now, std::nullopt, true, L"0.0.0"), "an untagged build skips automatic checks", failures);
}

} // namespace

int main() {
    if (FAILED(RoInitialize(RO_INIT_SINGLETHREADED))) return 2;
    int failures = 0;
    CheckVersions(failures);
    CheckReleaseResponse(failures);
    CheckSchedule(failures);
    RoUninitialize();
    return failures ? 1 : 0;
}
