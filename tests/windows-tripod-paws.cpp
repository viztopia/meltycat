#include "../src/cat.h"
#include "../src/skin_library.h"

#include <winrt/base.h>

#include <iostream>

namespace {

using cat::Pose;
using cat::SkinSpec;

constexpr std::string_view skinStart =
    R"({"formatVersion":1,"name":"Tripod test","body":"shorthair","palette":{"coat":"#19191D","iris":"#CFF54F"})";

void Check(bool condition, std::string_view label, int& failures) {
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    failures += !condition;
}

void CheckParsing(int& failures) {
    std::wstring error;
    const auto parse = [&](std::string_view fields) {
        return cat::ParseSkin(std::string(skinStart).append(fields).append("}"), error);
    };
    const auto defaultSkin = parse("");
    Check(defaultSkin && !defaultSkin->missingLeftFrontPaw && !defaultSkin->missingRightFrontPaw,
          "front paws are present by default", failures);
    const auto right = parse(R"(,"missingFrontPaws":["right"])");
    Check(right && !right->missingLeftFrontPaw && right->missingRightFrontPaw,
          "right is the only missing front paw", failures);
    const auto both = parse(R"(,"missingFrontPaws":["left","right","left"])");
    Check(both && both->missingLeftFrontPaw && both->missingRightFrontPaw,
          "both sides are accepted, including duplicate entries", failures);
    Check(!parse(R"(,"missingFrontPaws":["left","middle"])") &&
              error == L"missingFrontPaws[1]: expected \"left\" or \"right\"",
          "unknown side reports its array index", failures);
    Check(!parse(R"(,"missingFrontPaws":[false])") &&
              error == L"missingFrontPaws[0]: expected \"left\" or \"right\"",
          "non-string side reports its array index", failures);
    Check(!parse(R"(,"missingFrontPaws":"right")") && error == L"missingFrontPaws: must be an array",
          "non-array field reports its type error", failures);
}

std::vector<BYTE> Render(const Pose& pose, const SkinSpec& skin) {
    constexpr int rows = cat::height + cat::topPadding;
    constexpr int stride = cat::width * 4;
    std::vector<BYTE> pixels(rows * stride);
    Gdiplus::Bitmap image(cat::width, rows, stride, PixelFormat32bppPARGB, pixels.data());
    {
        Gdiplus::Graphics graphics(&image);
        graphics.TranslateTransform(0, float(cat::topPadding));
        cat::Draw(graphics, pose, skin, float(cat::topPadding));
    }
    return pixels;
}

int RunTests(const wchar_t* fixture) {
    int failures = 0;
    CheckParsing(failures);

    const auto directory = std::filesystem::path(L"build") /
                           (L"tripod-import-" + std::to_wstring(GetCurrentProcessId()));
    SkinLibrary library(fixture ? directory : std::filesystem::path{});
    const auto black = library.Find("black");
    if (!black) return 1;
    auto noRight = *black;
    noRight.missingRightFrontPaw = true;
    auto noLeft = *black;
    noLeft.missingLeftFrontPaw = true;

    const Pose awake{{-60, 120}, 0, 0, 1, 1, 1, 1, 0, 0};
    auto braced = awake;
    braced.hunt = 1;
    braced.brace = 1;
    braced.cursor = {120, -40};
    auto asleep = awake;
    asleep.alert = 0;
    asleep.eyeOpen = 0;
    asleep.tailMotion = 0;

    std::optional<SkinEntry> imported;
    if (fixture) {
        std::wstring error;
        const auto bytes = ReadSkinFile(fixture, error);
        if (bytes) imported = library.Import(*bytes, error);
        if (!imported) std::wcerr << error << '\n';
        Check(imported && imported->skin->missingRightFrontPaw && !imported->skin->missingLeftFrontPaw,
              "the handoff skin imports with only its right paw missing", failures);
    }

    const struct { const char* name; Pose pose; } poses[]{
        {"awake", awake}, {"braced", braced}, {"asleep", asleep}
    };
    for (const auto& [name, pose] : poses) {
        auto farTucked = pose;
        farTucked.pawTuckLeft = 1;
        auto nearTucked = pose;
        nearTucked.pawTuckRight = 1;
        const auto withoutRight = Render(pose, noRight);
        Check(withoutRight == Render(farTucked, *black) && withoutRight != Render(pose, *black),
              std::string(name) + " missing right matches a fully tucked far paw", failures);
        const auto withoutLeft = Render(pose, noLeft);
        Check(withoutLeft == Render(nearTucked, *black) && withoutLeft != Render(pose, *black),
              std::string(name) + " missing left matches a fully tucked near paw", failures);
        if (imported) {
            auto complete = *imported->skin;
            complete.missingRightFrontPaw = false;
            auto bothTucked = farTucked;
            bothTucked.pawTuckRight = 1;
            const auto pixels = Render(pose, *imported->skin);
            Check(pixels == Render(farTucked, complete) && pixels != Render(nearTucked, complete) &&
                      pixels != Render(bothTucked, complete),
                  std::string(name) + " imported Tripod shows only the viewer-right paw", failures);
        }
    }

    constexpr float headroom = float(cat::topPadding);
    const Gdiplus::PointF farPaw{71, 84};
    const Gdiplus::PointF nearPaw{107, 91};
    Check(cat::HitsPettingArea(farPaw, awake, *black, headroom) &&
              !cat::HitsPettingArea(farPaw, awake, noRight, headroom) &&
              cat::HitsPettingArea(farPaw, awake, noLeft, headroom),
          "far paw can be petted only when present", failures);
    Check(cat::HitsPettingArea(nearPaw, awake, *black, headroom) &&
              !cat::HitsPettingArea(nearPaw, awake, noLeft, headroom) &&
              cat::HitsPettingArea(nearPaw, awake, noRight, headroom),
          "near paw can be petted only when present", failures);
    if (imported) {
        Check(library.Delete(imported->id), "temporary import removed", failures);
        std::filesystem::remove(directory);
    }
    return failures != 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc > 2) return 2;
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) return 2;
    const int result = RunTests(argc == 2 ? argv[1] : nullptr);
    Gdiplus::GdiplusShutdown(token);
    winrt::uninit_apartment();
    return result;
}
