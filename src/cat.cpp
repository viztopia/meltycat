#include "cat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>

namespace cat {
namespace {

using namespace Gdiplus;

const PointF faceCenter{56.0f, 53.0f};
constexpr float fullHeadLift = 20.0f;

float UpperBodyBrace(const Pose& pose, float headroom) {
    // The resting ear silhouette already leaves a few pixels above it.
    return pose.brace * std::min(1.0f, (headroom + 6.0f) / fullHeadLift);
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

void Fill(Graphics& g, const GraphicsPath& path, Color color) {
    SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

void DrawOutline(Graphics& g, const GraphicsPath& path, std::optional<Color> ink, float thickness) {
    if (!ink) return;
    Pen edge(*ink, thickness);
    edge.SetLineJoin(LineJoinRound);
    g.DrawPath(&edge, &path);
}

void DrawFurOutline(Graphics& g, const GraphicsPath& path, const SkinSpec& skin) {
    DrawOutline(g, path, skin.outline, skin.longhair ? 1.0f : 0.8f);
}

PointF Lerp(PointF a, PointF b, float t) {
    return {a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
}

template <typename Transform>
void DrawMark(Graphics& g, const Mark& mark, bool mirrored, float settle, float brace,
              const Transform& transform) {
    GraphicsPath path;
    PointF current, start;
    for (std::size_t i = 0; i < mark.path.size(); ++i) {
        const auto& command = mark.path[i];
        const auto point = [&](std::size_t j) {
            const auto base = command.points[j];
            auto p = base;
            if (!mark.asleep.empty()) {
                p.X += (mark.asleep[i].points[j].X - base.X) * settle;
                p.Y += (mark.asleep[i].points[j].Y - base.Y) * settle;
            }
            if (!mark.braced.empty()) {
                p.X += (mark.braced[i].points[j].X - base.X) * brace;
                p.Y += (mark.braced[i].points[j].Y - base.Y) * brace;
            }
            if (mirrored) p.X = -p.X;
            return transform(mark, mirrored, p);
        };
        switch (command.kind) {
        case PathKind::Move:
            path.StartFigure();
            current = start = point(0);
            break;
        case PathKind::Line: {
            const auto end = point(0);
            path.AddLine(current, end);
            current = end;
            break;
        }
        case PathKind::Quad: {
            const auto control = point(0), end = point(1);
            path.AddBezier(current, Lerp(current, control, 2.0f / 3), Lerp(end, control, 2.0f / 3), end);
            current = end;
            break;
        }
        case PathKind::Cubic: {
            const auto end = point(2);
            path.AddBezier(current, point(0), point(1), end);
            current = end;
            break;
        }
        case PathKind::Close:
            path.CloseFigure();
            current = start;
            break;
        }
    }
    Fill(g, path, mark.color);
}

float EarSide(const Mark& mark, bool mirrored) {
    return mirrored || (!mark.mirror && mark.left) ? -1.0f : 1.0f;
}

template <typename Transform>
void DrawMarks(Graphics& g, const GraphicsPath& clip, const SkinSpec& skin, Part part,
               float settle, float brace, const Transform& transform, float earSide = 0) {
    if (skin.marks.empty()) return;
    const auto state = g.Save();
    g.SetClip(&clip, CombineModeIntersect);
    for (const auto& mark : skin.marks) {
        const bool paw = (part == Part::FarPaw || part == Part::NearPaw) && mark.part == Part::Paws;
        if (mark.part != part && !paw) continue;
        if (mark.mirror && (earSide == 0 || EarSide(mark, true) == earSide)) {
            DrawMark(g, mark, true, settle, brace, transform);
        }
        if (earSide == 0 || EarSide(mark, false) == earSide) {
            DrawMark(g, mark, false, settle, brace, transform);
        }
    }
    g.Restore(state);
}

void DrawBodyMarks(Graphics& g, const GraphicsPath& clip, const SkinSpec& skin, Part part, float brace) {
    DrawMarks(g, clip, skin, part, 0, brace, [](const Mark&, bool, PointF point) { return point; });
}

void Stroke(Graphics& g, std::initializer_list<PointF> points, Color color, float thickness) {
    Pen pen(color, thickness);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawBeziers(&pen, points.begin(), static_cast<INT>(points.size()));
}

using TailPoints = std::array<PointF, tailSegments + 1>;

void DrawLonghairTail(Graphics& g, const TailPoints& spine, bool inward, const SkinSpec& skin, float brace) {
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

    const auto tuft = [&](const Mark&, bool, PointF point) {
        const float distance = std::clamp(point.X, 0.0f, static_cast<float>(tailSegments));
        const int i = std::min(static_cast<int>(distance), tailSegments - 1);
        const float fraction = distance - i;
        const auto center = Lerp(spine[i], spine[i + 1], fraction);
        const auto edge = point.Y >= 0 ? Lerp(front[i], front[i + 1], fraction)
                                      : Lerp(back[tailSegments - i], back[tailSegments - i - 1], fraction);
        return Lerp(center, edge, std::abs(point.Y));
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
    Fill(g, plume, skin.tail);
    DrawMarks(g, plume, skin, Part::Tail, 0, brace, tuft);
    DrawFurOutline(g, plume, skin);
    g.Restore(state);
}

void DrawTail(Graphics& g, const Pose& pose, const SkinSpec& skin, float brace) {
    const bool longhair = skin.longhair;
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
        DrawLonghairTail(g, points, pose.tailDirection < 0, skin, brace);
        return;
    }

    Pen tail(skin.tail, 9.2f);
    tail.SetStartCap(LineCapRound);
    tail.SetEndCap(LineCapRound);
    constexpr float tension = 0.38f;
    g.DrawCurve(&tail, points.data(), tailSegments + 1, tension);
    for (const auto& band : skin.tailBands) {
        Pen stripe(band.color, 9.2f);
        if (band.to == tailSegments) stripe.SetEndCap(LineCapRound);
        g.DrawCurve(&stripe, points.data(), tailSegments + 1, band.from, band.to - band.from, tension);
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

HeadPosition PositionHead(const Pose& pose, float lift, bool longhair) {
    const PointF center{faceCenter.X + lift * 0.12f, faceCenter.Y - lift};
    const Gaze gaze(pose.cursor, center, pose.alert);
    const float droop = 1.0f - pose.alert;
    const float settle = droop * droop;
    const float sleepDrop = 5.8f + (longhair ? 3.0f : 0.0f);
    return {gaze, {center.X + gaze.x * 1.6f + settle * 5.5f, center.Y + gaze.y * 1.25f + droop * sleepDrop},
            -4.0f * gaze.x - 17.0f * droop, settle};
}

void DrawEye(Graphics& g, float side, const Gaze& gaze, const Pose& pose, const SkinSpec& skin) {
    const PointF center = gaze.Project(side * 13.6f, -1.0f, 15.0f);
    const float radiusX = 9.7f * (1.0f - side * gaze.x * 0.17f);
    const float radiusY = 9.1f * (1.0f - std::abs(gaze.y) * 0.14f);
    const auto state = g.Save();
    g.TranslateTransform(center.X, center.Y);
    g.RotateTransform(side * 3.4f + gaze.x * 2.0f);

    if (pose.eyeOpen < 0.18f) {
        const float lidAlert = skin.longhair && pose.squint > 0 ? 0.0f : pose.alert;
        const auto tint = [lidAlert](BYTE resting, BYTE awake) {
            return static_cast<BYTE>(resting + (awake - resting) * lidAlert);
        };
        const Color lid(255, tint(skin.eyelid.GetR(), skin.iris.GetR()),
                        tint(skin.eyelid.GetG(), skin.iris.GetG()),
                        tint(skin.eyelid.GetB(), skin.iris.GetB()));
        const float lidWidth = radiusX * (0.77f + 0.23f * pose.alert);
        const float lidCurve = 2.1f - 2.8f * pose.squint;
        Stroke(g, {{-lidWidth, 0}, {-lidWidth * 0.48f, lidCurve}, {lidWidth * 0.43f, lidCurve}, {lidWidth, 0}},
               lid, 1.35f + 0.4f * pose.squint);
    } else {
        g.ScaleTransform(1.0f, pose.eyeOpen);
        SolidBrush iris(skin.iris);
        g.FillEllipse(&iris, -radiusX, -radiusY, 2 * radiusX, 2 * radiusY);

        const float pupilX = gaze.x * (2.1f - 1.0f * pose.hunt);
        const float pupilY = gaze.y * (1.65f - 0.72f * pose.hunt);
        const float huntPupil = std::min(radiusX, radiusY) - std::hypot(pupilX, pupilY) - 1.65f;
        const float slitX = radiusX * 0.24f;
        const float slitY = radiusY * 0.72f;
        const float pupilRadiusX = slitX + (huntPupil - slitX) * pose.hunt;
        const float pupilRadiusY = slitY + (huntPupil - slitY) * pose.hunt;
        SolidBrush dark(skin.pupil);
        g.FillEllipse(&dark, pupilX - pupilRadiusX, pupilY - pupilRadiusY,
                      2.0f * pupilRadiusX, 2.0f * pupilRadiusY);
        const float shine = std::max(skin.eyeShine, (pose.hunt - 0.20f) / 0.80f);
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

PointF EarPoint(float side, float x, float y, float twitch) {
    const float flick = std::max(0.0f, side * twitch);
    const float tip = std::clamp((-y - 22.0f) / 17.0f, 0.0f, 1.0f);
    return {x + side * 5.3f * flick * tip, y + 3.3f * flick * tip};
}

void LonghairEarOutline(GraphicsPath& ear, float side, float twitch, bool tufted) {
    const auto p = [side, twitch](float x, float y) { return EarPoint(side, x * side, y, twitch); };
    if (tufted) {
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

PointF LonghairCheek(float x, float y, float settle) {
    const float contact = std::max(y, 0.0f) * settle;
    const PointF sheared{x - contact * 0.08f, y - contact * 0.19f};
    if (settle <= 0 || x >= -8.0f) return sheared;

    const float droop = std::sqrt(settle);
    const float tilt = -17.0f * droop * (3.14159265f / 180.0f);
    const float cosine = std::cos(tilt), sine = std::sin(tilt);
    PointF upright{cosine * sheared.X - sine * sheared.Y, sine * sheared.X + cosine * sheared.Y};
    // The ledge meets the lower left cheek at the screen height of (-28, 14).
    const float below = upright.Y - (-28.0f * sine + 14.0f * cosine);
    if (below <= 0) return sheared;
    const float t = std::clamp((-x - 8.0f) / 14.0f, 0.0f, 1.0f);
    const float press = 0.85f * below * droop * t * t * (3.0f - 2.0f * t);
    upright.X -= 0.6f * press;
    upright.Y -= press;
    return {cosine * upright.X + sine * upright.Y, -sine * upright.X + cosine * upright.Y};
}

void LonghairFaceOutline(GraphicsPath& face, float settle, bool round) {
    const auto c = [settle](float x, float y) {
        return LonghairCheek(x, y, settle);
    };
    if (round) {
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
                   c(27, 22.5f), c(22, 26), c(14, 27.3f),
                   c(9, 28.1f), c(4.5f, 28.4f), c(0, 28.4f),
                   c(-8, 28.4f), c(-21, 26), c(-28, 19),
                   c(-29, 20), c(-30, 21), c(-30, 23),
                   c(-35, 21), c(-38, 16), c(-37, 10),
                   c(-38, 11), c(-40, 12), c(-42, 12),
                   c(-42, 6), c(-39, 0), c(-36, -4),
                   c(-37, -4), c(-38, -4), c(-39, -4),
                   c(-37, -5), c(-34, -7), c(-33, -9)});
}

void DrawChatterMouth(Graphics& g, const Gaze& gaze, const Pose& pose, const SkinSpec& skin) {
    if (pose.chatter <= 0) return;

    const float jawPulse = pose.chatterAge < 0 ? 0.0f : 0.5f + 0.5f * std::sin(51.0f * pose.chatterAge);
    const float jaw = pose.chatter * (3.3f + 1.6f * jawPulse);
    const BYTE opacity = static_cast<BYTE>(255 * pose.chatter);
    const Color mouth(static_cast<BYTE>(skin.mouth.GetA() * pose.chatter),
                      skin.mouth.GetR(), skin.mouth.GetG(), skin.mouth.GetB());
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
    const float squash = 1.8f * settle;
    const float cheekEdge = -31.0f - 1.5f * squash;
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
                       {10, 28 - 0.5f * settle}, {-9 - 5 * squash, 27 - 8.3f * settle},
                       {-20 - 3 * squash, 22 - 6 * settle}, {-27 - 4 * squash, 19 - 5.5f * settle},
                       {-31 - 6 * squash, 13 - 4 * settle}, {cheekEdge, 4}};
    silhouette.AddBeziers(jaw, static_cast<INT>(std::size(jaw)));
    silhouette.CloseFigure();
}

void DrawHead(Graphics& g, const Pose& pose, const SkinSpec& skin, float lift = 0) {
    const auto head = PositionHead(pose, lift, skin.longhair);
    const auto& gaze = head.gaze;
    const float settle = head.settle;
    const auto state = g.Save();
    g.TranslateTransform(head.center.X, head.center.Y);
    g.RotateTransform(head.angle);
    const bool longhair = skin.longhair;
    const auto transform = [&](const Mark& mark, bool mirrored, PointF point) {
        switch (mark.space) {
        case Space::Skull: return LonghairCheek(point.X, point.Y, settle);
        case Space::Ear: return EarPoint(EarSide(mark, mirrored), point.X, point.Y, pose.earTwitch);
        default: return gaze.Project(point.X, point.Y, mark.depth);
        }
    };
    GraphicsPath face;
    if (longhair) {
        for (float side : {-1.0f, 1.0f}) {
            GraphicsPath ear;
            LonghairEarOutline(ear, side, pose.earTwitch, skin.tufted);
            Fill(g, ear, skin.ear);
            DrawFurOutline(g, ear, skin);
            DrawMarks(g, ear, skin, Part::Ear, settle, 0, transform, side);
        }
        LonghairFaceOutline(face, settle, skin.roundFace);
        Fill(g, face, skin.face);
    } else {
        ShorthairHeadOutline(face, pose, settle);
        Fill(g, face, skin.coat);
    }
    DrawMarks(g, face, skin, Part::Head, settle, 0, transform);
    if (longhair) DrawOutline(g, face, skin.faceOutline, skin.faceOutlineWidth);
    else DrawFurOutline(g, face, skin);

    DrawChatterMouth(g, gaze, pose, skin);
    for (const float side : {-1.0f, 1.0f}) {
        DrawEye(g, side, gaze, pose, skin);
        if (longhair) {
            for (float row : {0.0f, 1.0f}) {
                const auto w = [side, &gaze](float x, float y) { return gaze.Project(side * x, y, 10); };
                Stroke(g, {w(15 + row, 9 + 3 * row), w(20, 8 + 3 * row), w(26, 7 + 4 * row),
                           w(32 - row, 8 + 5 * row)}, skin.whisker, 0.65f);
            }
        } else {
            DrawWhiskers(g, side, gaze, side < 0 ? settle : 0.0f, skin.whisker);
        }
    }
    DrawChatterMarks(g, pose);
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

void DrawLonghairRuff(Graphics& g, float brace, const SkinSpec& skin) {
    GraphicsPath bib;
    LonghairRuffOutline(bib, brace);
    Fill(g, bib, skin.ruff);
    DrawOutline(g, bib, skin.ruffEdge, 0.9f);
    DrawBodyMarks(g, bib, skin, Part::Ruff, brace);
}

void BodyOutline(GraphicsPath& silhouette, float brace, const SkinSpec& skin) {
    if (skin.longhair) {
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
              const SkinSpec& skin, float brace) {
    Fill(g, silhouette, skin.coat);
    DrawBodyMarks(g, silhouette, skin, Part::Body, brace);
    if (skin.outline) {
        const auto state = g.Save();
        if (farPaw.GetPointCount() > 0) g.SetClip(&farPaw, CombineModeExclude);
        DrawFurOutline(g, silhouette, skin);
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

void PaintPaw(Graphics& g, const GraphicsPath& paw, const SkinSpec& skin, const PawPress& foot, Part part,
              float lift) {
    Fill(g, paw, skin.coat);
    DrawMarks(g, paw, skin, part, 0, lift, [&](const Mark& mark, bool, PointF point) {
        return mark.space == Space::Foot ? foot(foot.contact.X + foot.direction * point.X, foot.contact.Y + point.Y)
                                         : point;
    });
}

void FarFrontPawOutline(GraphicsPath& paw, float lift, const PawPress& foot) {
    Outline(paw, {{58, 65 - 17 * lift}, {65, 59 - 18 * lift}, {77, 62 - 14 * lift}, {80, 70 - 13 * lift},
                  {82, 74 - 7 * lift}, {83, 77 - 3 * lift}, foot(82, 79),
                  foot(81, 85), foot(76, 90), foot(70, 90),
                  foot(63, 90), foot(59, 85), foot(59, 79),
                  {59, 75 - 5 * lift}, {55, 70 - 11 * lift}, {58, 65 - 17 * lift}});
}

void DrawFarFrontPaw(Graphics& g, GraphicsPath& paw, const SkinSpec& skin, float lift,
                     const PawPress& foot, float tuck) {
    if (tuck >= 0.999f) return;
    FarFrontPawOutline(paw, lift, foot);
    const auto paint = [&] {
        PaintPaw(g, paw, skin, foot, Part::FarPaw, lift);
        DrawFurOutline(g, paw, skin);
        DrawToeCreases(g, foot, skin.crease);
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

void DrawNearFrontPaw(Graphics& g, const GraphicsPath& body, const SkinSpec& skin,
                      float lift, float press, float tuck) {
    if (tuck >= 0.999f) return;
    GraphicsPath paw;
    const PawPress foot({107, 93}, 1.0f, press);
    NearFrontPawOutline(paw, lift, foot);
    const auto paint = [&](const GraphicsPath& bodyHere) {
        PaintPaw(g, paw, skin, foot, Part::NearPaw, lift);
        if (skin.outline) {
            const auto edge = g.Save();
            g.SetClip(&bodyHere, CombineModeExclude);
            DrawFurOutline(g, paw, skin);
            g.Restore(edge);
        }
        DrawToeCreases(g, foot, skin.crease);
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

bool HitsPettingArea(PointF cursor, const Pose& pose, const SkinSpec& skin, float headroom) {
    if (cursor.X < 0 || cursor.X > foldedRight + 4 || cursor.Y < -headroom || cursor.Y > 134) return false;

    const float brace = UpperBodyBrace(pose, headroom);
    const auto head = PositionHead(pose, brace * fullHeadLift, skin.longhair);
    const float angle = head.angle * (3.14159265f / 180.0f);
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const float dx = cursor.X - head.center.X, dy = cursor.Y - head.center.Y;
    const PointF local{cosine * dx + sine * dy, -sine * dx + cosine * dy};
    GraphicsPath shape;
    if (std::abs(local.X) <= 44 && local.Y >= -48 && local.Y <= 29) {
        if (skin.longhair) {
            LonghairFaceOutline(shape, head.settle, skin.roundFace);
            if (shape.IsVisible(local)) return true;
            for (float side : {-1.0f, 1.0f}) {
                shape.Reset();
                LonghairEarOutline(shape, side, pose.earTwitch, skin.tufted);
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
    if (!skin.longhair) return false;

    shape.Reset();
    LonghairRuffOutline(shape, brace);
    return shape.IsVisible(cursor);
}

void Draw(Graphics& g, const Pose& pose, const SkinSpec& skin, float headroom) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    const float upperBodyBrace = UpperBodyBrace(pose, headroom);
    GraphicsPath body, farPaw;
    BodyOutline(body, upperBodyBrace, skin);
    DrawTail(g, pose, skin, upperBodyBrace);
    const PawPress farFoot({70, 90}, -1.0f, pose.brace);
    DrawFarFrontPaw(g, farPaw, skin, upperBodyBrace, farFoot, pose.pawTuckLeft);
    DrawBody(g, body, farPaw, skin, upperBodyBrace);
    DrawNearFrontPaw(g, body, skin, upperBodyBrace, pose.brace, pose.pawTuckRight);
    if (skin.longhair) DrawLonghairRuff(g, upperBodyBrace, skin);
    DrawHead(g, pose, skin, upperBodyBrace * fullHeadLift);
    DrawDreams(g, pose);
    DrawPetSymbols(g, pose);
}

void DrawTrayIcon(Graphics& g, const SkinSpec& skin) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    const bool longhair = skin.longhair;
    const float scale = longhair ? 0.33f : 0.36f;
    g.ScaleTransform(scale, scale);
    g.TranslateTransform(longhair ? -11.0f : -15.5f, -2.0f);
    DrawHead(g, {faceCenter, 0, 0, 1, 1, 1, 1, 0, 0}, skin);
}

} // namespace cat
