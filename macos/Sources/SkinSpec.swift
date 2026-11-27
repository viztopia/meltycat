import CoreGraphics
import Foundation

// A .meltycat skin: JSON describing colours and markings over one of the two built-in bodies.
// The format is specified in skins/AI-GUIDE.md; built-in skins in skins/ use it too.
struct SkinSpec {
    enum Body: String { case shorthair, longhair }
    enum Ears: String { case plain, tufted }
    enum Face: String { case fluffy, round }

    var name: String
    var author: String
    var body: Body
    var ears: Ears
    var face: Face
    var coat: Argb
    var faceColor: Argb
    var ear: Argb
    var tail: Argb
    var ruff: Argb
    var ruffEdge: Argb?
    var outline: Argb?
    var faceOutline: Argb?
    var faceOutlineWidth: CGFloat
    var iris: Argb
    var pupil: Argb
    var eyelid: Argb
    var whisker: Argb
    var crease: Argb
    var mouth: Argb
    var eyeShine: CGFloat
    var marks: [Mark]
    var tailBands: [TailBand]

    var isLonghair: Bool { body == .longhair }
    // The colour that fills most of the head, which decides how the menu bar icon is outlined.
    var headColor: Argb { isLonghair ? faceColor : coat }
}

struct Mark {
    enum Part: String { case head, ear, body, farPaw, nearPaw, paws, ruff, tail }
    enum Space: String { case head, skull, ear, body, foot, tail }
    enum Side: String { case left, right }

    var part: Part
    var space: Space
    var color: Argb
    var path: [PathCommand]
    var asleep: [PathCommand]?
    var braced: [PathCommand]?
    var mirror: Bool
    var depth: CGFloat
    var side: Side
}

struct TailBand {
    var from: Int
    var to: Int
    var color: Argb
}

enum PathCommand: Equatable {
    case move(CGPoint)
    case line(CGPoint)
    case quad(CGPoint, CGPoint)
    case cubic(CGPoint, CGPoint, CGPoint)
    case close

    var points: [CGPoint] {
        switch self {
        case .move(let p), .line(let p): [p]
        case .quad(let c, let p): [c, p]
        case .cubic(let c1, let c2, let p): [c1, c2, p]
        case .close: []
        }
    }

    func sameKind(as other: PathCommand) -> Bool {
        switch (self, other) {
        case (.move, .move), (.line, .line), (.quad, .quad), (.cubic, .cubic), (.close, .close): true
        default: false
        }
    }

    func with(_ points: [CGPoint]) -> PathCommand {
        switch self {
        case .move: .move(points[0])
        case .line: .line(points[0])
        case .quad: .quad(points[0], points[1])
        case .cubic: .cubic(points[0], points[1], points[2])
        case .close: .close
        }
    }
}

struct SkinError: Error, CustomStringConvertible {
    var description: String
    init(_ description: String) { self.description = description }
}

extension SkinSpec {
    static let maxBytes = 512 * 1024
    static let maxMarks = 200
    private static let coordinateLimit: CGFloat = 500

    static func parse(_ data: Data) throws -> SkinSpec {
        if data.count > maxBytes { throw SkinError("file is larger than \(maxBytes / 1024) KB") }
        let json: Any
        do {
            json = try JSONSerialization.jsonObject(with: data)
        } catch {
            throw SkinError("not valid JSON")
        }
        guard let root = json as? [String: Any] else { throw SkinError("the top level must be a JSON object") }
        return try SkinSpec(root)
    }

    private init(_ root: [String: Any]) throws {
        let fields = Fields(root, at: "")
        if let format = try fields.string("format"), format != "meltycat-skin" {
            throw SkinError("format: expected \"meltycat-skin\"")
        }
        guard let version = try fields.number("formatVersion") else { throw SkinError("formatVersion: required") }
        if version > 1 { throw SkinError("formatVersion \(Int(version)) needs a newer meltycat") }
        if version != 1 { throw SkinError("formatVersion: expected 1") }

        let trimmedName = (try fields.string("name") ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmedName.isEmpty { throw SkinError("name: required") }
        name = String(trimmedName.prefix(40))
        author = String((try fields.string("author") ?? "").trimmingCharacters(in: .whitespacesAndNewlines).prefix(40))
        guard let bodyName = try fields.string("body") else { throw SkinError("body: required (shorthair or longhair)") }
        body = try fields.choice(Body(rawValue: bodyName), "body", bodyName)
        ears = try fields.string("ears").map { try fields.choice(Ears(rawValue: $0), "ears", $0) } ?? .plain
        face = try fields.string("face").map { try fields.choice(Face(rawValue: $0), "face", $0) } ?? .fluffy
        faceOutlineWidth = (try fields.number("faceOutlineWidth")).map { min(max($0, 0.5), 2) } ?? 1
        eyeShine = (try fields.number("eyeShine")).map { min(max($0, 0), 1) } ?? 0

        guard let paletteObject = root["palette"] else { throw SkinError("palette: required") }
        guard let paletteFields = paletteObject as? [String: Any] else { throw SkinError("palette: must be an object") }
        let palette = Fields(paletteFields, at: "palette.")
        guard let coat = try palette.color("coat") else { throw SkinError("palette.coat: required") }
        guard let iris = try palette.color("iris") else { throw SkinError("palette.iris: required") }
        self.coat = coat
        self.iris = iris
        faceColor = try palette.color("face") ?? coat
        ear = try palette.color("ear") ?? coat
        tail = try palette.color("tail") ?? coat
        ruff = try palette.color("ruff") ?? coat
        outline = try palette.color("outline")
        ruffEdge = try palette.color("ruffEdge") ?? outline
        faceOutline = try palette.color("faceOutline") ?? outline
        pupil = try palette.color("pupil") ?? Argb(255, 25, 25, 29)
        eyelid = try palette.color("eyelid") ?? Argb(255, (iris.r * 0.7).rounded(), (iris.g * 0.7).rounded(), (iris.b * 0.7).rounded())
        let darkCoat = (0.2126 * coat.r + 0.7152 * coat.g + 0.0722 * coat.b) / 255 < 0.5
        whisker = try palette.color("whisker") ?? (darkCoat ? Argb(240, 159, 162, 165) : Argb(180, 161, 139, 132))
        crease = try palette.color("crease") ?? (darkCoat ? Argb(215, 131, 138, 140) : Argb(195, 145, 122, 116))
        mouth = try palette.color("mouth") ?? Argb(255, 14, 12, 17)

        let markList = try fields.array("marks") ?? []
        if markList.count > Self.maxMarks { throw SkinError("marks: at most \(Self.maxMarks)") }
        marks = try markList.enumerated().map { index, item in
            guard let object = item as? [String: Any] else { throw SkinError("marks[\(index)]: must be an object") }
            return try Self.mark(Fields(object, at: "marks[\(index)]."), body: bodyName)
        }

        let bandList = try fields.array("tailBands") ?? []
        if !bandList.isEmpty && body == .longhair {
            throw SkinError("tailBands: only for shorthair bodies; draw longhair tail markings as marks with part \"tail\"")
        }
        if bandList.count > 24 { throw SkinError("tailBands: at most 24") }
        tailBands = try bandList.enumerated().map { index, item in
            guard let object = item as? [String: Any] else { throw SkinError("tailBands[\(index)]: must be an object") }
            let band = Fields(object, at: "tailBands[\(index)].")
            guard let span = try band.array("span"), span.count == 2,
                  let from = span[0] as? NSNumber, let to = span[1] as? NSNumber else {
                throw SkinError("tailBands[\(index)].span: expected [from, to] with segments 0 to 24")
            }
            let start = min(max(from.intValue, 0), 24), end = min(max(to.intValue, 0), 24)
            if start >= end { throw SkinError("tailBands[\(index)].span: from must be less than to") }
            guard let color = try band.color("color") else { throw SkinError("tailBands[\(index)].color: required") }
            return TailBand(from: start, to: end, color: color)
        }
    }

    private static let allowedSpaces: [Mark.Part: Set<Mark.Space>] = [
        .head: [.head, .skull, .ear], .ear: [.ear], .body: [.body], .ruff: [.body], .tail: [.tail],
        .farPaw: [.body, .foot], .nearPaw: [.body, .foot], .paws: [.body, .foot],
    ]

    private static func mark(_ fields: Fields, body: String) throws -> Mark {
        let at = fields.prefix
        guard let partName = try fields.string("part") else { throw SkinError("\(at)part: required") }
        let part = try fields.choice(Mark.Part(rawValue: partName), "part", partName)
        guard let spaceName = try fields.string("space") else { throw SkinError("\(at)space: required") }
        let space = try fields.choice(Mark.Space(rawValue: spaceName), "space", spaceName)
        if !(allowedSpaces[part] ?? []).contains(space) {
            let allowed = (allowedSpaces[part] ?? []).map(\.rawValue).sorted().joined(separator: ", ")
            throw SkinError("\(at)space: part \"\(partName)\" takes space \(allowed)")
        }
        if body == "shorthair" && (part == .ear || part == .ruff || part == .tail) {
            throw SkinError("\(at)part: \"\(partName)\" exists only on longhair bodies")
        }
        guard let color = try fields.color("color") else { throw SkinError("\(at)color: required") }
        guard let pathText = try fields.string("path") else { throw SkinError("\(at)path: required") }
        let path = try parsePath(pathText, at: "\(at)path")
        let morph = { (key: String) -> [PathCommand]? in
            guard let text = try fields.string(key) else { return nil }
            let target = try parsePath(text, at: "\(at)\(key)")
            if target.count != path.count || zip(target, path).contains(where: { !$0.sameKind(as: $1) }) {
                throw SkinError("\(at)\(key): must use the same commands as path, only with different numbers")
            }
            return target
        }
        let mirror = try fields.bool("mirror") ?? false
        if mirror && !(space == .head || space == .skull || space == .ear) {
            throw SkinError("\(at)mirror: only head, skull and ear spaces can be mirrored")
        }
        let side = try fields.string("side").map { try fields.choice(Mark.Side(rawValue: $0), "side", $0) } ?? .right
        return Mark(part: part, space: space, color: color, path: path, asleep: try morph("asleep"),
                    braced: try morph("braced"), mirror: mirror,
                    depth: (try fields.number("depth")).map { min(max($0, 0), 30) } ?? 14, side: side)
    }

    // SVG path data with M, L, H, V, C, Q and Z, absolute or relative. Coordinates are clamped to ±500.
    static func parsePath(_ text: String, at: String) throws -> [PathCommand] {
        if text.count > 8000 { throw SkinError("\(at): longer than 8000 characters") }
        var tokens: [String] = []
        var number = ""
        let flush = { if !number.isEmpty { tokens.append(number); number = "" } }
        var previous: Character = " "
        for character in text {
            if character.isLetter && character != "e" && character != "E" {
                flush()
                tokens.append(String(character))
            } else if character == "," || character.isWhitespace {
                flush()
            } else if character == "-" || character == "+" {
                if previous != "e" && previous != "E" { flush() }
                number.append(character)
            } else if character == "." && number.contains(".") && !number.contains("e") && !number.contains("E") {
                flush()
                number.append(character)
            } else {
                number.append(character)
            }
            previous = character
        }
        flush()

        var commands: [PathCommand] = []
        var index = 0
        var current = CGPoint.zero, start = CGPoint.zero
        var command: Character?
        let limit = coordinateLimit
        func value() throws -> CGFloat {
            guard index < tokens.count, let parsed = Double(tokens[index]), parsed.isFinite else {
                throw SkinError("\(at): expected a number\(index < tokens.count ? " but found \"\(tokens[index])\"" : " at the end")")
            }
            index += 1
            return min(max(CGFloat(parsed), -limit), limit)
        }
        func point(_ relative: Bool) throws -> CGPoint {
            let x = try value(), y = try value()
            return relative ? CGPoint(x: current.x + x, y: current.y + y) : CGPoint(x: x, y: y)
        }
        while index < tokens.count {
            if let letter = tokens[index].first, tokens[index].count == 1, letter.isLetter {
                command = letter
                index += 1
            } else if command == nil {
                throw SkinError("\(at): must start with M")
            }
            guard let active = command else { break }
            let relative = active.isLowercase
            switch active.uppercased() {
            case "M":
                current = try point(relative)
                start = current
                commands.append(.move(current))
                command = relative ? "l" : "L"
            case "L":
                current = try point(relative)
                commands.append(.line(current))
            case "H":
                current = CGPoint(x: (relative ? current.x : 0) + (try value()), y: current.y)
                commands.append(.line(current))
            case "V":
                current = CGPoint(x: current.x, y: (relative ? current.y : 0) + (try value()))
                commands.append(.line(current))
            case "C":
                let c1 = try point(relative), c2 = try point(relative), end = try point(relative)
                current = end
                commands.append(.cubic(c1, c2, end))
            case "Q":
                let control = try point(relative), end = try point(relative)
                current = end
                commands.append(.quad(control, end))
            case "Z":
                commands.append(.close)
                current = start
                command = nil
            default:
                throw SkinError("\(at): unsupported command \"\(active)\" (use M, L, H, V, C, Q, Z)")
            }
            if commands.count > 400 { throw SkinError("\(at): more than 400 commands") }
        }
        guard case .move = commands.first else { throw SkinError("\(at): must start with M") }
        return commands
    }
}

// Typed access to one JSON object, naming the offending field in errors. Unknown keys are ignored.
private struct Fields {
    let object: [String: Any]
    let prefix: String

    init(_ object: [String: Any], at prefix: String) {
        self.object = object
        self.prefix = prefix
    }

    func string(_ key: String) throws -> String? {
        guard let value = object[key] else { return nil }
        guard let text = value as? String else { throw SkinError("\(prefix)\(key): must be a string") }
        return text
    }

    func number(_ key: String) throws -> CGFloat? {
        guard let value = object[key] else { return nil }
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.isFinite else {
            throw SkinError("\(prefix)\(key): must be a number")
        }
        return CGFloat(number.doubleValue)
    }

    func bool(_ key: String) throws -> Bool? {
        guard let value = object[key] else { return nil }
        guard let number = value as? NSNumber, CFGetTypeID(number) == CFBooleanGetTypeID() else {
            throw SkinError("\(prefix)\(key): must be true or false")
        }
        return number.boolValue
    }

    func array(_ key: String) throws -> [Any]? {
        guard let value = object[key] else { return nil }
        guard let list = value as? [Any] else { throw SkinError("\(prefix)\(key): must be an array") }
        return list
    }

    // "#RRGGBB" or "#RRGGBBAA".
    func color(_ key: String) throws -> Argb? {
        guard let text = try string(key) else { return nil }
        let hex = text.hasPrefix("#") ? String(text.dropFirst()) : ""
        guard hex.count == 6 || hex.count == 8, let value = UInt32(hex, radix: 16) else {
            throw SkinError("\(prefix)\(key): expected a colour like \"#RRGGBB\" or \"#RRGGBBAA\", got \"\(text)\"")
        }
        let rgba = hex.count == 6 ? value << 8 | 0xFF : value
        return Argb(CGFloat(rgba & 0xFF), CGFloat(rgba >> 24 & 0xFF), CGFloat(rgba >> 16 & 0xFF), CGFloat(rgba >> 8 & 0xFF))
    }

    func choice<T>(_ value: T?, _ key: String, _ raw: String) throws -> T {
        guard let value else { throw SkinError("\(prefix)\(key): unknown value \"\(raw)\"") }
        return value
    }
}
