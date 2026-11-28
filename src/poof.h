#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <array>
#include <chrono>
#include <optional>

enum class PerchEvent { None, Appear, Disappear, Move };
enum class PoofMode { Quiet, Always, Off };
enum class PoofStyle { None, Cloud, Fade };

class PerchPresence {
public:
    using Clock = std::chrono::steady_clock;

    PerchEvent Update(Clock::time_point now, HWND observed, bool holding);
    HWND Shown() const { return shown_; }
    bool Settling() const { return pending_.has_value(); }

private:
    struct Pending {
        HWND target;
        Clock::time_point since;
    };
    HWND shown_ = nullptr;
    std::optional<Pending> pending_;
};

class PoofGate {
public:
    bool AllowsEffect(PerchEvent event, PerchPresence::Clock::time_point now, PoofMode mode);

private:
    std::optional<PerchPresence::Clock::time_point> lastChange_;
};

PoofStyle PoofStyleFor(bool puff, bool reduceMotion);

constexpr int poofMargin = 32;
constexpr auto poofDuration = std::chrono::milliseconds(550);
constexpr float poofPeak = 0.24f;

struct Puff {
    Gdiplus::PointF center;
    float radius;
};

std::array<Puff, 7> PoofPuffs(float progress);
float PoofCatOpacity(bool appearing, bool reduceMotion, float progress);
void DrawPoof(Gdiplus::Graphics& graphics, float progress, float scale);
void DrawWithOpacity(Gdiplus::Graphics& graphics, Gdiplus::Bitmap& image, Gdiplus::RectF target, float opacity);
