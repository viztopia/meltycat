#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cat {

constexpr std::size_t maxSkinBytes = 512 * 1024;
constexpr int tailSegments = 24;

enum class Part { Head, Ear, Body, FarPaw, NearPaw, Paws, Ruff, Tail };
enum class Space { Head, Skull, Ear, Body, Foot, Tail };
enum class PathKind { Move, Line, Quad, Cubic, Close };

struct PathCommand {
    PathKind kind;
    std::array<Gdiplus::PointF, 3> points{};
};

using MarkPath = std::vector<PathCommand>;

struct Mark {
    Part part;
    Space space;
    Gdiplus::Color color;
    MarkPath path;
    MarkPath asleep;
    MarkPath braced;
    bool mirror;
    float depth;
    bool left;
};

struct TailBand {
    int from;
    int to;
    Gdiplus::Color color;
};

struct SkinSpec {
    std::wstring name;
    std::wstring author;
    bool longhair = false;
    bool tufted = false;
    bool roundFace = false;
    bool missingLeftFrontPaw = false;
    bool missingRightFrontPaw = false;
    Gdiplus::Color coat, face, ear, tail, ruff;
    std::optional<Gdiplus::Color> outline, ruffEdge, faceOutline;
    float faceOutlineWidth = 1;
    Gdiplus::Color iris, pupil, eyelid, whisker, crease, mouth;
    float eyeShine = 0;
    std::vector<Mark> marks;
    std::vector<TailBand> tailBands;
};

std::optional<SkinSpec> ParseSkin(std::string_view bytes, std::wstring& error);

} // namespace cat
