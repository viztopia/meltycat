#include "poof.h"

#include <algorithm>
#include <cmath>

using namespace Gdiplus;

PerchEvent PerchPresence::Update(Clock::time_point now, HWND observed, bool holding) {
    if (observed == shown_) {
        pending_.reset();
        return PerchEvent::None;
    }
    if (shown_ && observed) {
        shown_ = observed;
        pending_.reset();
        return PerchEvent::Move;
    }
    if (!pending_ || pending_->target != observed) pending_ = Pending{observed, now};
    constexpr auto settle = std::chrono::milliseconds(120);
    if (now - pending_->since < settle || (observed && holding)) return PerchEvent::None;
    shown_ = observed;
    pending_.reset();
    return observed ? PerchEvent::Appear : PerchEvent::Disappear;
}

namespace {

const PointF poofCenter{100, 70};

struct Billow {
    float dx, dy, radius, swell, fade, driftX, driftY;
};

constexpr Billow billows[]{
    {-2, 0, 36, .03f, .55f, 2, -10},
    {-40, -10, 27, 0, .38f, -10, -8},
    {34, -8, 24, .02f, .46f, 9, -12},
    {48, 26, 19, .05f, .3f, 12, 2},
    {6, 24, 22, .04f, .33f, 3, 6},
    {-28, 24, 15, .06f, .26f, -9, 4},
    {-18, -32, 13, .01f, .29f, -4, -11},
};

float Clamp(float t) { return std::clamp(t, 0.0f, 1.0f); }

float Smooth(float start, float end, float value) {
    const float t = Clamp((value - start) / (end - start));
    return t * t * (3 - 2 * t);
}

float EaseOut(float t) {
    const float u = 1 - Clamp(t);
    return 1 - u * u * u;
}

float Swell(float t) {
    const float u = Clamp(t) - 1;
    return 1 + 2.2f * u * u * u + 1.2f * u * u;
}

RectF Circle(PointF center, float radius) {
    return {center.X - radius, center.Y - radius, 2 * radius, 2 * radius};
}

void DrawCrispCloud(Graphics& g, const std::array<Puff, 7>& puffs) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen outline(Color(115, 140, 135, 133), 1.6f);
    SolidBrush shade(Color(255, 237, 235, 230));
    SolidBrush light(Color(255, 255, 254, 251));
    for (const auto& p : puffs) if (p.radius > 0.3f) g.DrawEllipse(&outline, Circle(p.center, p.radius));
    for (const auto& p : puffs) if (p.radius > 0.3f) g.FillEllipse(&shade, Circle(p.center, p.radius));
    for (const auto& p : puffs) {
        if (p.radius <= 0.3f) continue;
        const PointF center{p.center.X - 0.08f * p.radius, p.center.Y - 0.12f * p.radius};
        g.FillEllipse(&light, Circle(center, p.radius * 0.84f));
    }
}

void DrawHaze(Graphics& g, const std::array<Puff, 7>& puffs, float opacity) {
    const Color center(static_cast<BYTE>(191 * opacity), 255, 254, 251);
    const Color edge(0, 255, 254, 251);
    for (const auto& p : puffs) {
        if (p.radius <= 0.3f) continue;
        GraphicsPath path;
        path.AddEllipse(Circle(p.center, p.radius * 1.2f));
        PathGradientBrush gradient(&path);
        gradient.SetCenterPoint(p.center);
        gradient.SetCenterColor(center);
        INT count = 1;
        gradient.SetSurroundColors(&edge, &count);
        gradient.SetFocusScales(0.35f, 0.35f);
        g.FillPath(&gradient, &path);
    }
}

} // namespace

std::array<Puff, 7> PoofPuffs(float progress) {
    const float t = Clamp(progress);
    std::array<Puff, 7> result{};
    const float drift = 0.5f * EaseOut((t - poofPeak) / (1 - poofPeak));
    const float spread = 0.7f + 0.3f * EaseOut(t / poofPeak);
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto& b = billows[i];
        const float grow = t <= b.swell ? 0 : Swell((t - b.swell) / (poofPeak - 0.02f));
        const float shrink = std::pow(Clamp((t - b.fade) / (0.98f - b.fade)), 1.2f);
        const float shrinkBy = b.radius < 20 ? 1.0f : 0.55f;
        result[i] = {{poofCenter.X + b.dx * spread + b.driftX * drift,
                      poofCenter.Y + b.dy * spread + b.driftY * drift},
                     std::max(0.0f, b.radius * grow * (1 - shrinkBy * shrink))};
    }
    return result;
}

float PoofCatOpacity(bool appearing, bool reduceMotion, float progress) {
    const float shown = reduceMotion ? Clamp(progress) : Smooth(poofPeak - 0.07f, poofPeak + 0.07f, progress);
    return appearing ? shown : 1 - shown;
}

void DrawWithOpacity(Graphics& g, Bitmap& image, RectF target, float opacity) {
    if (opacity <= 0) return;
    const ColorMatrix matrix{{{1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0},
                              {0, 0, 0, opacity, 0}, {0, 0, 0, 0, 1}}};
    ImageAttributes attributes;
    attributes.SetColorMatrix(&matrix);
    g.DrawImage(&image, target, 0, 0, static_cast<float>(image.GetWidth()), static_cast<float>(image.GetHeight()),
                UnitPixel, &attributes);
}

void DrawPoof(Graphics& g, float progress, float scale) {
    if (progress <= 0 || progress >= 1) return;
    const auto puffs = PoofPuffs(progress);
    const float alpha = 1 - Smooth(0.5f, 1, progress);
    const float haze = Smooth(poofPeak + 0.08f, 0.62f, progress);
    if (haze < 1) {
        // Composite the union once so overlapping billows have just one outline and opacity.
        const int width = static_cast<int>(std::ceil(190 * scale));
        const int height = static_cast<int>(std::ceil(140 * scale));
        Bitmap crisp(width, height, PixelFormat32bppPARGB);
        {
            Graphics layer(&crisp);
            layer.Clear(Color(0, 0, 0, 0));
            layer.ScaleTransform(scale, scale);
            DrawCrispCloud(layer, puffs);
        }
        DrawWithOpacity(g, crisp, RectF(0, 0, width / scale, height / scale), alpha * 0.96f * (1 - haze));
    }
    if (haze > 0) DrawHaze(g, puffs, alpha * haze);
}

