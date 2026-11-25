#include "cat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>

namespace cat {
namespace {

using namespace Gdiplus;

struct TabbyFur {
    Color stripe;
    Color belly;
    Color pawTip;
    Color innerEar;
    Color edge;
};

const TabbyFur orangeFur{{255, 179, 82, 33}, {255, 248, 178, 93}, {255, 255, 210, 146},
                        {255, 225, 134, 102}, {170, 137, 66, 35}};
const TabbyFur liHuaFur{{255, 72, 59, 46}, {255, 184, 157, 114}, {255, 165, 140, 108},
                       {255, 177, 121, 111}, {180, 92, 71, 55}};
const TabbyFur maineCoonFur{{255, 88, 60, 50}, {255, 187, 151, 126}, {255, 173, 138, 115},
                            {255, 221, 164, 148}, {225, 79, 55, 47}};
const TabbyFur siberianFur{{255, 189, 186, 183}, {255, 217, 216, 211}, {255, 168, 166, 163},
                           {255, 210, 190, 186}, {207, 120, 120, 119}};

struct Palette {
    Color coat;
    Color iris;
    Color pupil;
    Color eyelid;
    Color whisker;
    Color crease;
    const TabbyFur* tabby = nullptr;
};

const Palette black{{255, 25, 25, 29}, {255, 207, 245, 79}, {255, 25, 25, 29}, {255, 154, 171, 79},
                    {240, 159, 162, 165}, {215, 131, 138, 140}};
const Palette uni{{255, 46, 47, 51}, {255, 242, 204, 86}, {255, 46, 47, 51}, {255, 187, 155, 75},
                  {240, 175, 175, 172}, {215, 117, 122, 128}};
const Palette orangeTabby{{255, 236, 151, 63}, {255, 188, 217, 114}, {255, 52, 37, 27}, {255, 116, 79, 42},
                         {240, 255, 225, 173}, {215, 126, 65, 34}, &orangeFur};
const Palette tuxedo{{255, 32, 33, 39}, {255, 200, 214, 141}, {255, 30, 32, 36}, {255, 161, 172, 116},
                     {240, 202, 204, 203}, {215, 128, 127, 126}};
const Palette liHua{{255, 156, 132, 100}, {255, 190, 204, 128}, {255, 45, 39, 34}, {255, 97, 94, 57},
                    {240, 237, 222, 191}, {215, 90, 68, 53}, &liHuaFur};
const Palette ragdoll{{255, 243, 233, 218}, {255, 110, 174, 200}, {255, 44, 49, 63}, {255, 88, 71, 72},
                      {180, 161, 139, 132}, {195, 145, 122, 116}};
const Palette maineCoon{{255, 147, 111, 91}, {255, 223, 187, 132}, {255, 54, 39, 34}, {255, 74, 53, 43},
                        {220, 243, 214, 184}, {210, 90, 62, 51}, &maineCoonFur};
const Color maineCoonRuff(255, 224, 192, 161);
const Palette siberian{{255, 233, 232, 225}, {255, 163, 185, 195}, {255, 45, 43, 47}, {255, 86, 85, 86},
                       {210, 177, 174, 171}, {205, 115, 113, 113}, &siberianFur};
const Color siberianWhite(255, 246, 245, 238);
const Color cream(255, 246, 240, 225);
const Color shadowedCream(255, 218, 214, 200);
const Color nose(255, 45, 42, 44);
const Color whiteFur(255, 249, 247, 240);
const Color ragdollPoints(255, 108, 83, 82);
const Color ragdollBack(255, 226, 207, 187);
const Color ragdollIvory(255, 253, 247, 236);
const Color ragdollInk(203, 114, 92, 85);
const PointF faceCenter{56.0f, 53.0f};
constexpr float fullHeadLift = 20.0f;

float UpperBodyBrace(const Pose& pose, float headroom) {
    // The resting ear silhouette already leaves a few pixels above it.
    return pose.brace * std::min(1.0f, (headroom + 6.0f) / fullHeadLift);
}

const Palette& PaletteFor(Skin skin) {
    switch (skin) {
    case Skin::Uni: return uni;
    case Skin::OrangeTabby: return orangeTabby;
    case Skin::Tuxedo: return tuxedo;
    case Skin::LiHua: return liHua;
    case Skin::Ragdoll: return ragdoll;
    case Skin::MaineCoon: return maineCoon;
    case Skin::Siberian: return siberian;
    default: return black;
    }
}

bool HasWhitePaws(Skin skin) {
    return skin == Skin::Uni || skin == Skin::Tuxedo || skin == Skin::Ragdoll;
}

void Outline(GraphicsPath& path, std::initializer_list<PointF> points) {
    path.AddBeziers(points.begin(), static_cast<INT>(points.size()));
    path.CloseFigure();
}

void FillShape(Graphics& g, Color color, std::initializer_list<PointF> points) {
    GraphicsPath path;
    Outline(path, points);
    SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

void DrawFurOutline(Graphics& g, const GraphicsPath& silhouette, const Palette& palette, Skin skin) {
    const Color ink = palette.tabby ? palette.tabby->edge
                                    : skin == Skin::Ragdoll ? ragdollInk : Color(162, 49, 49, 51);
    Pen edge(ink, IsLonghair(skin) ? 1.0f : 0.8f);
    edge.SetLineJoin(LineJoinRound);
    g.DrawPath(&edge, &silhouette);
}

void Stroke(Graphics& g, std::initializer_list<PointF> points, Color color, float thickness) {
    Pen pen(color, thickness);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawBeziers(&pen, points.begin(), static_cast<INT>(points.size()));
}

constexpr int tailSegments = 24;
using TailPoints = std::array<PointF, tailSegments + 1>;

void DrawLonghairTail(Graphics& g, const TailPoints& spine, bool inward, const Palette& palette, Skin skin) {
    TailPoints front, back;

    for (int i = 0; i <= tailSegments; ++i) {
        const auto& previous = spine[std::max(0, i - 1)];
        const auto& next = spine[std::min(tailSegments, i + 1)];
        const float dx = next.X - previous.X;
        const float dy = next.Y - previous.Y;
        const float length = std::hypot(dx, dy);
        const PointF normal{-dy / length, dx / length};
        const float t = i / static_cast<float>(tailSegments);
        // A broad middle and a swept, tapered tip trace one continuous brush of fur.
        const float radius = (1.0f - t) * 6.5f + 12.8f * std::sin(3.14159265f * t);
        const float outerLock = std::max(0.0f, 1.0f - std::abs(i - 16.0f) / (i < 16 ? 5.0f : 1.0f));
        const float innerLock = std::max(0.0f, 1.0f - std::abs(i - 19.0f) / (i < 19 ? 5.0f : 1.0f));
        const float outer = radius * (1.0f + 0.14f * t) + 1.8f * outerLock;
        const float inner = radius * (1.0f - 0.18f * t) + 1.5f * innerLock;
        front[i] = {spine[i].X + outer * normal.X, spine[i].Y + outer * normal.Y};
        back[tailSegments - i] = {spine[i].X - inner * normal.X, spine[i].Y - inner * normal.Y};
    }

    const auto tuft = [&spine, &front, &back](int i, float across) {
        const PointF edge = across >= 0 ? front[i] : back[tailSegments - i];
        const float amount = std::abs(across);
        return PointF{spine[i].X + amount * (edge.X - spine[i].X),
                      spine[i].Y + amount * (edge.Y - spine[i].Y)};
    };
    // Follow the spine through the bend so the outer arc stays soft even at full curl.
    GraphicsPath plume(FillModeWinding);
    plume.AddCurve(front.data(), tailSegments + 1, 0.42f);
    plume.AddCurve(back.data(), tailSegments + 1, 0.42f);
    plume.CloseFigure();
    const auto state = g.Save();
    // The plume is wider than the other tails, so reserve room for its edge when folded inward.
    if (inward) {
        RectF bounds;
        plume.GetBounds(&bounds);
        g.TranslateTransform(-std::max(0.0f, bounds.GetRight() - (foldedRight - 1.5f)), 0.0f);
    }
    SolidBrush fur(palette.tabby ? palette.coat : Color(255, 134, 103, 100));
    g.FillPath(&fur, &plume);
    const auto fillState = g.Save();
    g.SetClip(&plume, CombineModeIntersect);
    FillShape(g, palette.tabby ? palette.tabby->belly : Color(255, 168, 134, 124),
              {tuft(0, -1), tuft(6, -1.7f), tuft(15, -1.6f), tuft(23, -0.48f),
               tuft(20, -0.45f), tuft(17, 0.10f), tuft(12, 0.18f),
               tuft(14, -0.03f), tuft(15, -0.22f), tuft(16, -0.32f),
               tuft(11, 0.04f), tuft(6, 0.63f), tuft(0, 0.48f),
               tuft(0, 0), tuft(0, -0.4f), tuft(0, -1)});
    if (palette.tabby) {
        // Bend each band around the plume and sweep its middle into a point.
        for (int i : {5, 12}) {
            FillShape(g, palette.tabby->stripe,
                      {tuft(i, -1.2f), tuft(i, -0.7f), tuft(i + 2, -0.3f), tuft(i + 3, 0),
                       tuft(i + 2, 0), tuft(i, 0.7f), tuft(i - 1, 1.2f),
                       tuft(i + 2, 1.2f), tuft(i + 4, 0.5f), tuft(i + 5, 0.1f),
                       tuft(i + 4, -0.3f), tuft(i + 2, -0.9f), tuft(i + 2, -1.2f)});
        }
        FillShape(g, palette.tabby->stripe,
                  {tuft(19, -1.3f), tuft(20, -0.8f), tuft(22, -0.2f), tuft(22, 0.1f),
                   tuft(21, 0), tuft(20, 0.1f), tuft(19, 0.1f),
                   tuft(20, 0.5f), tuft(19, 0.9f), tuft(18, 1.3f),
                   tuft(22, 1.8f), tuft(24, 1), spine.back(),
                   tuft(24, -1), tuft(22, -1.8f), tuft(19, -1.3f)});
    }
    g.Restore(fillState);
    DrawFurOutline(g, plume, palette, skin);
    g.Restore(state);
}

void DrawTail(Graphics& g, const Pose& pose, const Palette& palette, Skin skin) {
    const bool longhair = IsLonghair(skin);
    const float step = (longhair ? 68.0f : 59.0f) / tailSegments;
    const float restingAngle = pose.tailDirection < 0 ? 0.86f : 0.22f;
    constexpr float raisedAngle = 1.77f;
    TailPoints points;
    points[0] = longhair ? PointF{152, 100} : PointF{154, 122};
    for (int i = 1; i <= tailSegments; ++i) {
        const float u = (i - 0.5f) / tailSegments;
        const float blend = std::min(1.0f, 5.5f * u);
        float normalAngle = restingAngle * pose.tailMotion + pose.swing * (0.32f + 0.77f * u)
                            + 7.3f * pose.curl * u * std::sqrt(u);
        normalAngle += pose.tailWave * 0.75f * std::pow(u, 0.7f) * std::sin(pose.tailWavePhase - 6.5f * u);
        const float tip = std::max(0.0f, (u - 0.65f) / 0.35f);
        const float straightAngle = raisedAngle - 0.22f * (1.0f - blend);
        float angle = blend * normalAngle * (1.0f - pose.brace) + straightAngle * pose.brace;
        if (longhair) {
            const float turn = u * u * (3.0f - 2.0f * u);
            const float asleep = 0.07f + 0.49f * turn;
            const float awake = 0.62f + 2.34f * turn;
            const float rest = asleep + (awake - asleep) * pose.tailMotion;
            float curl = rest + pose.swing * (0.28f + 0.36f * u) + 1.3f * pose.curl * u;
            curl += pose.tailWave * 0.75f * std::pow(u, 0.7f) * std::sin(pose.tailWavePhase - 6.5f * u);
            angle = blend * curl * (1.0f - pose.brace) + (straightAngle + 0.53f * turn) * pose.brace;
        }
        angle += pose.tailTwitch * tip * tip;
        points[i] = {points[i - 1].X + pose.tailDirection * step * std::sin(angle),
                     points[i - 1].Y + step * std::cos(angle)};
    }

    if (longhair) {
        DrawLonghairTail(g, points, pose.tailDirection < 0, palette, skin);
        return;
    }

    Pen tail(palette.coat, 9.2f);
    tail.SetStartCap(LineCapRound);
    tail.SetEndCap(LineCapRound);
    constexpr float tension = 0.38f;
    g.DrawCurve(&tail, points.data(), tailSegments + 1, tension);
    if (palette.tabby) {
        Pen stripe(palette.tabby->stripe, 9.2f);
        for (const int start : {6, 10, 14, 18}) {
            g.DrawCurve(&stripe, points.data(), tailSegments + 1, start, 2, tension);
        }
        stripe.SetEndCap(LineCapRound);
        g.DrawCurve(&stripe, points.data(), tailSegments + 1, 22, 2, tension);
    }
}

struct Gaze {
    Gaze(PointF cursor, PointF center, float alert) {
        const float dx = cursor.X - center.X;
        const float dy = cursor.Y - center.Y;
        const float length = std::sqrt(dx * dx + dy * dy + 80.0f * 80.0f);
        x = (dx / length) * alert;
        y = (dy / length) * alert;
        yaw = x * 0.31f;
        pitch = y * 0.25f;
    }

    // Project features from the front of a shallow head volume onto the drawing plane.
    PointF Project(float localX, float localY, float depth) const {
        const float turnedDepth = depth * std::cos(yaw) - localX * std::sin(yaw);
        return {localX * std::cos(yaw) + depth * std::sin(yaw),
                localY * std::cos(pitch) + turnedDepth * std::sin(pitch)};
    }

    float x;
    float y;
    float yaw;
    float pitch;
};

struct HeadPosition {
    Gaze gaze;
    PointF center;
    float angle;
    float settle;
};

HeadPosition PositionHead(const Pose& pose, float lift) {
    const PointF center{faceCenter.X + lift * 0.12f, faceCenter.Y - lift};
    const Gaze gaze(pose.cursor, center, pose.alert);
    const float droop = 1.0f - pose.alert;
    const float settle = droop * droop;
    return {gaze, {center.X + gaze.x * 1.6f + settle * 5.5f, center.Y + gaze.y * 1.25f + droop * 5.8f},
            -4.0f * gaze.x - 17.0f * droop, settle};
}

void DrawEye(Graphics& g, float side, const Gaze& gaze, const Pose& pose, const Palette& palette, Skin skin) {
    const PointF center = gaze.Project(side * 13.6f, -1.0f, 15.0f);
    const float radiusX = 9.7f * (1.0f - side * gaze.x * 0.17f);
    const float radiusY = 9.1f * (1.0f - std::abs(gaze.y) * 0.14f);
    const auto state = g.Save();
    g.TranslateTransform(center.X, center.Y);
    g.RotateTransform(side * 3.4f + gaze.x * 2.0f);

    if (pose.eyeOpen < 0.18f) {
        const float lidAlert = IsLonghair(skin) && pose.squint > 0 ? 0.0f : pose.alert;
        const auto tint = [lidAlert](BYTE resting, BYTE awake) {
            return static_cast<BYTE>(resting + (awake - resting) * lidAlert);
        };
        const Color lid(255, tint(palette.eyelid.GetR(), palette.iris.GetR()),
                        tint(palette.eyelid.GetG(), palette.iris.GetG()),
                        tint(palette.eyelid.GetB(), palette.iris.GetB()));
        const float lidWidth = radiusX * (0.77f + 0.23f * pose.alert);
        const float lidCurve = 2.1f - 2.8f * pose.squint;
        Stroke(g, {{-lidWidth, 0}, {-lidWidth * 0.48f, lidCurve}, {lidWidth * 0.43f, lidCurve}, {lidWidth, 0}},
               lid, 1.35f + 0.4f * pose.squint);
    } else {
        g.ScaleTransform(1.0f, pose.eyeOpen);
        SolidBrush iris(palette.iris);
        g.FillEllipse(&iris, -radiusX, -radiusY, 2 * radiusX, 2 * radiusY);

        const float pupilX = gaze.x * (2.1f - 1.0f * pose.hunt);
        const float pupilY = gaze.y * (1.65f - 0.72f * pose.hunt);
        const float huntPupil = std::min(radiusX, radiusY) - std::hypot(pupilX, pupilY) - 1.65f;
        const float slitX = radiusX * 0.24f;
        const float slitY = radiusY * 0.72f;
        const float pupilRadiusX = slitX + (huntPupil - slitX) * pose.hunt;
        const float pupilRadiusY = slitY + (huntPupil - slitY) * pose.hunt;
        SolidBrush dark(palette.pupil);
        g.FillEllipse(&dark, pupilX - pupilRadiusX, pupilY - pupilRadiusY,
                      2.0f * pupilRadiusX, 2.0f * pupilRadiusY);
        const float shine = std::max(skin == Skin::Siberian ? 0.55f : 0.0f, (pose.hunt - 0.20f) / 0.80f);
        if (shine > 0) {
            const float size = 2.7f * shine;
            SolidBrush reflection(Color(static_cast<BYTE>(237 * shine), 255, 255, 247));
            g.FillEllipse(&reflection, pupilX - 0.47f * pupilRadiusX,
                          pupilY - 0.56f * pupilRadiusY, size, size);
        }
    }
    g.Restore(state);
}

void DrawWhiskers(Graphics& g, float side, const Gaze& gaze, float cheekPress, Color whisker) {
    for (int row = 0; row < 3; ++row) {
        const float y = 9.0f + row * 3.2f - cheekPress * (1.2f + 1.9f * row);
        const float lift = 2.2f - row * (2.25f - 0.32f * cheekPress);
        Stroke(g, {gaze.Project(side * 18.7f, y, 13.0f),
                   gaze.Project(side * 22.2f, y - lift * 0.6f, 11.0f),
                   gaze.Project(side * 25.4f, y - lift, 9.0f),
                   gaze.Project(side * (29.1f - row * 0.4f + 1.2f * cheekPress), y - lift, 7.0f)},
               whisker, 0.85f);
    }
}

void DrawUniFace(Graphics& g, const Gaze& gaze, const GraphicsPath& silhouette, float settle) {
    const auto state = g.Save();
    g.SetClip(&silhouette, CombineModeIntersect);
    const auto point = [&gaze](float x, float y) { return gaze.Project(x, y, 14.0f); };

    // The blaze bends toward the side of the nose, as in the reference photos.
    FillShape(g, Color(255, 208, 205, 192),
              {point(1.1f, -21), point(2.5f, -20), point(3.1f, -16), point(3.7f, -13),
               point(4.9f, -8), point(4.4f, -4), point(5.9f, -0.4f),
               point(6.3f, 1), point(6.1f, 3.3f), point(7.0f, 6),
               point(4.4f, 3.9f), point(4.1f, 0), point(3.8f, -2.8f),
               point(3.2f, -8), point(1.9f, -15), point(1.1f, -21)});

    FillShape(g, cream,
              {point(-17 - 2 * settle, 18 - 3 * settle), point(-14, 15 - 2 * settle), point(-11, 11), point(-6, 13),
               point(-4, 13.7f), point(-3.7f, 11.1f), point(0, 11),
               point(2.7f, 10.6f), point(4.3f, 5.8f), point(8.4f, 6.8f),
               point(13, 7.6f), point(12.8f, 12), point(17, 14),
               point(20, 15.5f), point(21, 22), point(26, 30),
               point(13, 38), point(-21, 34), point(-25, 26),
               point(-25, 22), point(-19.5f, 20), point(-17 - 2 * settle, 18 - 3 * settle)});
    FillShape(g, nose,
              {point(-3.6f, 8), point(-2.8f, 6.8f), point(3, 6.8f), point(3.9f, 8.2f),
               point(4.5f, 9), point(2.1f, 10.4f), point(0.5f, 12),
               point(-0.4f, 12.7f), point(-3.6f, 9.5f), point(-3.6f, 8)});
    g.Restore(state);
}

PointF EarPoint(float side, float x, float y, float twitch) {
    const float flick = std::max(0.0f, side * twitch);
    const float tip = std::clamp((-y - 22.0f) / 17.0f, 0.0f, 1.0f);
    return {x + side * 5.3f * flick * tip, y + 3.3f * flick * tip};
}

void DrawTuxedoFace(Graphics& g, const Gaze& gaze, const GraphicsPath& silhouette, float settle) {
    const auto state = g.Save();
    g.SetClip(&silhouette, CombineModeIntersect);
    const auto point = [&gaze](float x, float y) { return gaze.Project(x, y, 14.0f); };

    // The blaze widens toward the chin while keeping the sides of the face dark.
    FillShape(g, whiteFur,
              {point(0, -15.5f), point(1.6f, -14), point(2.3f, -8), point(3.0f, -3),
               point(3.7f, 1), point(5.6f, 4.9f), point(8.4f, 7.4f),
               point(11.5f, 10.2f), point(14.6f, 13.2f), point(15.4f, 17.3f),
               point(16.4f, 22.1f), point(17.3f, 27), point(12.4f, 31.4f),
               point(5, 35), point(-10, 34), point(-16, 30),
               point(-19, 27), point(-17.3f - 1.1f * settle, 22.4f), point(-15.4f - 1.3f * settle, 17.3f - 2.5f * settle),
               point(-14.6f, 13.2f), point(-11.5f, 10.2f), point(-8.4f, 7.4f),
               point(-5.6f, 4.9f), point(-3.7f, 1), point(-3.0f, -3),
               point(-2.3f, -8), point(-1.6f, -14), point(0, -15.5f)});
    g.Restore(state);
}

void DrawInnerEars(Graphics& g, const Pose& pose, Color earPink) {
    const auto ear = [&pose](float side, float x, float y) { return EarPoint(side, x, y, pose.earTwitch); };
    FillShape(g, earPink,
              {ear(-1, -22, -35), ear(-1, -20, -37), ear(-1, -17, -31), ear(-1, -12, -24),
               ear(-1, -16, -25), ear(-1, -20, -20), ear(-1, -22, -20),
               ear(-1, -22.8f, -24), ear(-1, -22.5f, -32), ear(-1, -22, -35)});
    FillShape(g, earPink,
              {ear(1, 29, -33), ear(1, 33, -34), ear(1, 30, -25), ear(1, 28.5f, -20),
               ear(1, 25, -21), ear(1, 22, -25), ear(1, 19, -23),
               ear(1, 22, -29), ear(1, 26, -31), ear(1, 29, -33)});
}

void DrawTabbyStripes(Graphics& g, const Gaze& gaze, const TabbyFur& marks, float crownExtension = 0) {
    const auto point = [&gaze](float x, float y) { return gaze.Project(x, y, 13.0f); };
    for (const float side : {-1.0f, 1.0f}) {
        const auto p = [side, &point](float x, float y) { return point(side * x, y); };
        // Extend the roots past the taller longhair crown; the face clip trims them as the gaze moves.
        const auto root = [&p, crownExtension](float x, float y) { return p(x, y - crownExtension); };
        FillShape(g, marks.stripe,
                  {root(0.8f, -26), p(2, -22), p(3.8f, -16), p(3.4f, -11.3f),
                   p(5.4f, -15), p(6.5f, -21), root(7.2f, -26),
                   root(5.1f, -28), root(2.7f, -27), root(0.8f, -26)});
        FillShape(g, marks.stripe,
                  {root(8.1f, -25.5f), p(9.7f, -22), p(10, -17.2f), p(9.2f, -12.5f),
                   p(12.4f, -15.5f), p(14.0f, -20), root(15.8f, -24),
                   root(14, -26.5f), root(11, -27.2f), root(8.1f, -25.5f)});
        FillShape(g, marks.stripe,
                  {p(38, -7), p(33, -7), p(29, -5), p(25, -2.4f),
                   p(29, -2.9f), p(34, -1), p(39, -1.3f),
                   p(40, -4), p(39, -5), p(38, -7)});
        FillShape(g, marks.stripe,
                  {p(38, 5), p(33.8f, 3.1f), p(29, 3.9f), p(24, 6.4f),
                   p(28, 6.6f), p(34, 9), p(39, 11),
                   p(40, 8), p(39, 6), p(38, 5)});
    }
}

void LonghairEarOutline(GraphicsPath& ear, float side, float twitch, Skin skin) {
    const auto p = [side, twitch](float x, float y) { return EarPoint(side, x * side, y, twitch); };
    if (skin == Skin::MaineCoon) {
        Outline(ear, {p(10, -20), p(14, -27), p(23, -35), p(26, -39),
                      p(27, -41), p(28, -43), p(28, -45),
                      p(29, -44), p(29, -42), p(29, -41),
                      p(31, -43), p(32, -46), p(32, -47),
                      p(33, -42), p(31, -39), p(31, -36),
                      p(32, -29), p(32, -21), p(29, -13),
                      p(23, -14), p(16, -18), p(10, -20)});
        return;
    }
    Outline(ear, {p(10, -20), p(14, -27), p(24, -39), p(28, -39),
                  p(32, -38), p(32, -24), p(29, -13),
                  p(23, -14), p(16, -18), p(10, -20)});
}

void DrawLonghairEar(Graphics& g, float side, const Pose& pose, const Palette& palette, Skin skin) {
    const auto p = [side, &pose](float x, float y) { return EarPoint(side, x * side, y, pose.earTwitch); };
    GraphicsPath ear;
    LonghairEarOutline(ear, side, pose.earTwitch, skin);
    SolidBrush fur(palette.tabby ? palette.tabby->stripe : ragdollPoints);
    g.FillPath(&fur, &ear);
    DrawFurOutline(g, ear, palette, skin);
    FillShape(g, palette.tabby ? palette.tabby->innerEar : Color(255, 212, 162, 155),
              {p(16, -22), p(20, -28), p(25, -34), p(27.4f, -35),
               p(29, -31), p(28.5f, -23), p(27, -19),
               p(25, -22), p(24, -24), p(22, -25),
               p(22, -23), p(22, -22), p(22, -21),
               p(20, -22), p(18, -22), p(16, -22)});
}

PointF LonghairCheek(float x, float y, float settle) {
    const float contact = std::max(y, 0.0f) * settle;
    return {x - contact * 0.08f, y - contact * 0.19f};
}

void LonghairFaceOutline(GraphicsPath& face, float settle, Skin skin) {
    const auto c = [settle](float x, float y) {
        return LonghairCheek(x, y, settle);
    };
    if (skin == Skin::Siberian) {
        // Full, rounded cheeks with just two short locks at the sides.
        Outline(face, {c(0, -28), c(18, -28), c(31, -18), c(35, -5),
                       c(36, -2), c(37, 1), c(39, 3),
                       c(37, 3), c(36, 3), c(35, 2),
                       c(37, 7), c(38, 11), c(38, 14),
                       c(36, 13), c(35, 13), c(33, 12),
                       c(30, 23), c(15, 28), c(0, 28),
                       c(-15, 28), c(-30, 23), c(-33, 12),
                       c(-35, 13), c(-36, 13), c(-38, 14),
                       c(-38, 11), c(-37, 7), c(-35, 2),
                       c(-36, 3), c(-37, 3), c(-39, 3),
                       c(-37, 1), c(-36, -2), c(-35, -5),
                       c(-31, -18), c(-18, -28), c(0, -28)});
        return;
    }
    Outline(face, {c(-33, -9), c(-29, -21), c(-15, -28), c(-2, -28),
                   c(14, -29), c(27, -22), c(32, -14),
                   c(35, -10), c(36, -7), c(39, -5),
                   c(38, -5), c(36, -5), c(35, -5),
                   c(38, -2), c(39, 4), c(42, 9),
                   c(40, 10), c(37, 9), c(35, 8),
                   c(38, 15), c(35, 21), c(32, 24),
                   c(32, 21), c(31, 19), c(30, 18),
                   c(26, 23), c(23, 25), c(16, 26),
                   c(7, 27), c(3, 25), c(0, 25),
                   c(-8, 27), c(-21, 25), c(-28, 19),
                   c(-29, 20), c(-30, 21), c(-30, 23),
                   c(-35, 21), c(-38, 16), c(-37, 10),
                   c(-38, 11), c(-40, 12), c(-42, 12),
                   c(-42, 6), c(-39, 0), c(-36, -4),
                   c(-37, -4), c(-38, -4), c(-39, -4),
                   c(-37, -5), c(-34, -7), c(-33, -9)});
}

void DrawRagdollFace(Graphics& g, const Gaze& gaze, const GraphicsPath& face, float settle) {
    // The cheek volume starts beside the eyes. Only the underside squashes on the ledge.
    const auto c = [settle](float x, float y) { return LonghairCheek(x, y, settle); };
    SolidBrush ivory(ragdollIvory);
    g.FillPath(&ivory, &face);
    for (float side : {-1.0f, 1.0f}) {
        const auto cheek = [side, &c](float x, float y) { return c(side * x, y); };
        FillShape(g, Color(255, 235, 218, 198),
                  {cheek(31, 0), cheek(34, 3), cheek(36, 7), cheek(36, 10),
                   cheek(35, 9), cheek(33, 8), cheek(32, 8),
                   cheek(34, 12), cheek(33, 15), cheek(32, 17),
                   cheek(32, 12), cheek(29, 11), cheek(27, 9),
                   cheek(31, 9), cheek(32, 6), cheek(31, 0)});
    }
    Pen outline(Color(255, 145, 121, 109), 1.1f);
    outline.SetLineJoin(LineJoinRound);

    // Anchor the larger point at the temple. Only its inner edge follows the eye as the head turns.
    const auto p = [&gaze](float x, float y) { return gaze.Project(x, y, 14); };
    const auto state = g.Save();
    g.SetClip(&face, CombineModeIntersect);
    const Color mink(255, 192, 166, 150);
    FillShape(g, mink,
              {c(19, -26), c(32, -29), c(43, -15), c(43, -4),
               c(39, -2), c(34, 3), p(31, 6),
               p(26, 4), p(22, 7), p(18, 9),
               p(12, 13), p(7, 10), p(5, 6),
               p(2, 0), p(3, -9), p(9, -13),
               p(14, -16), c(20, -20), c(19, -26)});
    FillShape(g, mink,
              {p(-30, 4), p(-29, -4), p(-25, -11), p(-18, -13),
               p(-11, -15), p(-5, -11), p(-4, -5),
               p(-2, 2), p(-6, 9), p(-12, 10),
               p(-19, 12), p(-22, 6), p(-29, 8),
               p(-28, 6), p(-27, 5), p(-26, 4),
               p(-27, 3), p(-29, 3), p(-30, 4)});
    g.Restore(state);
    g.DrawPath(&outline, &face);
}

void DrawMaineCoonFace(Graphics& g, const Gaze& gaze, const GraphicsPath& face) {
    SolidBrush fur(maineCoon.coat);
    g.FillPath(&fur, &face);
    const auto state = g.Save();
    g.SetClip(&face, CombineModeIntersect);
    DrawTabbyStripes(g, gaze, maineCoonFur, 10.0f);
    g.Restore(state);
    DrawFurOutline(g, face, maineCoon, Skin::MaineCoon);
}

void DrawSiberianFace(Graphics& g, const Gaze& gaze, const GraphicsPath& face) {
    SolidBrush fur(siberianWhite);
    g.FillPath(&fur, &face);
    const auto state = g.Save();
    g.SetClip(&face, CombineModeIntersect);
    const auto p = [&gaze](float x, float y) { return gaze.Project(x, y, 13); };
    // The silver cap meets the pale cheeks below the smoke-gray eye markings.
    FillShape(g, Color(255, 207, 205, 200),
              {p(-46, -40), p(-18, -41), p(18, -41), p(46, -40),
               p(47, -23), p(43, -8), p(39, 4),
               p(29, 0), p(21, 6), p(16, 9),
               p(9, 12), p(5, 8), p(0, 7),
               p(-6, 8), p(-9, 12), p(-16, 9),
               p(-24, 5), p(-30, 0), p(-39, 4),
               p(-43, -8), p(-47, -23), p(-46, -40)});
    for (float side : {-1.0f, 1.0f}) {
        const auto q = [side, &p](float x, float y) { return p(side * x, y); };
        const PointF crest = q(15, -9), outer = q(29, -4);
        const PointF curve1 = q(21, -9), curve2 = q(25, -5);
        FillShape(g, Color(255, 168, 165, 162),
                  {q(2, 4), q(4, -4), q(9, -9), crest,
                   curve1, curve2, outer,
                   q(28, -2), q(26, 0), q(24, 1),
                   q(26, 1), q(28, 1), q(29, 0),
                   q(27, 4), q(23, 7), q(19, 9),
                   q(12, 13), q(7, 11), q(2, 4)});
        FillShape(g, Color(255, 132, 129, 126),
                  {crest, curve1, curve2, outer, q(25, -3.7f), q(21, -7.8f), crest});
        const Color forehead(255, 164, 161, 159);
        FillShape(g, forehead,
                  {q(3, -36), q(3, -24), q(4, -15), q(2, -9),
                   q(6, -13), q(7, -23), q(8, -36)});
        FillShape(g, forehead,
                  {q(13, -35), q(12, -25), q(12, -19), q(10, -15),
                   q(15, -18), q(17, -25), q(18, -35)});
    }
    g.Restore(state);
    DrawFurOutline(g, face, siberian, Skin::Siberian);
}

void DrawChatterMouth(Graphics& g, const Gaze& gaze, const Pose& pose, Skin skin) {
    if (pose.chatter <= 0) return;

    const float jawPulse = pose.chatterAge < 0 ? 0.0f : 0.5f + 0.5f * std::sin(51.0f * pose.chatterAge);
    const float jaw = pose.chatter * (3.3f + 1.6f * jawPulse);
    const BYTE opacity = static_cast<BYTE>(255 * pose.chatter);
    const Color mouth = skin == Skin::Black ? Color(opacity, 123, 89, 101)
                        : skin == Skin::OrangeTabby ? Color(opacity, 92, 43, 37) : Color(opacity, 14, 12, 17);
    const auto point = [&gaze](float x, float y) { return gaze.Project(x, y, 16.0f); };
    FillShape(g, mouth,
              {point(-8, 15), point(-5, 14.2f), point(5, 14.2f), point(8, 15),
               point(7, 15 + jaw), point(3, 15.5f + jaw), point(0, 15.5f + jaw),
               point(-3, 15.5f + jaw), point(-7, 15 + jaw), point(-8, 15)});

    SolidBrush enamel(Color(opacity, 255, 246, 221));
    for (const float side : {-1.0f, 1.0f}) {
        const PointF fang[] = {point(2.9f * side, 14.8f), point(6.2f * side, 14.6f),
                               point(4.8f * side, 14.8f + 3.7f * pose.chatter)};
        g.FillPolygon(&enamel, fang, 3);
    }
}

void DrawChatterMarks(Graphics& g, const Pose& pose) {
    float left = 45.0f;
    for (std::size_t i = 0; i < pose.chatterCode.size(); ++i) {
        const bool dash = pose.chatterCode[i] == '-';
        const float markWidth = dash ? 12.9f : 4.3f;
        const float center = left + markWidth * 0.5f;
        left += markWidth + 3.4f;
        const float phase = (pose.chatterAge - static_cast<float>(i) * chatterMarkStagger) / chatterMarkLifetime;
        if (phase <= 0 || phase >= 1) continue;

        const float opacity = pose.chatter * std::sin(3.14159265f * phase);
        const float x = center + 4.8f * phase;
        const float y = -3.0f - 7.0f * phase;
        const Color edge(static_cast<BYTE>(160 * opacity), 63, 49, 57);
        const Color ink(static_cast<BYTE>(245 * opacity), 255, 226, 177);
        if (dash) {
            Stroke(g, {{x - 4.85f, y}, {x - 1.6f, y}, {x + 1.6f, y}, {x + 4.85f, y}}, edge, 3.2f);
            Stroke(g, {{x - 4.85f, y}, {x - 1.6f, y}, {x + 1.6f, y}, {x + 4.85f, y}}, ink, 1.75f);
        } else {
            SolidBrush rim(edge);
            SolidBrush fill(ink);
            g.FillEllipse(&rim, x - 2.15f, y - 2.15f, 4.3f, 4.3f);
            g.FillEllipse(&fill, x - 1.42f, y - 1.42f, 2.84f, 2.84f);
        }
    }
}

void ShorthairHeadOutline(GraphicsPath& silhouette, const Pose& pose, float settle) {
    const float cheekEdge = -31.0f - 1.5f * settle;
    // The two lowest curves flatten and widen the cheek as the head settles onto the ledge.
    const auto ear = [&pose](float side, float x, float y) {
        return EarPoint(side, x, y, pose.earTwitch);
    };
    const PointF crown[]{{cheekEdge, 4}, {-32, -5}, {-28, -12}, {-24, -17},
                         ear(-1, -25, -24), ear(-1, -24, -34), ear(-1, -22, -39),
                         ear(-1, -21, -42), ear(-1, -18, -41), ear(-1, -16, -38),
                         ear(-1, -12, -33), ear(-1, -8.5f, -26), {-5.5f, -22},
                         {0, -23.5f}, {7, -23.8f}, {12, -22},
                         ear(1, 16, -28), ear(1, 23, -35), ear(1, 28, -37),
                         ear(1, 31, -38.4f), ear(1, 33, -36.4f), ear(1, 33, -33),
                         ear(1, 33, -26), {32, -20}, {30, -15}, {35, -10}, {38, -3}, {38, 5}};
    silhouette.AddBeziers(crown, static_cast<INT>(std::size(crown)));
    const PointF jaw[]{{38, 5}, {38.5f, 14}, {33, 21.5f}, {25, 24},
                       {10, 28 - 0.5f * settle}, {-9 - 5 * settle, 27 - 8.3f * settle},
                       {-20 - 3 * settle, 22 - 6 * settle}, {-27 - 4 * settle, 19 - 5.5f * settle},
                       {-31 - 6 * settle, 13 - 4 * settle}, {cheekEdge, 4}};
    silhouette.AddBeziers(jaw, static_cast<INT>(std::size(jaw)));
    silhouette.CloseFigure();
}

void DrawShorthairFace(Graphics& g, const Pose& pose, const Gaze& gaze, const Palette& palette, Skin skin,
                       float settle) {
    GraphicsPath silhouette;
    ShorthairHeadOutline(silhouette, pose, settle);
    SolidBrush fur(palette.coat);
    g.FillPath(&fur, &silhouette);
    if (skin == Skin::Uni) DrawUniFace(g, gaze, silhouette, settle);
    if (palette.tabby) {
        const auto state = g.Save();
        g.SetClip(&silhouette, CombineModeIntersect);
        DrawInnerEars(g, pose, palette.tabby->innerEar);
        DrawTabbyStripes(g, gaze, *palette.tabby);
        g.Restore(state);
    }
    if (skin == Skin::Tuxedo) DrawTuxedoFace(g, gaze, silhouette, settle);
    if (skin != Skin::Black) DrawFurOutline(g, silhouette, palette, skin);
}

void DrawHead(Graphics& g, const Pose& pose, const Palette& palette, Skin skin, float lift = 0) {
    const auto head = PositionHead(pose, lift);
    const auto& gaze = head.gaze;
    const float settle = head.settle;
    const auto state = g.Save();
    g.TranslateTransform(head.center.X, head.center.Y);
    g.RotateTransform(head.angle);
    const bool longhair = IsLonghair(skin);
    if (longhair) {
        for (float side : {-1.0f, 1.0f}) DrawLonghairEar(g, side, pose, palette, skin);
        GraphicsPath face;
        LonghairFaceOutline(face, settle, skin);
        if (skin == Skin::MaineCoon) DrawMaineCoonFace(g, gaze, face);
        else if (skin == Skin::Siberian) DrawSiberianFace(g, gaze, face);
        else DrawRagdollFace(g, gaze, face, settle);
    } else {
        DrawShorthairFace(g, pose, gaze, palette, skin, settle);
    }

    DrawChatterMouth(g, gaze, pose, skin);
    for (const float side : {-1.0f, 1.0f}) {
        DrawEye(g, side, gaze, pose, palette, skin);
        if (longhair) {
            for (float row : {0.0f, 1.0f}) {
                const auto w = [side, &gaze](float x, float y) { return gaze.Project(side * x, y, 10); };
                Stroke(g, {w(15 + row, 9 + 3 * row), w(20, 8 + 3 * row), w(26, 7 + 4 * row),
                           w(32 - row, 8 + 5 * row)}, palette.whisker, 0.65f);
            }
        } else {
            DrawWhiskers(g, side, gaze, side < 0 ? settle : 0.0f, palette.whisker);
        }
    }
    DrawChatterMarks(g, pose);
    g.Restore(state);
}

void DrawUniBody(Graphics& g, const GraphicsPath& silhouette, float brace) {
    const auto state = g.Save();
    g.SetClip(&silhouette, CombineModeIntersect);
    // Extend the belly color past the underside, clipping it to the same body contour.
    FillShape(g, shadowedCream,
              {{53, 65 - 21 * brace}, {70, 62 - 19 * brace}, {86, 64 - 16 * brace}, {99, 68 - 13 * brace},
               {109, 71 - 10 * brace}, {120, 70 - 7 * brace}, {128, 74},
               {139, 80}, {146, 92}, {147, 103},
               {148, 111}, {145, 117}, {141, 122},
               {137, 125}, {131, 117}, {128, 106},
               {126, 96}, {124, 88 + 3 * brace}, {115, 83 + 7 * brace},
               {103, 78 + 10 * brace}, {89, 76 + 12 * brace}, {75, 77 + 10 * brace},
               {63, 79 + 7 * brace}, {50, 78 + 2 * brace}, {53, 65 - 21 * brace}});
    g.Restore(state);
}

void DrawBib(Graphics& g, const GraphicsPath& silhouette, float brace) {
    const auto state = g.Save();
    g.SetClip(&silhouette, CombineModeIntersect);
    FillShape(g, whiteFur,
              {{50, 56 - 21 * brace}, {60, 51 - 19 * brace}, {83, 54 - 16 * brace}, {92, 58 - 13 * brace},
               {98, 63 - 10 * brace}, {97, 72 - 4 * brace}, {106, 77 + 2 * brace},
               {109, 80 + 5 * brace}, {114, 84 + 6 * brace}, {114, 88 + 6 * brace},
               {102, 96 + 5 * brace}, {74, 94 + 3 * brace}, {63, 89},
               {49, 86}, {43, 62 - 15 * brace}, {50, 56 - 21 * brace}});
    g.Restore(state);
}

void DrawShadedCoat(Graphics& g, const GraphicsPath& silhouette, float brace, const Palette& palette, Color shade) {
    SolidBrush coat(palette.coat);
    g.FillPath(&coat, &silhouette);
    const auto state = g.Save();
    g.SetClip(&silhouette, CombineModeIntersect);
    FillShape(g, shade,
              {{76, 29 - 20 * brace}, {103, 20 - 12 * brace}, {149, 25}, {171, 46},
               {180, 64}, {175, 92}, {163, 105},
               {162, 91}, {158, 81}, {151, 75},
               {153, 80}, {154, 85}, {153, 89},
               {147, 75}, {137, 64}, {123, 60},
               {110, 56}, {99, 54 - 7 * brace}, {87, 49 - 13 * brace},
               {83, 44 - 15 * brace}, {79, 38 - 18 * brace}, {76, 29 - 20 * brace}});
    g.Restore(state);
}

PointF LonghairRuffPoint(float x, float y, float brace) {
    const float lift = std::clamp((100.0f - y) / 54.0f, 0.0f, 1.0f);
    return {x, y - 22.0f * brace * lift};
}

void LonghairRuffOutline(GraphicsPath& bib, float brace) {
    // The collar is rooted at the moving shoulder and falls between the two planted paws.
    const auto p = [brace](float x, float y) { return LonghairRuffPoint(x, y, brace); };
    Outline(bib, {p(28, 62), p(31, 49), p(61, 35), p(80, 42),
                  p(94, 44), p(98, 55), p(106, 62),
                  p(104, 62), p(102, 62), p(101, 62),
                  p(106, 68), p(108, 74), p(107, 80),
                  p(106, 78), p(104, 76), p(102, 76),
                  p(101, 84), p(94, 90), p(87, 92),
                  p(88, 90), p(88, 88), p(87, 87),
                  p(86, 91), p(82, 94), p(78, 96),
                  p(77, 92), p(74, 88), p(71, 86),
                  p(65, 85), p(60, 84), p(56, 81),
                  p(57, 81), p(58, 80), p(58, 80),
                  p(42, 83), p(30, 74), p(28, 62)});
}

void DrawLonghairRuff(Graphics& g, float brace, Skin skin) {
    const auto p = [brace](float x, float y) { return LonghairRuffPoint(x, y, brace); };
    GraphicsPath bib;
    LonghairRuffOutline(bib, brace);
    const auto* marks = PaletteFor(skin).tabby;
    SolidBrush fur(skin == Skin::Siberian ? siberianWhite : marks ? maineCoonRuff : ragdollIvory);
    g.FillPath(&fur, &bib);
    Pen hem(skin == Skin::Siberian ? siberianFur.edge : Color(185, 132, 108, 96), 0.9f);
    hem.SetLineJoin(LineJoinRound);
    g.DrawPath(&hem, &bib);
    const auto state = g.Save();
    g.SetClip(&bib, CombineModeIntersect);
    FillShape(g, marks ? marks->belly : Color(255, 234, 219, 202),
              {p(62, 72), p(63, 78), p(69, 84), p(75, 87),
               p(73, 84), p(73, 83), p(73, 81),
               p(72, 82), p(71, 82), p(70, 82),
               p(66, 79), p(64, 75), p(62, 72)});
    FillShape(g, marks ? marks->belly : Color(255, 230, 212, 193),
              {p(98, 67), p(100, 73), p(97, 81), p(92, 84),
               p(94, 81), p(94, 80), p(94, 78),
               p(92, 81), p(89, 84), p(86, 85),
               p(93, 78), p(96, 75), p(98, 67)});
    g.Restore(state);
}

void DrawTabbyBody(Graphics& g, const GraphicsPath& silhouette, float brace, const TabbyFur& marks) {
    const auto state = g.Save();
    g.SetClip(&silhouette, CombineModeIntersect);
    FillShape(g, marks.belly,
              {{51, 64 - 15 * brace}, {62, 59 - 12 * brace}, {74, 67 - 6 * brace}, {85, 69 - 4 * brace},
               {99, 73 - 2 * brace}, {109, 73 + 3 * brace}, {119, 80 + 4 * brace},
               {131, 88 + 2 * brace}, {141, 107}, {141, 117},
               {130, 112}, {101, 95}, {78, 95},
               {60, 95}, {42, 75 - 10 * brace}, {51, 64 - 15 * brace}});

    const auto p = [brace](float x, float y) {
        const float forward = std::clamp((129.0f - x) / 59.0f, 0.0f, 1.0f);
        const float upper = std::clamp((79.0f - y) / 40.0f, 0.0f, 1.0f);
        return PointF{x, y - 23.0f * brace * forward * upper};
    };
    FillShape(g, marks.stripe,
              {p(82, 35), p(83, 45), p(90, 53), p(99, 60),
               p(93, 60), p(83, 54), p(77, 44),
               p(74, 40), p(75, 36), p(82, 35)});
    FillShape(g, marks.stripe,
              {p(101, 36), p(104, 47), p(113, 57), p(122, 66),
               p(113, 65), p(103, 60), p(97, 53),
               p(92, 47), p(94, 39), p(101, 36)});
    FillShape(g, marks.stripe,
              {p(124, 38), p(124, 51), p(130, 65), p(139, 77),
               p(132, 75), p(124, 70), p(119, 61),
               p(114, 52), p(115, 41), p(124, 38)});
    FillShape(g, marks.stripe,
              {{151, 46}, {148, 61}, {150, 74}, {155, 84},
               {148, 82}, {142, 75}, {140, 66},
               {137, 55}, {140, 46}, {151, 46}});
    FillShape(g, marks.stripe,
              {{172, 80}, {168, 88}, {156, 91}, {145, 93},
               {151, 96}, {162, 97}, {170, 93},
               {175, 90}, {175, 85}, {172, 80}});
    FillShape(g, marks.stripe,
              {{171, 104}, {165, 109}, {154, 113}, {144, 112},
               {150, 117}, {161, 119}, {168, 115},
               {173, 111}, {174, 108}, {171, 104}});
    g.Restore(state);
}

void BodyOutline(GraphicsPath& silhouette, float brace, Skin skin) {
    if (IsLonghair(skin)) {
        Outline(silhouette, {{56, 60 - 19 * brace}, {66, 36 - 20 * brace}, {103, 33 - 9 * brace}, {125, 39},
                             {143, 43}, {155, 54}, {161, 68},
                             {160, 68}, {159, 68}, {158, 68},
                             {164, 77}, {167, 88}, {165, 100},
                             {164, 98}, {163, 97}, {162, 96},
                             {162, 106}, {158, 113}, {151, 117},
                             {151, 115}, {150, 114}, {149, 113},
                             {145, 114}, {139, 110}, {136, 104},
                             {131, 92}, {127, 83}, {113, 84 + 3 * brace},
                             {95, 86 + 2 * brace}, {72, 91}, {58, 78 - 5 * brace},
                             {54, 74 - 11 * brace}, {53, 65 - 15 * brace}, {56, 60 - 19 * brace}});
        return;
    }
    const PointF shoulder[]{{62, 52 - 22 * brace}, {80, 42 - 22 * brace}, {106, 43 - 12 * brace}, {127, 46}};
    silhouette.AddBeziers(shoulder, static_cast<INT>(std::size(shoulder)));
    const PointF back[]{{127, 46}, {166.5f, 51.64f}, {171, 107}, {161.5f, 127},
                        {158.65f, 133}, {149, 133}, {143, 127},
                        {138, 122}, {139, 111}, {137, 101},
                        {135, 91}, {129, 84}, {117, 81 + 4 * brace}};
    silhouette.AddBeziers(back, static_cast<INT>(std::size(back)));
    const PointF belly[]{{117, 81 + 4 * brace}, {105, 78 + 8 * brace}, {93, 80 + 5 * brace}, {82, 79 + 3 * brace},
                         {71, 78 + brace}, {59, 79 - 5 * brace}, {53, 76 - 12 * brace}};
    silhouette.AddBeziers(belly, static_cast<INT>(std::size(belly)));
    const PointF neck[]{{53, 76 - 12 * brace}, {47, 73 - 19 * brace}, {49, 66 - 20 * brace}, {53, 60 - 22 * brace},
                        {55, 56 - 22 * brace}, {58, 54 - 22 * brace}, {62, 52 - 22 * brace}};
    silhouette.AddBeziers(neck, static_cast<INT>(std::size(neck)));
    silhouette.CloseFigure();
}

void DrawBody(Graphics& g, const GraphicsPath& silhouette, const GraphicsPath& farPaw,
              const Palette& palette, Skin skin, float brace) {
    if (skin == Skin::Ragdoll || skin == Skin::Siberian) {
        const Color shade = skin == Skin::Siberian ? Color(255, 209, 207, 204) : ragdollBack;
        DrawShadedCoat(g, silhouette, brace, palette, shade);
    } else {
        SolidBrush fur(palette.coat);
        g.FillPath(&fur, &silhouette);
        if (skin == Skin::Uni) DrawUniBody(g, silhouette, brace);
        if (palette.tabby) DrawTabbyBody(g, silhouette, brace, *palette.tabby);
        if (skin == Skin::Tuxedo) DrawBib(g, silhouette, brace);
    }
    if (skin != Skin::Black) {
        const auto state = g.Save();
        g.SetClip(&farPaw, CombineModeExclude);
        DrawFurOutline(g, silhouette, palette, skin);
        g.Restore(state);
    }
}

struct PawPress {
    PawPress(PointF contact, float direction, float press)
        : contact(contact), direction(direction), width(1.0f + 0.14f * press), depth(1.0f - 0.28f * press),
          spread(direction * 4.8f * press), sine(std::sin(-direction * 0.44f * press)),
          cosine(std::cos(0.44f * press)) {}

    PointF operator()(float x, float y) const {
        x = (x - contact.X) * width;
        y = (y - contact.Y) * depth;
        return {contact.X + x * cosine - y * sine + spread, contact.Y + x * sine + y * cosine};
    }

    PointF contact;
    float direction;
    float width;
    float depth;
    float spread;
    float sine;
    float cosine;
};

// Shrinks a paw toward the belly line, narrowing it as it retracts.
void TuckMatrix(Matrix& matrix, float centerX, float tuck) {
    matrix.Translate(centerX, 74.0f);
    matrix.Scale(1.0f - 0.35f * tuck, 1.0f - 0.95f * tuck);
    matrix.Translate(-centerX, -74.0f);
}

void DrawToeCreases(Graphics& g, const PawPress& foot, Color crease) {
    const auto toe = [&foot](float x, float y) { return foot(foot.contact.X + foot.direction * x, foot.contact.Y + y); };
    Stroke(g, {toe(-4.3f, -8), toe(-4.3f, -5.8f), toe(-3.5f, -3.5f), toe(-2.4f, -2.1f)}, crease, 0.9f);
    Stroke(g, {toe(1.6f, -8.5f), toe(1.6f, -6.1f), toe(2.3f, -3.9f), toe(3.6f, -2.4f)}, crease, 0.9f);
}

void DrawTabbyPaw(Graphics& g, const GraphicsPath& paw, const PawPress& foot, const TabbyFur& marks) {
    const auto state = g.Save();
    g.SetClip(&paw, CombineModeIntersect);
    const auto p = [&foot](float x, float y) { return foot(foot.contact.X + foot.direction * x, foot.contact.Y + y); };
    FillShape(g, marks.pawTip,
              {p(-16, -4), p(-8, -5), p(5, -7), p(15, -4),
               p(17, 2), p(8, 6), p(0, 7),
               p(-9, 7), p(-19, 2), p(-16, -4)});
    for (const float stripeY : {-19.0f, -11.0f}) {
        FillShape(g, marks.stripe,
                  {p(-16, stripeY - 2), p(-5, stripeY + 1), p(7, stripeY - 1), p(15, stripeY - 5),
                   p(18, stripeY - 4), p(17, stripeY), p(14, stripeY + 1),
                   p(2, stripeY + 4), p(-8, stripeY + 5), p(-16, stripeY + 2),
                   p(-18, stripeY + 1), p(-18, stripeY), p(-16, stripeY - 2)});
    }
    g.Restore(state);
}

void FarFrontPawOutline(GraphicsPath& paw, float lift, const PawPress& foot) {
    Outline(paw, {{58, 65 - 17 * lift}, {65, 59 - 18 * lift}, {77, 62 - 14 * lift}, {80, 70 - 13 * lift},
                  {82, 74 - 7 * lift}, {83, 77 - 3 * lift}, foot(82, 79),
                  foot(81, 85), foot(76, 90), foot(70, 90),
                  foot(63, 90), foot(59, 85), foot(59, 79),
                  {59, 75 - 5 * lift}, {55, 70 - 11 * lift}, {58, 65 - 17 * lift}});
}

void DrawFarFrontPaw(Graphics& g, GraphicsPath& paw, const Palette& palette, Skin skin, float lift,
                     const PawPress& foot, float tuck) {
    if (tuck >= 0.999f) return;
    FarFrontPawOutline(paw, lift, foot);
    const auto paint = [&] {
        SolidBrush fur(palette.coat);
        g.FillPath(&fur, &paw);
        if (HasWhitePaws(skin)) {
            const auto state = g.Save();
            g.SetClip(&paw, CombineModeIntersect);
            FillShape(g, skin == Skin::Uni ? cream : whiteFur,
                      {foot(57, 82), foot(64, 80), foot(75, 81), foot(83, 84),
                       foot(84, 89), foot(78, 96), foot(67, 95),
                       foot(59, 94), foot(54, 90), foot(57, 82)});
            g.Restore(state);
        }
        if (palette.tabby) DrawTabbyPaw(g, paw, foot, *palette.tabby);
        if (skin != Skin::Black) DrawFurOutline(g, paw, palette, skin);
        DrawToeCreases(g, foot, palette.crease);
    };
    if (tuck <= 0) {
        paint();
        return;
    }
    // The paw retracts into the belly; the body hides whatever has already gone inside.
    const auto state = g.Save();
    Matrix fold;
    TuckMatrix(fold, 70.0f, tuck);
    g.MultiplyTransform(&fold);
    paint();
    g.Restore(state);
    paw.Transform(&fold);
}

void NearFrontPawOutline(GraphicsPath& paw, float lift, const PawPress& foot) {
    Outline(paw, {{99, 65 - 19 * lift}, {108, 61 - 17 * lift}, {119 - 4 * lift, 65 - 12 * lift},
                  {121 - 4 * lift, 75 - 9 * lift}, {122 - 3 * lift, 82 - 5 * lift}, foot(115, 92), foot(108, 93),
                  foot(101, 95), foot(95, 90), foot(95, 84),
                  {93 + 2 * lift, 80 - 4 * lift}, {91 + 4 * lift, 73 - 10 * lift}, {94 + 2 * lift, 69 - 15 * lift},
                  {95 + 2 * lift, 67 - 17 * lift}, {97, 66 - 18 * lift}, {99, 65 - 19 * lift}});
}

void DrawNearFrontPaw(Graphics& g, const GraphicsPath& body, const Palette& palette, Skin skin,
                      float lift, float press, float tuck) {
    if (tuck >= 0.999f) return;
    GraphicsPath paw;
    const PawPress foot({107, 93}, 1.0f, press);
    NearFrontPawOutline(paw, lift, foot);
    const auto paint = [&](const GraphicsPath& bodyHere) {
        if (skin == Skin::Ragdoll) {
            DrawShadedCoat(g, paw, lift, palette, ragdollBack);
        } else {
            SolidBrush fur(palette.coat);
            g.FillPath(&fur, &paw);
        }
        if (HasWhitePaws(skin)) {
            const auto state = g.Save();
            g.SetClip(&paw, CombineModeIntersect);
            FillShape(g, skin == Skin::Uni ? cream : whiteFur,
                      {foot(93, 85), foot(101, 82), foot(111, 84), foot(119, 81),
                       foot(122, 90), foot(115, 97), foot(105, 98),
                       foot(98, 98), foot(91, 93), foot(93, 85)});
            g.Restore(state);
        }
        if (palette.tabby) DrawTabbyPaw(g, paw, foot, *palette.tabby);
        if (skin != Skin::Black) {
            const auto edge = g.Save();
            g.SetClip(&bodyHere, CombineModeExclude);
            DrawFurOutline(g, paw, palette, skin);
            g.Restore(edge);
        }
        DrawToeCreases(g, foot, palette.crease);
    };
    if (tuck <= 0) {
        paint(body);
        return;
    }
    // The paw retracts into the belly; the body hides whatever has already gone inside.
    const auto state = g.Save();
    g.SetClip(&body, CombineModeExclude);
    Matrix fold;
    TuckMatrix(fold, 107.0f, tuck);
    g.MultiplyTransform(&fold);
    fold.Invert();
    std::unique_ptr<GraphicsPath> bodyInPawSpace(body.Clone());
    bodyInPawSpace->Transform(&fold);
    paint(*bodyInPawSpace);
    g.Restore(state);
}

void DrawDreams(Graphics& g, const Pose& pose) {
    for (int i = 0; i < pose.dreamCount; ++i) {
        const float phase = (pose.dreamAge - i * dreamStagger) / dreamLifetime;
        if (phase <= 0 || phase >= 1) continue;

        const float opacity = std::sin(3.14159265f * phase);
        const float x = 98.0f + 14.0f * i + 12.0f * phase;
        const float y = 38.0f - 7.0f * i - 18.0f * phase;
        const float size = 6.7f + 1.6f * i + 1.7f * phase;
        const PointF letter[] = {{x, y}, {x + size, y}, {x, y + size}, {x + size, y + size}};
        Pen rim(Color(static_cast<BYTE>(96 * opacity), 46, 53, 52), 2.5f);
        rim.SetLineJoin(LineJoinRound);
        g.DrawLines(&rim, letter, 4);
        Pen ink(Color(static_cast<BYTE>(163 * opacity), 235, 241, 227), 1.35f);
        ink.SetLineJoin(LineJoinRound);
        g.DrawLines(&ink, letter, 4);
    }
}

void DrawPetSymbols(Graphics& g, const Pose& pose) {
    for (int i = 0; i < pose.petSymbolCount; ++i) {
        const float phase = (pose.petSymbolAge - i * petSymbolStagger) / petSymbolLifetime;
        if (phase <= 0 || phase >= 1) continue;

        const float opacity = std::min(1.0f, phase * 7.0f) * std::min(1.0f, (1.0f - phase) * 2.6f);
        const float x = 102.0f + 13.0f * i + 9.0f * phase;
        const float y = 32.0f - 7.0f * i - 19.0f * phase;
        const float size = (i == 0 ? 5.2f : 4.2f) * (0.76f + 0.24f * std::min(1.0f, phase * 6.0f));
        const auto p = [x, y, size](float dx, float dy) { return PointF{x + size * dx, y + size * dy}; };
        GraphicsPath symbol;
        if (pose.petIsNote) {
            // One continuous eighth-note silhouette stays legible at desktop scale.
            Outline(symbol, {p(0, -1.25f), p(0.18f, -1.05f), p(0.99f, -0.95f), p(0.81f, -0.18f),
                             p(0.72f, -0.43f), p(0.47f, -0.5f), p(0.25f, -0.54f),
                             p(0.25f, -0.2f), p(0.25f, 0.26f), p(0.25f, 0.56f),
                             p(0.25f, 1.08f), p(-0.71f, 1.32f), p(-0.84f, 0.85f),
                             p(-1.0f, 0.42f), p(-0.4f, 0.09f), p(0, 0.25f),
                             p(0, 0), p(0, -0.87f), p(0, -1.25f)});
        } else {
            Outline(symbol, {p(0, -0.29f), p(-0.46f, -1.09f), p(-1.23f, -0.67f), p(-0.96f, -0.04f),
                             p(-0.77f, 0.43f), p(-0.27f, 0.70f), p(0, 0.99f),
                             p(0.27f, 0.70f), p(0.77f, 0.43f), p(0.96f, -0.04f),
                             p(1.23f, -0.67f), p(0.46f, -1.09f), p(0, -0.29f)});
        }
        const BYTE fillAlpha = static_cast<BYTE>(246 * opacity);
        const BYTE edgeAlpha = static_cast<BYTE>(220 * opacity);
        SolidBrush fill(pose.petIsNote ? Color(fillAlpha, 43, 43, 46) : Color(fillAlpha, 255, 151, 184));
        Pen edge(pose.petIsNote ? Color(edgeAlpha, 115, 115, 119) : Color(edgeAlpha, 185, 82, 119), 0.85f);
        edge.SetLineJoin(LineJoinRound);
        g.FillPath(&fill, &symbol);
        g.DrawPath(&edge, &symbol);
    }
}

} // namespace

bool HitsPettingArea(PointF cursor, const Pose& pose, Skin skin, float headroom) {
    if (cursor.X < 0 || cursor.X > foldedRight + 4 || cursor.Y < -headroom || cursor.Y > 134) return false;

    const float brace = UpperBodyBrace(pose, headroom);
    const auto head = PositionHead(pose, brace * fullHeadLift);
    const float angle = head.angle * (3.14159265f / 180.0f);
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const float dx = cursor.X - head.center.X, dy = cursor.Y - head.center.Y;
    const PointF local{cosine * dx + sine * dy, -sine * dx + cosine * dy};
    GraphicsPath shape;
    if (std::abs(local.X) <= 44 && local.Y >= -48 && local.Y <= 29) {
        if (IsLonghair(skin)) {
            LonghairFaceOutline(shape, head.settle, skin);
            if (shape.IsVisible(local)) return true;
            for (float side : {-1.0f, 1.0f}) {
                shape.Reset();
                LonghairEarOutline(shape, side, pose.earTwitch, skin);
                if (shape.IsVisible(local)) return true;
            }
        } else {
            ShorthairHeadOutline(shape, pose, head.settle);
            if (shape.IsVisible(local)) return true;
        }
    }
    if (cursor.X < 28 || cursor.Y < 10) return false;

    shape.Reset();
    BodyOutline(shape, brace, skin);
    if (shape.IsVisible(cursor)) return true;
    if (cursor.Y > 98) return false;

    shape.Reset();
    if (pose.pawTuckLeft < 0.5f) {
        FarFrontPawOutline(shape, brace, PawPress({70, 90}, -1.0f, pose.brace));
        if (shape.IsVisible(cursor)) return true;
        shape.Reset();
    }
    if (pose.pawTuckRight < 0.5f) {
        NearFrontPawOutline(shape, brace, PawPress({107, 93}, 1.0f, pose.brace));
        if (shape.IsVisible(cursor)) return true;
    }
    if (!IsLonghair(skin)) return false;

    shape.Reset();
    LonghairRuffOutline(shape, brace);
    return shape.IsVisible(cursor);
}

void Draw(Graphics& g, const Pose& pose, Skin skin, float headroom) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    const auto& palette = PaletteFor(skin);
    const float upperBodyBrace = UpperBodyBrace(pose, headroom);
    GraphicsPath body, farPaw;
    BodyOutline(body, upperBodyBrace, skin);
    DrawTail(g, pose, palette, skin);
    const PawPress farFoot({70, 90}, -1.0f, pose.brace);
    DrawFarFrontPaw(g, farPaw, palette, skin, upperBodyBrace, farFoot, pose.pawTuckLeft);
    DrawBody(g, body, farPaw, palette, skin, upperBodyBrace);
    DrawNearFrontPaw(g, body, palette, skin, upperBodyBrace, pose.brace, pose.pawTuckRight);
    if (IsLonghair(skin)) DrawLonghairRuff(g, upperBodyBrace, skin);
    DrawHead(g, pose, palette, skin, upperBodyBrace * fullHeadLift);
    DrawDreams(g, pose);
    DrawPetSymbols(g, pose);
}

void DrawTrayIcon(Graphics& g, Skin skin) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    const bool longhair = IsLonghair(skin);
    const float scale = longhair ? 0.33f : 0.36f;
    g.ScaleTransform(scale, scale);
    g.TranslateTransform(longhair ? -11.0f : -15.5f, -2.0f);
    const auto& palette = PaletteFor(skin);
    DrawHead(g, {faceCenter, 0, 0, 1, 1, 1, 1, 0, 0}, palette, skin);
}

} // namespace cat
