#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <string_view>

namespace cat {

constexpr int width = 224;
constexpr int height = 190;
constexpr int topPadding = 24;
constexpr int cornerX = 152;
constexpr int cornerY = 72;
constexpr int foldedRight = 167;
constexpr float dreamLifetime = 1.8f;
constexpr float dreamStagger = 0.32f;
constexpr float chatterMarkLifetime = 0.48f;
constexpr float chatterMarkStagger = 0.08f;
constexpr float petSymbolLifetime = 1.55f;
constexpr float petSymbolStagger = 0.23f;

enum class Skin { Black, Uni, OrangeTabby, Tuxedo, LiHua, Ragdoll, MaineCoon, Siberian };

constexpr bool IsLonghair(Skin skin) {
    return skin == Skin::Ragdoll || skin == Skin::MaineCoon || skin == Skin::Siberian;
}

struct Pose {
    Gdiplus::PointF cursor;
    float swing;
    float curl;
    float eyeOpen;
    float tailDirection;
    float alert;
    float tailMotion;
    float dreamAge;
    int dreamCount;
    float hunt = 0;
    float brace = 0;
    float tailTwitch = 0;
    float chatter = 0;
    float chatterAge = -1;
    float earTwitch = 0;
    std::string_view chatterCode{};
    float squint = 0;
    float petSymbolAge = 0;
    int petSymbolCount = 0;
    float pawTuckLeft = 0;
    float pawTuckRight = 0;
    float tailWave = 0;
    float tailWavePhase = 0;
    bool petIsNote = false;
};

bool HitsPettingArea(Gdiplus::PointF cursor, const Pose& pose, Skin skin, float headroom);
void Draw(Gdiplus::Graphics& graphics, const Pose& pose, Skin skin, float headroom);
void DrawTrayIcon(Gdiplus::Graphics& graphics, Skin skin = Skin::Black);

} // namespace cat
