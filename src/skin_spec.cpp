#include "skin_spec.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>

namespace cat {
namespace {

using namespace winrt::Windows::Data::Json;
using Gdiplus::Color;
using Gdiplus::PointF;

struct SkinError {
    std::wstring message;
};

class Fields {
public:
    Fields(JsonObject object, std::wstring prefix = {})
        : object_(std::move(object)), prefix_(std::move(prefix)) {}

    [[noreturn]] void Fail(std::wstring_view key, std::wstring_view problem) const {
        throw SkinError{prefix_ + std::wstring(key) + L": " + std::wstring(problem)};
    }

    IJsonValue Value(std::wstring_view key, JsonValueType type, std::wstring_view description,
                     bool required = false) const {
        const auto value = object_.TryLookup(key);
        if (!value && required) Fail(key, L"required");
        if (value && value.ValueType() != type) Fail(key, description);
        return value;
    }

    std::wstring Text(std::wstring_view key, bool required = false) const {
        const auto value = Value(key, JsonValueType::String, L"must be a string", required);
        return value ? std::wstring(value.GetString()) : std::wstring();
    }

    double Number(std::wstring_view key, double fallback, bool required = false) const {
        const auto value = Value(key, JsonValueType::Number, L"must be a number", required);
        if (!value) return fallback;
        const double number = value.GetNumber();
        if (!std::isfinite(number)) Fail(key, L"must be a finite number");
        return number;
    }

    bool Bool(std::wstring_view key) const {
        const auto value = Value(key, JsonValueType::Boolean, L"must be true or false");
        return value && value.GetBoolean();
    }

    JsonArray Array(std::wstring_view key, bool required = false) const {
        const auto value = Value(key, JsonValueType::Array, L"must be an array", required);
        return value ? value.GetArray() : nullptr;
    }

    std::optional<Color> Paint(std::wstring_view key, bool required = false) const {
        const auto value = Value(key, JsonValueType::String, L"must be a string", required);
        if (!value) return std::nullopt;
        const auto text = value.GetString();
        if ((text.size() != 7 && text.size() != 9) || text[0] != L'#') {
            Fail(key, L"expected #RRGGBB or #RRGGBBAA");
        }
        unsigned int rgba = 0;
        for (uint32_t i = 1; i < text.size(); ++i) {
            const auto c = text[i];
            unsigned int digit;
            if (c >= L'0' && c <= L'9') digit = c - L'0';
            else if (c >= L'a' && c <= L'f') digit = c - L'a' + 10;
            else if (c >= L'A' && c <= L'F') digit = c - L'A' + 10;
            else Fail(key, L"expected #RRGGBB or #RRGGBBAA");
            rgba = (rgba << 4) | digit;
        }
        if (text.size() == 7) rgba = (rgba << 8) | 0xFF;
        return Color(static_cast<BYTE>(rgba), static_cast<BYTE>(rgba >> 24),
                     static_cast<BYTE>(rgba >> 16), static_cast<BYTE>(rgba >> 8));
    }

    template <typename T, std::size_t N>
    T Choice(std::wstring_view key, const std::pair<std::wstring_view, T> (&values)[N],
             std::optional<T> fallback = {}) const {
        const auto value = Value(key, JsonValueType::String, L"must be a string", !fallback);
        if (!value) return *fallback;
        const auto text = value.GetString();
        for (const auto& [name, entry] : values) {
            if (text == name) return entry;
        }
        Fail(key, L"unknown value \"" + std::wstring(text) + L"\"");
    }

private:
    JsonObject object_;
    std::wstring prefix_;
};

constexpr int maxMarks = 200;
constexpr int maxPathLength = 8000;
constexpr int maxCommands = 400;
constexpr double coordinateLimit = 500;

// Parse SVG path numbers and normalize shorthand into moves, lines, and curves.
MarkPath ParsePath(const std::wstring& path, const Fields& fields, std::wstring_view key) {
    if (path.size() > maxPathLength) fields.Fail(key, L"longer than 8000 characters");
    const auto text = winrt::to_string(path);
    const char* next = text.data();
    const char* end = next + text.size();
    const auto skip = [&] {
        while (next != end && (*next == ',' || *next == ' ' || *next == '\r' ||
                               *next == '\n' || *next == '\t')) ++next;
    };
    const auto value = [&] {
        skip();
        if (next != end && *next == '+') ++next;
        double number = 0;
        const auto parsed = std::from_chars(next, end, number, std::chars_format::general);
        if (parsed.ec != std::errc{} || !std::isfinite(number)) fields.Fail(key, L"expected a number");
        next = parsed.ptr;
        return std::clamp(number, -coordinateLimit, coordinateLimit);
    };
    const auto coordinate = [](double number) {
        return static_cast<float>(std::clamp(number, -coordinateLimit, coordinateLimit));
    };
    MarkPath commands;
    PointF current, start;
    char active = 0;
    for (skip(); next != end; skip()) {
        if ((*next >= 'A' && *next <= 'Z') || (*next >= 'a' && *next <= 'z')) active = *next++;
        const bool relative = active >= 'a' && active <= 'z';
        const char type = relative ? static_cast<char>(active - ('a' - 'A')) : active;
        if ((commands.empty() && type != 'M') || !active) fields.Fail(key, L"expected a move command");
        const auto point = [&] {
            const float x = coordinate((relative ? current.X : 0) + value());
            const float y = coordinate((relative ? current.Y : 0) + value());
            return PointF{x, y};
        };
        switch (type) {
        case 'M':
            current = start = point();
            commands.push_back({PathKind::Move, {current}});
            active = relative ? 'l' : 'L';
            break;
        case 'L':
            current = point();
            commands.push_back({PathKind::Line, {current}});
            break;
        case 'H':
            current.X = coordinate((relative ? current.X : 0) + value());
            commands.push_back({PathKind::Line, {current}});
            break;
        case 'V':
            current.Y = coordinate((relative ? current.Y : 0) + value());
            commands.push_back({PathKind::Line, {current}});
            break;
        case 'C': {
            const auto c1 = point(), c2 = point();
            current = point();
            commands.push_back({PathKind::Cubic, {c1, c2, current}});
            break;
        }
        case 'Q': {
            const auto control = point();
            current = point();
            commands.push_back({PathKind::Quad, {control, current}});
            break;
        }
        case 'Z':
            commands.push_back({PathKind::Close});
            current = start;
            active = 0;
            break;
        default:
            fields.Fail(key, L"unsupported command (use M, L, H, V, C, Q, Z)");
        }
        if (commands.size() > maxCommands) fields.Fail(key, L"more than 400 commands");
    }
    if (commands.empty()) fields.Fail(key, L"must start with M");
    return commands;
}

Mark ParseMark(const Fields& fields, bool longhair) {
    constexpr std::pair<std::wstring_view, Part> parts[]{
        {L"head", Part::Head}, {L"ear", Part::Ear}, {L"body", Part::Body}, {L"farPaw", Part::FarPaw},
        {L"nearPaw", Part::NearPaw}, {L"paws", Part::Paws}, {L"ruff", Part::Ruff}, {L"tail", Part::Tail}
    };
    constexpr std::pair<std::wstring_view, Space> spaces[]{
        {L"head", Space::Head}, {L"skull", Space::Skull}, {L"ear", Space::Ear},
        {L"body", Space::Body}, {L"foot", Space::Foot}, {L"tail", Space::Tail}
    };
    constexpr std::pair<std::wstring_view, bool> sides[]{{L"left", true}, {L"right", false}};
    const auto part = fields.Choice(L"part", parts);
    const auto space = fields.Choice(L"space", spaces);
    const bool headSpace = space == Space::Head || space == Space::Skull || space == Space::Ear;
    bool allowed = false;
    switch (part) {
    case Part::Head: allowed = headSpace; break;
    case Part::Ear: allowed = space == Space::Ear; break;
    case Part::Body:
    case Part::Ruff: allowed = space == Space::Body; break;
    case Part::Tail: allowed = space == Space::Tail; break;
    case Part::FarPaw:
    case Part::NearPaw:
    case Part::Paws: allowed = space == Space::Body || space == Space::Foot; break;
    }
    if (!allowed) fields.Fail(L"space", L"incompatible with this part");
    if (!longhair && (part == Part::Ear || part == Part::Ruff || part == Part::Tail)) {
        fields.Fail(L"part", L"exists only on longhair bodies");
    }
    const auto color = *fields.Paint(L"color", true);
    auto path = ParsePath(fields.Text(L"path", true), fields, L"path");
    const auto morph = [&](const wchar_t* key) {
        const auto value = fields.Value(key, JsonValueType::String, L"must be a string");
        if (!value) return MarkPath{};
        auto target = ParsePath(std::wstring(value.GetString()), fields, key);
        if (target.size() != path.size() ||
            !std::equal(target.begin(), target.end(), path.begin(), [](const auto& a, const auto& b) {
                return a.kind == b.kind;
            })) {
            fields.Fail(key, L"must use the same commands as path");
        }
        return target;
    };
    auto asleep = morph(L"asleep");
    auto braced = morph(L"braced");
    const bool mirror = fields.Bool(L"mirror");
    if (mirror && !headSpace) fields.Fail(L"mirror", L"only head, skull and ear spaces can be mirrored");
    const float depth = static_cast<float>(std::clamp(fields.Number(L"depth", 14), 0.0, 30.0));
    const bool left = fields.Choice(L"side", sides, std::optional{false});
    return {part, space, color, std::move(path), std::move(asleep), std::move(braced), mirror, depth, left};
}

std::wstring Label(std::wstring text) {
    const auto begin = std::find_if_not(text.begin(), text.end(), [](wchar_t c) { return std::iswspace(c); });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](wchar_t c) { return std::iswspace(c); }).base();
    if (begin >= end) return {};
    std::wstring result(begin, end);
    if (result.size() > 40) {
        // Keep UTF-16 surrogate pairs together at the truncation point.
        result.resize(result[39] >= 0xD800 && result[39] <= 0xDBFF ? 39 : 40);
    }
    return result;
}

SkinSpec ParseObject(const JsonObject& object) {
    constexpr std::pair<std::wstring_view, bool> bodies[]{{L"shorthair", false}, {L"longhair", true}};
    constexpr std::pair<std::wstring_view, bool> ears[]{{L"plain", false}, {L"tufted", true}};
    constexpr std::pair<std::wstring_view, bool> faces[]{{L"fluffy", false}, {L"round", true}};
    const Fields root(object);
    if (const auto format = root.Value(L"format", JsonValueType::String, L"must be a string")) {
        if (format.GetString() != L"meltycat-skin") root.Fail(L"format", L"expected \"meltycat-skin\"");
    }
    const auto version = root.Number(L"formatVersion", 0, true);
    if (version > 1) root.Fail(L"formatVersion", L"needs a newer meltycat");
    if (version != 1) root.Fail(L"formatVersion", L"expected 1");

    SkinSpec spec;
    spec.name = Label(root.Text(L"name", true));
    if (spec.name.empty()) root.Fail(L"name", L"required");
    spec.author = Label(root.Text(L"author"));
    spec.longhair = root.Choice(L"body", bodies);
    spec.tufted = root.Choice(L"ears", ears, std::optional{false});
    spec.roundFace = root.Choice(L"face", faces, std::optional{false});
    spec.faceOutlineWidth = static_cast<float>(std::clamp(root.Number(L"faceOutlineWidth", 1), 0.5, 2.0));
    spec.eyeShine = static_cast<float>(std::clamp(root.Number(L"eyeShine", 0), 0.0, 1.0));

    if (const auto paws = root.Array(L"missingFrontPaws")) {
        for (unsigned int i = 0; i < paws.Size(); ++i) {
            const auto item = paws.GetAt(i);
            const auto side = item.ValueType() == JsonValueType::String ? item.GetString() : winrt::hstring{};
            if (side == L"left") spec.missingLeftFrontPaw = true;
            else if (side == L"right") spec.missingRightFrontPaw = true;
            else root.Fail(L"missingFrontPaws[" + std::to_wstring(i) + L"]", L"expected \"left\" or \"right\"");
        }
    }

    const Fields palette(root.Value(L"palette", JsonValueType::Object, L"must be an object", true).GetObject(),
                         L"palette.");
    spec.coat = *palette.Paint(L"coat", true);
    spec.iris = *palette.Paint(L"iris", true);
    spec.face = palette.Paint(L"face").value_or(spec.coat);
    spec.ear = palette.Paint(L"ear").value_or(spec.coat);
    spec.tail = palette.Paint(L"tail").value_or(spec.coat);
    spec.ruff = palette.Paint(L"ruff").value_or(spec.coat);
    spec.outline = palette.Paint(L"outline");
    spec.ruffEdge = palette.Paint(L"ruffEdge");
    if (!spec.ruffEdge) spec.ruffEdge = spec.outline;
    spec.faceOutline = palette.Paint(L"faceOutline");
    if (!spec.faceOutline) spec.faceOutline = spec.outline;
    spec.pupil = palette.Paint(L"pupil").value_or(Color(255, 25, 25, 29));
    const auto shade = [](BYTE value) { return static_cast<BYTE>(std::lround(0.7 * value)); };
    spec.eyelid = palette.Paint(L"eyelid").value_or(
        Color(255, shade(spec.iris.GetR()), shade(spec.iris.GetG()), shade(spec.iris.GetB())));
    const bool dark = 0.2126 * spec.coat.GetR() + 0.7152 * spec.coat.GetG() + 0.0722 * spec.coat.GetB() < 127.5;
    spec.whisker = palette.Paint(L"whisker").value_or(dark ? Color(240, 159, 162, 165) : Color(180, 161, 139, 132));
    spec.crease = palette.Paint(L"crease").value_or(dark ? Color(215, 131, 138, 140) : Color(195, 145, 122, 116));
    spec.mouth = palette.Paint(L"mouth").value_or(Color(255, 14, 12, 17));

    if (const auto marks = root.Array(L"marks")) {
        if (marks.Size() > maxMarks) root.Fail(L"marks", L"at most 200 marks");
        spec.marks.reserve(marks.Size());
        for (unsigned int i = 0; i < marks.Size(); ++i) {
            const auto key = L"marks[" + std::to_wstring(i) + L"]";
            const auto item = marks.GetAt(i);
            if (item.ValueType() != JsonValueType::Object) root.Fail(key, L"must be an object");
            spec.marks.push_back(ParseMark(Fields(item.GetObject(), key + L"."), spec.longhair));
        }
    }
    if (const auto bands = root.Array(L"tailBands")) {
        if (bands.Size() && spec.longhair) root.Fail(L"tailBands", L"only for shorthair bodies");
        if (bands.Size() > tailSegments) root.Fail(L"tailBands", L"at most 24 bands");
        for (unsigned int i = 0; i < bands.Size(); ++i) {
            const auto key = L"tailBands[" + std::to_wstring(i) + L"]";
            const auto item = bands.GetAt(i);
            if (item.ValueType() != JsonValueType::Object) root.Fail(key, L"must be an object");
            const Fields band(item.GetObject(), key + L".");
            const auto span = band.Array(L"span", true);
            if (span.Size() != 2) band.Fail(L"span", L"expected [from, to] with segments 0 to 24");
            const auto index = [&](unsigned int j) {
                const auto number = span.GetAt(j);
                if (number.ValueType() != JsonValueType::Number || !std::isfinite(number.GetNumber())) {
                    band.Fail(L"span", L"expected [from, to] with segments 0 to 24");
                }
                return static_cast<int>(std::clamp(number.GetNumber(), 0.0, double(tailSegments)));
            };
            const int from = index(0), to = index(1);
            if (from >= to) band.Fail(L"span", L"from must be less than to");
            spec.tailBands.push_back({from, to, *band.Paint(L"color", true)});
        }
    }
    return spec;
}

} // namespace

std::optional<SkinSpec> ParseSkin(std::string_view bytes, std::wstring& error) {
    error.clear();
    if (bytes.size() > maxSkinBytes) {
        error = L"file is larger than 512 KB";
        return std::nullopt;
    }
    if (bytes.substr(0, 3) == "\xEF\xBB\xBF") bytes.remove_prefix(3);
    try {
        const int length = static_cast<int>(bytes.size());
        const int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), length, nullptr, 0);
        if (!characters) throw SkinError{L"not valid UTF-8 JSON"};
        std::wstring text(characters, L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), length, text.data(), characters)) {
            throw SkinError{L"not valid UTF-8 JSON"};
        }
        const auto json = JsonValue::Parse(text);
        if (json.ValueType() != JsonValueType::Object) throw SkinError{L"the top level must be a JSON object"};
        return ParseObject(json.GetObject());
    } catch (const SkinError& rejected) {
        error = rejected.message;
    } catch (const winrt::hresult_error&) {
        error = L"not valid JSON";
    }
    return std::nullopt;
}

} // namespace cat


