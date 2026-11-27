import CoreGraphics
import Foundation

enum CatGeometry {
    static let width = 224
    static let height = 190
    static let topPadding = 24
    static let cornerX = 152
    static let cornerY = 72
    static let foldedRight = 167
}

let dreamLifetime: CGFloat = 1.8
let dreamStagger: CGFloat = 0.32
let chatterMarkLifetime: CGFloat = 0.48
let chatterMarkStagger: CGFloat = 0.08
let petSymbolLifetime: CGFloat = 1.55
let petSymbolStagger: CGFloat = 0.23

struct Pose {
    var cursor: CGPoint
    var swing: CGFloat = 0
    var curl: CGFloat = 0
    var eyeOpen: CGFloat = 1
    var tailDirection: CGFloat = 1
    var alert: CGFloat = 1
    var tailMotion: CGFloat = 1
    var dreamAge: CGFloat = 0
    var dreamCount = 0
    var hunt: CGFloat = 0
    var brace: CGFloat = 0
    var tailTwitch: CGFloat = 0
    var chatter: CGFloat = 0
    var chatterAge: CGFloat = -1
    var earTwitch: CGFloat = 0
    var chatterCode = ""
    var squint: CGFloat = 0
    var petSymbolAge: CGFloat = 0
    var petSymbolCount = 0
    var pawTuckLeft: CGFloat = 0
    var pawTuckRight: CGFloat = 0
    var tailWave: CGFloat = 0
    var tailWavePhase: CGFloat = 0
    var petIsNote = false

    static let awake = Pose(cursor: CatRenderer.faceCenter)
}

// Components on GDI+'s 0-255 scale, so colours copy over from cat.cpp verbatim.
struct Argb {
    var a, r, g, b: CGFloat

    init(_ a: CGFloat, _ r: CGFloat, _ g: CGFloat, _ b: CGFloat) {
        self.a = a; self.r = r; self.g = g; self.b = b
    }

    var cg: CGColor {
        CGColor(srgbRed: r / 255, green: g / 255, blue: b / 255, alpha: a / 255)
    }
}

private func pt(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: x, y: y) }

// GDI+ cardinal spline: tangents are (next - previous) * tension / 3, clamped at the ends.
private func cardinalControls(_ p: [CGPoint], _ i: Int, _ tension: CGFloat) -> (CGPoint, CGPoint) {
    let k = tension / 3
    let before = p[max(0, i - 1)], after = p[min(p.count - 1, i + 2)]
    return (pt(p[i].x + k * (p[i + 1].x - before.x), p[i].y + k * (p[i + 1].y - before.y)),
            pt(p[i + 1].x - k * (after.x - p[i].x), p[i + 1].y - k * (after.y - p[i].y)))
}

private extension CGMutablePath {
    // An open figure continues from its last point with a line, as in GraphicsPath.
    func continueFigure(at start: CGPoint) {
        if isEmpty { move(to: start) } else if currentPoint != start { addLine(to: start) }
    }

    // Matches GraphicsPath::AddBeziers: p0, then (c1, c2, end) triples.
    func addBeziers(_ p: [CGPoint]) {
        continueFigure(at: p[0])
        var i = 1
        while i + 2 < p.count {
            addCurve(to: p[i + 2], control1: p[i], control2: p[i + 1])
            i += 3
        }
    }

    func addCardinal(_ p: [CGPoint], tension: CGFloat, from first: Int = 0, segments: Int? = nil) {
        continueFigure(at: p[first])
        for i in first..<(first + (segments ?? p.count - 1 - first)) {
            let (c1, c2) = cardinalControls(p, i, tension)
            addCurve(to: p[i + 1], control1: c1, control2: c2)
        }
    }
}

private func outline(_ p: [CGPoint]) -> CGMutablePath {
    let path = CGMutablePath()
    path.addBeziers(p)
    path.closeSubpath()
    return path
}

// GraphicsPath defaults to FillModeAlternate; only the longhair plume winds.
private func fill(_ g: CGContext, _ color: Argb, _ path: CGPath, winding: Bool = false) {
    g.addPath(path)
    g.setFillColor(color.cg)
    g.fillPath(using: winding ? .winding : .evenOdd)
}

private func clip(_ g: CGContext, to path: CGPath, winding: Bool = false) {
    g.addPath(path)
    g.clip(using: winding ? .winding : .evenOdd)
}

private func clip(_ g: CGContext, outside path: CGPath) {
    let outside = CGMutablePath()
    outside.addRect(CGRect(x: -10_000, y: -10_000, width: 20_000, height: 20_000))
    outside.addPath(path)
    g.addPath(outside)
    g.clip(using: .evenOdd)
}

private func strokePath(_ g: CGContext, _ path: CGPath, _ color: Argb, _ width: CGFloat,
                        cap: CGLineCap = .butt, join: CGLineJoin = .miter) {
    g.addPath(path)
    g.setStrokeColor(color.cg)
    g.setLineWidth(width)
    g.setLineCap(cap)
    g.setLineJoin(join)
    g.strokePath()
}

private func stroke(_ g: CGContext, _ points: [CGPoint], _ color: Argb, _ thickness: CGFloat) {
    let path = CGMutablePath()
    path.addBeziers(points)
    strokePath(g, path, color, thickness, cap: .round)
}

private func degrees(_ value: CGFloat) -> CGFloat { value * .pi / 180 }

private struct Gaze {
    var x: CGFloat
    var y: CGFloat
    var yaw: CGFloat
    var pitch: CGFloat

    init(cursor: CGPoint, center: CGPoint, alert: CGFloat) {
        let dx = cursor.x - center.x, dy = cursor.y - center.y
        let length = (dx * dx + dy * dy + 80 * 80).squareRoot()
        x = dx / length * alert
        y = dy / length * alert
        yaw = x * 0.31
        pitch = y * 0.25
    }

    // Project features from the front of a shallow head volume onto the drawing plane.
    func project(_ localX: CGFloat, _ localY: CGFloat, _ depth: CGFloat) -> CGPoint {
        let turnedDepth = depth * cos(yaw) - localX * sin(yaw)
        return pt(localX * cos(yaw) + depth * sin(yaw), localY * cos(pitch) + turnedDepth * sin(pitch))
    }
}

private struct HeadPosition {
    var gaze: Gaze
    var center: CGPoint
    var angle: CGFloat
    var settle: CGFloat
}

private struct PawPress {
    let contact: CGPoint
    let direction: CGFloat
    let width: CGFloat
    let depth: CGFloat
    let spread: CGFloat
    let sine: CGFloat
    let cosine: CGFloat

    init(contact: CGPoint, direction: CGFloat, press: CGFloat) {
        self.contact = contact
        self.direction = direction
        width = 1 + 0.14 * press
        depth = 1 - 0.28 * press
        spread = direction * 4.8 * press
        sine = sin(-direction * 0.44 * press)
        cosine = cos(0.44 * press)
    }

    func callAsFunction(_ x: CGFloat, _ y: CGFloat) -> CGPoint {
        let px = (x - contact.x) * width, py = (y - contact.y) * depth
        return pt(contact.x + px * cosine - py * sine + spread, contact.y + px * sine + py * cosine)
    }

    // The foot space of skin marks: offsets from the contact point, x pointing away from the body's centre.
    func toe(_ x: CGFloat, _ y: CGFloat) -> CGPoint { self(contact.x + direction * x, contact.y + y) }
}

// How a mark's points morph with the pose and land in the drawing.
private struct MarkFrame {
    var settle: CGFloat
    var brace: CGFloat
    var place: (Mark, _ mirrored: Bool, CGPoint) -> CGPoint
}

enum CatRenderer {
    static let faceCenter = pt(56, 53)
    private static let fullHeadLift: CGFloat = 20
    private static let tailSegments = 24
    private static let farContact = pt(70, 90)
    private static let nearContact = pt(107, 93)

    // Expects a y-down context in cat-local units, already offset by the top padding.
    // A front paw the skin leaves out stays fully tucked. The far paw is the cat's right, the near paw its left.
    private static func pawTucks(_ pose: Pose, _ spec: SkinSpec) -> (far: CGFloat, near: CGFloat) {
        (spec.missingFrontPaws.contains(.right) ? 1 : pose.pawTuckLeft,
         spec.missingFrontPaws.contains(.left) ? 1 : pose.pawTuckRight)
    }

    static func draw(in g: CGContext, pose: Pose, spec: SkinSpec, headroom: CGFloat) {
        g.setShouldAntialias(true)
        let tucks = pawTucks(pose, spec)
        let brace = upperBodyBrace(pose, headroom)
        let body = bodyOutline(brace, spec)
        drawTail(g, pose, spec, brace)
        let farPaw = drawFarFrontPaw(g, spec, lift: brace,
                                     foot: PawPress(contact: farContact, direction: -1, press: pose.brace),
                                     tuck: tucks.far)
        drawBody(g, body, farPaw, spec, brace)
        drawNearFrontPaw(g, body, spec, lift: brace, press: pose.brace, tuck: tucks.near)
        if spec.isLonghair { drawLonghairRuff(g, brace, spec) }
        drawHead(g, pose, spec, lift: brace * fullHeadLift)
        drawDreams(g, pose)
        drawPetSymbols(g, pose)
    }

    // 32 x 32 head for the menu bar, as DrawTrayIcon.
    static func drawTrayIcon(in g: CGContext, spec: SkinSpec) {
        g.saveGState()
        defer { g.restoreGState() }
        g.setShouldAntialias(true)
        let scale: CGFloat = spec.isLonghair ? 0.33 : 0.36
        g.scaleBy(x: scale, y: scale)
        g.translateBy(x: spec.isLonghair ? -11 : -15.5, y: -2)
        drawHead(g, Pose(cursor: faceCenter), spec, lift: 0)
    }

    static func hitsPettingArea(_ cursor: CGPoint, _ pose: Pose, spec: SkinSpec, headroom: CGFloat) -> Bool {
        if cursor.x < 0 || cursor.x > CGFloat(CatGeometry.foldedRight) + 4 || cursor.y < -headroom || cursor.y > 134 {
            return false
        }
        let brace = upperBodyBrace(pose, headroom)
        let head = positionHead(pose, lift: brace * fullHeadLift, longhair: spec.isLonghair)
        let angle = degrees(head.angle)
        let dx = cursor.x - head.center.x, dy = cursor.y - head.center.y
        let local = pt(cos(angle) * dx + sin(angle) * dy, -sin(angle) * dx + cos(angle) * dy)
        if abs(local.x) <= 44 && local.y >= -48 && local.y <= 29 {
            if spec.isLonghair {
                if longhairFaceOutline(head.settle, spec.face).contains(local, using: .evenOdd) { return true }
                for side: CGFloat in [-1, 1]
                where longhairEarOutline(side, pose.earTwitch, spec.ears).contains(local, using: .evenOdd) {
                    return true
                }
            } else if shorthairHeadOutline(pose, settle: head.settle).contains(local, using: .evenOdd) {
                return true
            }
        }
        if cursor.x < 28 || cursor.y < 10 { return false }

        if bodyOutline(brace, spec).contains(cursor, using: .evenOdd) { return true }
        if cursor.y > 98 { return false }
        let tucks = pawTucks(pose, spec)
        if tucks.far < 0.5
            && farFrontPawOutline(brace, PawPress(contact: farContact, direction: -1, press: pose.brace))
                .contains(cursor, using: .evenOdd) {
            return true
        }
        if tucks.near < 0.5
            && nearFrontPawOutline(brace, PawPress(contact: nearContact, direction: 1, press: pose.brace))
                .contains(cursor, using: .evenOdd) {
            return true
        }
        return spec.isLonghair && longhairRuffOutline(brace).contains(cursor, using: .evenOdd)
    }

    private static func upperBodyBrace(_ pose: Pose, _ headroom: CGFloat) -> CGFloat {
        // The resting ear silhouette already leaves a few pixels above it.
        pose.brace * min(1, (headroom + 6) / fullHeadLift)
    }

    private static func drawFurOutline(_ g: CGContext, _ path: CGPath, _ spec: SkinSpec) {
        guard let ink = spec.outline else { return }
        strokePath(g, path, ink, spec.isLonghair ? 1 : 0.8, join: .round)
    }

    // MARK: Skin marks

    private static func drawMarks(_ g: CGContext, _ marks: [Mark], _ frame: MarkFrame,
                                  only include: (Mark, _ mirrored: Bool) -> Bool = { _, _ in true }) {
        for mark in marks {
            // Mirrored marks draw the left copy first, as the hand-written skins did.
            for mirrored in mark.mirror ? [true, false] : [false] where include(mark, mirrored) {
                fill(g, mark.color, markPath(mark, mirrored: mirrored, frame))
            }
        }
    }

    private static func markPath(_ mark: Mark, mirrored: Bool, _ frame: MarkFrame) -> CGMutablePath {
        let path = CGMutablePath()
        for (index, command) in mark.path.enumerated() {
            let asleep = mark.asleep?[index].points, braced = mark.braced?[index].points
            let points = command.points.enumerated().map { j, base -> CGPoint in
                var p = base
                if let target = asleep?[j] {
                    p.x += frame.settle * (target.x - base.x)
                    p.y += frame.settle * (target.y - base.y)
                }
                if let target = braced?[j] {
                    p.x += frame.brace * (target.x - base.x)
                    p.y += frame.brace * (target.y - base.y)
                }
                if mirrored { p.x = -p.x }
                return frame.place(mark, mirrored, p)
            }
            switch command {
            case .move: path.move(to: points[0])
            case .line: path.addLine(to: points[0])
            case .quad: path.addQuadCurve(to: points[1], control: points[0])
            case .cubic: path.addCurve(to: points[2], control1: points[0], control2: points[1])
            case .close: path.closeSubpath()
            }
        }
        return path
    }

    private static func earSide(_ mark: Mark, mirrored: Bool) -> CGFloat {
        mirrored || (!mark.mirror && mark.side == .left) ? -1 : 1
    }

    private static func marks(_ spec: SkinSpec, _ parts: Set<Mark.Part>) -> [Mark] {
        spec.marks.filter { parts.contains($0.part) }
    }

    // MARK: Tail

    private static func drawTail(_ g: CGContext, _ pose: Pose, _ spec: SkinSpec, _ brace: CGFloat) {
        let longhair = spec.isLonghair
        let step: CGFloat = (longhair ? 68 : 59) / CGFloat(tailSegments)
        let restingAngle: CGFloat = pose.tailDirection < 0 ? 0.86 : 0.22
        let raisedAngle: CGFloat = 1.77
        var points = [longhair ? pt(152, 100) : pt(154, 122)]
        for i in 1...tailSegments {
            let u = (CGFloat(i) - 0.5) / CGFloat(tailSegments)
            let blend = min(1, 5.5 * u)
            let wave = pose.tailWave * 0.75 * pow(u, 0.7) * sin(pose.tailWavePhase - 6.5 * u)
            let normalAngle = restingAngle * pose.tailMotion + pose.swing * (0.32 + 0.77 * u)
                + 7.3 * pose.curl * u * u.squareRoot() + wave
            let tip = max(0, (u - 0.65) / 0.35)
            let straightAngle = raisedAngle - 0.22 * (1 - blend)
            var angle = blend * normalAngle * (1 - pose.brace) + straightAngle * pose.brace
            if longhair {
                let turn = u * u * (3 - 2 * u)
                let asleep = 0.07 + 0.49 * turn
                let awake = 0.62 + 2.34 * turn
                let rest = asleep + (awake - asleep) * pose.tailMotion
                let curl = rest + pose.swing * (0.28 + 0.36 * u) + 1.3 * pose.curl * u + wave
                angle = blend * curl * (1 - pose.brace) + (straightAngle + 0.53 * turn) * pose.brace
            }
            angle += pose.tailTwitch * tip * tip
            let previous = points[i - 1]
            points.append(pt(previous.x + pose.tailDirection * step * sin(angle), previous.y + step * cos(angle)))
        }

        if longhair {
            drawLonghairTail(g, points, inward: pose.tailDirection < 0, spec, brace)
            return
        }

        let tension: CGFloat = 0.38
        let tail = CGMutablePath()
        tail.addCardinal(points, tension: tension)
        strokePath(g, tail, spec.tail, 9.2, cap: .round)
        for band in spec.tailBands {
            let stripe = CGMutablePath()
            stripe.addCardinal(points, tension: tension, from: band.from, segments: band.to - band.from)
            strokePath(g, stripe, band.color, 9.2)
            if band.to == tailSegments {
                // Only the far end of a band that reaches the tip is rounded.
                g.setFillColor(band.color.cg)
                g.fillEllipse(in: CGRect(x: points[band.to].x - 4.6, y: points[band.to].y - 4.6, width: 9.2, height: 9.2))
            }
        }
    }

    private static func drawLonghairTail(_ g: CGContext, _ spine: [CGPoint], inward: Bool, _ spec: SkinSpec,
                                         _ brace: CGFloat) {
        let n = tailSegments
        var front = [CGPoint](repeating: .zero, count: n + 1)
        var back = front
        for i in 0...n {
            let previous = spine[max(0, i - 1)], next = spine[min(n, i + 1)]
            let dx = next.x - previous.x, dy = next.y - previous.y
            let length = hypot(dx, dy)
            let normal = pt(-dy / length, dx / length)
            let t = CGFloat(i) / CGFloat(n)
            // A broad middle and a swept, tapered tip trace one continuous brush of fur.
            let radius = (1 - t) * 6.5 + 12.8 * sin(.pi * t)
            let outerLock = max(0, 1 - abs(CGFloat(i) - 16) / (i < 16 ? 5 : 1))
            let innerLock = max(0, 1 - abs(CGFloat(i) - 19) / (i < 19 ? 5 : 1))
            let outer = radius * (1 + 0.14 * t) + 1.8 * outerLock
            let inner = radius * (1 - 0.18 * t) + 1.5 * innerLock
            front[i] = pt(spine[i].x + outer * normal.x, spine[i].y + outer * normal.y)
            back[n - i] = pt(spine[i].x - inner * normal.x, spine[i].y - inner * normal.y)
        }
        // Tail space: x runs along the spine in segments 0...24, y across it, ±1 at the plume's edges.
        let tuft = { (_: Mark, _: Bool, p: CGPoint) -> CGPoint in
            let s = min(max(p.x, 0), CGFloat(n))
            let i = min(Int(s), n - 1), f = s - CGFloat(i)
            let lerp = { (a: CGPoint, b: CGPoint) in pt(a.x + f * (b.x - a.x), a.y + f * (b.y - a.y)) }
            let center = lerp(spine[i], spine[i + 1])
            let edge = p.y >= 0 ? lerp(front[i], front[i + 1]) : lerp(back[n - i], back[n - i - 1])
            let amount = abs(p.y)
            return pt(center.x + amount * (edge.x - center.x), center.y + amount * (edge.y - center.y))
        }
        // Follow the spine through the bend so the outer arc stays soft even at full curl.
        let plume = CGMutablePath()
        plume.addCardinal(front, tension: 0.42)
        plume.addCardinal(back, tension: 0.42)
        plume.closeSubpath()

        g.saveGState()
        defer { g.restoreGState() }
        // The plume is wider than the other tails, so reserve room for its edge when folded inward.
        if inward {
            let right = plume.boundingBoxOfPath.maxX
            g.translateBy(x: -max(0, right - (CGFloat(CatGeometry.foldedRight) - 1.5)), y: 0)
        }
        fill(g, spec.tail, plume, winding: true)
        g.saveGState()
        clip(g, to: plume, winding: true)
        drawMarks(g, marks(spec, [.tail]), MarkFrame(settle: 0, brace: brace, place: tuft))
        g.restoreGState()
        drawFurOutline(g, plume, spec)
    }

    // MARK: Head

    // A longhair's fluffy body hangs lower than a shorthair's, so its sleeping head sinks further to lie level with
    // the belly, like a loaf on the ledge.
    private static let longhairSleepDrop: CGFloat = 3

    private static func positionHead(_ pose: Pose, lift: CGFloat, longhair: Bool) -> HeadPosition {
        let center = pt(faceCenter.x + lift * 0.12, faceCenter.y - lift)
        let gaze = Gaze(cursor: pose.cursor, center: center, alert: pose.alert)
        let droop = 1 - pose.alert
        let settle = droop * droop
        return HeadPosition(gaze: gaze,
                            center: pt(center.x + gaze.x * 1.6 + settle * 5.5,
                                       center.y + gaze.y * 1.25 + droop * (5.8 + (longhair ? longhairSleepDrop : 0))),
                            angle: -4 * gaze.x - 17 * droop, settle: settle)
    }

    private static func earPoint(_ side: CGFloat, _ x: CGFloat, _ y: CGFloat, _ twitch: CGFloat) -> CGPoint {
        let flick = max(0, side * twitch)
        let tip = min(max((-y - 22) / 17, 0), 1)
        return pt(x + side * 5.3 * flick * tip, y + 3.3 * flick * tip)
    }

    // How much wider a sleeping shorthair's lower cheek spreads than the original squash.
    private static let cheekSpread: CGFloat = 1.8

    private static func easeStep(_ t: CGFloat) -> CGFloat {
        let u = min(max(t, 0), 1)
        return u * u * (3 - 2 * u)
    }


    // Asleep, the head tilts down to the left, but the ledge holds up the fur of the lower left cheek: whatever
    // would sink below the chin's level is pressed back up to it and spreads sideways, so that edge stays level.
    private static func longhairCheek(_ x: CGFloat, _ y: CGFloat, _ settle: CGFloat) -> CGPoint {
        let contact = max(y, 0) * settle
        let p = pt(x - contact * 0.08, y - contact * 0.19)
        let droop = settle.squareRoot()
        let tilt = CGAffineTransform(rotationAngle: degrees(-17 * droop))
        var upright = p.applying(tilt)
        // The ledge meets the cheek at the level of this point on the lower left of the face.
        let below = upright.y - pt(-28, 14).applying(tilt).y
        guard below > 0, droop > 0 else { return p }
        // Only the left cheek rests on the ledge; the chin and the right side keep their shape.
        let press = below * 0.85 * droop * easeStep((-x - 8) / 14)
        upright.y -= press
        upright.x -= press * 0.6
        return upright.applying(tilt.inverted())
    }

    private static func headFrame(_ pose: Pose, _ head: HeadPosition) -> MarkFrame {
        MarkFrame(settle: head.settle, brace: 0) { mark, mirrored, p in
            switch mark.space {
            case .skull: longhairCheek(p.x, p.y, head.settle)
            case .ear: earPoint(earSide(mark, mirrored: mirrored), p.x, p.y, pose.earTwitch)
            default: head.gaze.project(p.x, p.y, mark.depth)
            }
        }
    }

    private static func drawHead(_ g: CGContext, _ pose: Pose, _ spec: SkinSpec, lift: CGFloat) {
        let head = positionHead(pose, lift: lift, longhair: spec.isLonghair)
        let gaze = head.gaze
        let frame = headFrame(pose, head)
        g.saveGState()
        defer { g.restoreGState() }
        g.translateBy(x: head.center.x, y: head.center.y)
        g.rotate(by: degrees(head.angle))
        let silhouette: CGMutablePath
        if spec.isLonghair {
            for side: CGFloat in [-1, 1] {
                let ear = longhairEarOutline(side, pose.earTwitch, spec.ears)
                fill(g, spec.ear, ear)
                drawFurOutline(g, ear, spec)
                g.saveGState()
                clip(g, to: ear)
                drawMarks(g, marks(spec, [.ear]), frame) { mark, mirrored in earSide(mark, mirrored: mirrored) == side }
                g.restoreGState()
            }
            silhouette = longhairFaceOutline(head.settle, spec.face)
            fill(g, spec.faceColor, silhouette)
        } else {
            silhouette = shorthairHeadOutline(pose, settle: head.settle)
            fill(g, spec.coat, silhouette)
        }
        g.saveGState()
        clip(g, to: silhouette)
        drawMarks(g, marks(spec, [.head]), frame)
        g.restoreGState()
        if spec.isLonghair {
            if let ink = spec.faceOutline { strokePath(g, silhouette, ink, spec.faceOutlineWidth, join: .round) }
        } else {
            drawFurOutline(g, silhouette, spec)
        }

        drawChatterMouth(g, gaze, pose, spec)
        for side: CGFloat in [-1, 1] {
            drawEye(g, side, gaze, pose, spec)
            if spec.isLonghair {
                for row: CGFloat in [0, 1] {
                    let w = { (x: CGFloat, y: CGFloat) in gaze.project(side * x, y, 10) }
                    stroke(g, [w(15 + row, 9 + 3 * row), w(20, 8 + 3 * row), w(26, 7 + 4 * row),
                               w(32 - row, 8 + 5 * row)], spec.whisker, 0.65)
                }
            } else {
                drawWhiskers(g, side, gaze, cheekPress: side < 0 ? head.settle : 0, spec.whisker)
            }
        }
        drawChatterMarks(g, pose)
    }

    private static func drawEye(_ g: CGContext, _ side: CGFloat, _ gaze: Gaze, _ pose: Pose, _ spec: SkinSpec) {
        let center = gaze.project(side * 13.6, -1, 15)
        let radiusX = 9.7 * (1 - side * gaze.x * 0.17)
        let radiusY = 9.1 * (1 - abs(gaze.y) * 0.14)
        g.saveGState()
        defer { g.restoreGState() }
        g.translateBy(x: center.x, y: center.y)
        g.rotate(by: degrees(side * 3.4 + gaze.x * 2))

        if pose.eyeOpen < 0.18 {
            let lidAlert = spec.isLonghair && pose.squint > 0 ? 0 : pose.alert
            let tint = { (resting: CGFloat, awake: CGFloat) in (resting + (awake - resting) * lidAlert).rounded(.down) }
            let lid = Argb(255, tint(spec.eyelid.r, spec.iris.r), tint(spec.eyelid.g, spec.iris.g),
                           tint(spec.eyelid.b, spec.iris.b))
            let lidWidth = radiusX * (0.77 + 0.23 * pose.alert)
            let lidCurve = 2.1 - 2.8 * pose.squint
            stroke(g, [pt(-lidWidth, 0), pt(-lidWidth * 0.48, lidCurve), pt(lidWidth * 0.43, lidCurve), pt(lidWidth, 0)],
                   lid, 1.35 + 0.4 * pose.squint)
            return
        }
        g.scaleBy(x: 1, y: pose.eyeOpen)
        g.setFillColor(spec.iris.cg)
        g.fillEllipse(in: CGRect(x: -radiusX, y: -radiusY, width: 2 * radiusX, height: 2 * radiusY))

        let pupilX = gaze.x * (2.1 - 1.0 * pose.hunt)
        let pupilY = gaze.y * (1.65 - 0.72 * pose.hunt)
        let huntPupil = min(radiusX, radiusY) - hypot(pupilX, pupilY) - 1.65
        let slitX = radiusX * 0.24, slitY = radiusY * 0.72
        let pupilRadiusX = slitX + (huntPupil - slitX) * pose.hunt
        let pupilRadiusY = slitY + (huntPupil - slitY) * pose.hunt
        g.setFillColor(spec.pupil.cg)
        g.fillEllipse(in: CGRect(x: pupilX - pupilRadiusX, y: pupilY - pupilRadiusY,
                                 width: 2 * pupilRadiusX, height: 2 * pupilRadiusY))
        let shine = max(spec.eyeShine, (pose.hunt - 0.20) / 0.80)
        if shine > 0 {
            let size = 2.7 * shine
            g.setFillColor(Argb((237 * shine).rounded(.down), 255, 255, 247).cg)
            g.fillEllipse(in: CGRect(x: pupilX - 0.47 * pupilRadiusX, y: pupilY - 0.56 * pupilRadiusY,
                                     width: size, height: size))
        }
    }

    private static func drawWhiskers(_ g: CGContext, _ side: CGFloat, _ gaze: Gaze, cheekPress: CGFloat,
                                     _ whisker: Argb) {
        for row in 0..<3 {
            let r = CGFloat(row)
            let y = 9 + r * 3.2 - cheekPress * (1.2 + 1.9 * r)
            let lift = 2.2 - r * (2.25 - 0.32 * cheekPress)
            stroke(g, [gaze.project(side * 18.7, y, 13),
                       gaze.project(side * 22.2, y - lift * 0.6, 11),
                       gaze.project(side * 25.4, y - lift, 9),
                       gaze.project(side * (29.1 - r * 0.4 + 1.2 * cheekPress), y - lift, 7)],
                   whisker, 0.85)
        }
    }

    private static func shorthairHeadOutline(_ pose: Pose, settle: CGFloat) -> CGMutablePath {
        let squash = settle * cheekSpread
        let cheekEdge = -31 - 1.5 * squash
        // The two lowest curves flatten and widen the cheek as the head settles onto the ledge.
        let ear = { (side: CGFloat, x: CGFloat, y: CGFloat) in earPoint(side, x, y, pose.earTwitch) }
        let path = CGMutablePath()
        path.addBeziers([pt(cheekEdge, 4), pt(-32, -5), pt(-28, -12), pt(-24, -17),
                         ear(-1, -25, -24), ear(-1, -24, -34), ear(-1, -22, -39),
                         ear(-1, -21, -42), ear(-1, -18, -41), ear(-1, -16, -38),
                         ear(-1, -12, -33), ear(-1, -8.5, -26), pt(-5.5, -22),
                         pt(0, -23.5), pt(7, -23.8), pt(12, -22),
                         ear(1, 16, -28), ear(1, 23, -35), ear(1, 28, -37),
                         ear(1, 31, -38.4), ear(1, 33, -36.4), ear(1, 33, -33),
                         ear(1, 33, -26), pt(32, -20), pt(30, -15), pt(35, -10), pt(38, -3), pt(38, 5)])
        path.addBeziers([pt(38, 5), pt(38.5, 14), pt(33, 21.5), pt(25, 24),
                         pt(10, 28 - 0.5 * settle), pt(-9 - 5 * squash, 27 - 8.3 * settle),
                         pt(-20 - 3 * squash, 22 - 6 * settle), pt(-27 - 4 * squash, 19 - 5.5 * settle),
                         pt(-31 - 6 * squash, 13 - 4 * settle), pt(cheekEdge, 4)])
        path.closeSubpath()
        return path
    }

    private static func longhairEarOutline(_ side: CGFloat, _ twitch: CGFloat, _ ears: SkinSpec.Ears) -> CGMutablePath {
        let p = { (x: CGFloat, y: CGFloat) in earPoint(side, x * side, y, twitch) }
        if ears == .tufted {
            return outline([p(10, -20), p(14, -27), p(23, -35), p(26, -39),
                            p(27, -41), p(28, -43), p(28, -45),
                            p(29, -44), p(29, -42), p(29, -41),
                            p(31, -43), p(32, -46), p(32, -47),
                            p(33, -42), p(31, -39), p(31, -36),
                            p(32, -29), p(32, -21), p(29, -13),
                            p(23, -14), p(16, -18), p(10, -20)])
        }
        return outline([p(10, -20), p(14, -27), p(24, -39), p(28, -39),
                        p(32, -38), p(32, -24), p(29, -13),
                        p(23, -14), p(16, -18), p(10, -20)])
    }

    private static func longhairFaceOutline(_ settle: CGFloat, _ face: SkinSpec.Face) -> CGMutablePath {
        let c = { (x: CGFloat, y: CGFloat) in longhairCheek(x, y, settle) }
        if face == .round {
            // Full, rounded cheeks with just two short locks at the sides.
            return outline([c(0, -28), c(18, -28), c(31, -18), c(35, -5),
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
                            c(-31, -18), c(-18, -28), c(0, -28)])
        }
        return outline([c(-33, -9), c(-29, -21), c(-15, -28), c(-2, -28),
                        c(14, -29), c(27, -22), c(32, -14),
                        c(35, -10), c(36, -7), c(39, -5),
                        c(38, -5), c(36, -5), c(35, -5),
                        c(38, -2), c(39, 4), c(42, 9),
                        c(40, 10), c(37, 9), c(35, 8),
                        c(38, 15), c(35, 21), c(32, 24),
                        c(32, 21), c(31, 19), c(30, 18),
                        // One smooth chin: a dip in the middle read as a "w" that looked pinched when the head
                        // tilted in sleep.
                        c(27, 22.5), c(22, 26), c(14, 27.3),
                        c(9, 28.1), c(4.5, 28.4), c(0, 28.4),
                        c(-8, 28.4), c(-21, 26), c(-28, 19),
                        c(-29, 20), c(-30, 21), c(-30, 23),
                        c(-35, 21), c(-38, 16), c(-37, 10),
                        c(-38, 11), c(-40, 12), c(-42, 12),
                        c(-42, 6), c(-39, 0), c(-36, -4),
                        c(-37, -4), c(-38, -4), c(-39, -4),
                        c(-37, -5), c(-34, -7), c(-33, -9)])
    }

    private static func drawChatterMouth(_ g: CGContext, _ gaze: Gaze, _ pose: Pose, _ spec: SkinSpec) {
        if pose.chatter <= 0 { return }
        let jawPulse = pose.chatterAge < 0 ? 0 : 0.5 + 0.5 * sin(51 * pose.chatterAge)
        let jaw = pose.chatter * (3.3 + 1.6 * jawPulse)
        let opacity = (255 * pose.chatter).rounded(.down)
        let p = { (x: CGFloat, y: CGFloat) in gaze.project(x, y, 16) }
        fill(g, Argb(opacity, spec.mouth.r, spec.mouth.g, spec.mouth.b),
             outline([p(-8, 15), p(-5, 14.2), p(5, 14.2), p(8, 15),
                      p(7, 15 + jaw), p(3, 15.5 + jaw), p(0, 15.5 + jaw),
                      p(-3, 15.5 + jaw), p(-7, 15 + jaw), p(-8, 15)]))
        for side: CGFloat in [-1, 1] {
            let fang = CGMutablePath()
            fang.addLines(between: [p(2.9 * side, 14.8), p(6.2 * side, 14.6), p(4.8 * side, 14.8 + 3.7 * pose.chatter)])
            fang.closeSubpath()
            fill(g, Argb(opacity, 255, 246, 221), fang)
        }
    }

    private static func drawChatterMarks(_ g: CGContext, _ pose: Pose) {
        var left: CGFloat = 45
        for (i, mark) in pose.chatterCode.enumerated() {
            let dash = mark == "-"
            let markWidth: CGFloat = dash ? 12.9 : 4.3
            let center = left + markWidth * 0.5
            left += markWidth + 3.4
            let phase = (pose.chatterAge - CGFloat(i) * chatterMarkStagger) / chatterMarkLifetime
            if phase <= 0 || phase >= 1 { continue }

            let opacity = pose.chatter * sin(.pi * phase)
            let x = center + 4.8 * phase, y = -3 - 7 * phase
            let edge = Argb((160 * opacity).rounded(.down), 63, 49, 57)
            let ink = Argb((245 * opacity).rounded(.down), 255, 226, 177)
            if dash {
                let line = [pt(x - 4.85, y), pt(x - 1.6, y), pt(x + 1.6, y), pt(x + 4.85, y)]
                stroke(g, line, edge, 3.2)
                stroke(g, line, ink, 1.75)
            } else {
                g.setFillColor(edge.cg)
                g.fillEllipse(in: CGRect(x: x - 2.15, y: y - 2.15, width: 4.3, height: 4.3))
                g.setFillColor(ink.cg)
                g.fillEllipse(in: CGRect(x: x - 1.42, y: y - 1.42, width: 2.84, height: 2.84))
            }
        }
    }

    // MARK: Body

    private static let bodyFrame = { (brace: CGFloat) in MarkFrame(settle: 0, brace: brace) { _, _, p in p } }

    private static func bodyOutline(_ brace: CGFloat, _ spec: SkinSpec) -> CGMutablePath {
        if spec.isLonghair {
            return outline([pt(56, 60 - 19 * brace), pt(66, 36 - 20 * brace), pt(103, 33 - 9 * brace), pt(125, 39),
                            pt(143, 43), pt(155, 54), pt(161, 68),
                            pt(160, 68), pt(159, 68), pt(158, 68),
                            pt(164, 77), pt(167, 88), pt(165, 100),
                            pt(164, 98), pt(163, 97), pt(162, 96),
                            pt(162, 106), pt(158, 113), pt(151, 117),
                            pt(151, 115), pt(150, 114), pt(149, 113),
                            pt(145, 114), pt(139, 110), pt(136, 104),
                            pt(131, 92), pt(127, 83), pt(113, 84 + 3 * brace),
                            pt(95, 86 + 2 * brace), pt(72, 91), pt(58, 78 - 5 * brace),
                            pt(54, 74 - 11 * brace), pt(53, 65 - 15 * brace), pt(56, 60 - 19 * brace)])
        }
        let path = CGMutablePath()
        path.addBeziers([pt(62, 52 - 22 * brace), pt(80, 42 - 22 * brace), pt(106, 43 - 12 * brace), pt(127, 46)])
        path.addBeziers([pt(127, 46), pt(166.5, 51.64), pt(171, 107), pt(161.5, 127),
                         pt(158.65, 133), pt(149, 133), pt(143, 127),
                         pt(138, 122), pt(139, 111), pt(137, 101),
                         pt(135, 91), pt(129, 84), pt(117, 81 + 4 * brace)])
        path.addBeziers([pt(117, 81 + 4 * brace), pt(105, 78 + 8 * brace), pt(93, 80 + 5 * brace),
                         pt(82, 79 + 3 * brace), pt(71, 78 + brace), pt(59, 79 - 5 * brace),
                         pt(53, 76 - 12 * brace)])
        path.addBeziers([pt(53, 76 - 12 * brace), pt(47, 73 - 19 * brace), pt(49, 66 - 20 * brace),
                         pt(53, 60 - 22 * brace), pt(55, 56 - 22 * brace), pt(58, 54 - 22 * brace),
                         pt(62, 52 - 22 * brace)])
        path.closeSubpath()
        return path
    }

    private static func drawBody(_ g: CGContext, _ body: CGPath, _ farPaw: CGPath?, _ spec: SkinSpec, _ brace: CGFloat) {
        fill(g, spec.coat, body)
        g.saveGState()
        clip(g, to: body)
        drawMarks(g, marks(spec, [.body]), bodyFrame(brace))
        g.restoreGState()
        g.saveGState()
        if let farPaw { clip(g, outside: farPaw) }
        drawFurOutline(g, body, spec)
        g.restoreGState()
    }

    private static func longhairRuffOutline(_ brace: CGFloat) -> CGMutablePath {
        // The collar is rooted at the moving shoulder and falls between the two planted paws.
        let p = { (x: CGFloat, y: CGFloat) in pt(x, y - 22 * brace * min(max((100 - y) / 54, 0), 1)) }
        return outline([p(28, 62), p(31, 49), p(61, 35), p(80, 42),
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
                        p(42, 83), p(30, 74), p(28, 62)])
    }

    private static func drawLonghairRuff(_ g: CGContext, _ brace: CGFloat, _ spec: SkinSpec) {
        let bib = longhairRuffOutline(brace)
        fill(g, spec.ruff, bib)
        if let edge = spec.ruffEdge { strokePath(g, bib, edge, 0.9, join: .round) }
        g.saveGState()
        defer { g.restoreGState() }
        clip(g, to: bib)
        drawMarks(g, marks(spec, [.ruff]), bodyFrame(brace))
    }

    // MARK: Paws

    // Shrinks a paw toward the belly line, narrowing it as it retracts.
    private static func tuckTransform(_ centerX: CGFloat, _ tuck: CGFloat) -> CGAffineTransform {
        CGAffineTransform(translationX: centerX, y: 74)
            .scaledBy(x: 1 - 0.35 * tuck, y: 1 - 0.95 * tuck)
            .translatedBy(x: -centerX, y: -74)
    }

    private static func drawToeCreases(_ g: CGContext, _ foot: PawPress, _ crease: Argb) {
        stroke(g, [foot.toe(-4.3, -8), foot.toe(-4.3, -5.8), foot.toe(-3.5, -3.5), foot.toe(-2.4, -2.1)], crease, 0.9)
        stroke(g, [foot.toe(1.6, -8.5), foot.toe(1.6, -6.1), foot.toe(2.3, -3.9), foot.toe(3.6, -2.4)], crease, 0.9)
    }

    private static func pawFrame(_ foot: PawPress, _ lift: CGFloat) -> MarkFrame {
        MarkFrame(settle: 0, brace: lift) { mark, _, p in mark.space == .foot ? foot.toe(p.x, p.y) : p }
    }

    private static func farFrontPawOutline(_ lift: CGFloat, _ foot: PawPress) -> CGMutablePath {
        outline([pt(58, 65 - 17 * lift), pt(65, 59 - 18 * lift), pt(77, 62 - 14 * lift), pt(80, 70 - 13 * lift),
                 pt(82, 74 - 7 * lift), pt(83, 77 - 3 * lift), foot(82, 79),
                 foot(81, 85), foot(76, 90), foot(70, 90),
                 foot(63, 90), foot(59, 85), foot(59, 79),
                 pt(59, 75 - 5 * lift), pt(55, 70 - 11 * lift), pt(58, 65 - 17 * lift)])
    }

    private static func nearFrontPawOutline(_ lift: CGFloat, _ foot: PawPress) -> CGMutablePath {
        outline([pt(99, 65 - 19 * lift), pt(108, 61 - 17 * lift), pt(119 - 4 * lift, 65 - 12 * lift),
                 pt(121 - 4 * lift, 75 - 9 * lift), pt(122 - 3 * lift, 82 - 5 * lift), foot(115, 92), foot(108, 93),
                 foot(101, 95), foot(95, 90), foot(95, 84),
                 pt(93 + 2 * lift, 80 - 4 * lift), pt(91 + 4 * lift, 73 - 10 * lift), pt(94 + 2 * lift, 69 - 15 * lift),
                 pt(95 + 2 * lift, 67 - 17 * lift), pt(97, 66 - 18 * lift), pt(99, 65 - 19 * lift)])
    }

    private static func paintPaw(_ g: CGContext, _ paw: CGPath, _ spec: SkinSpec, _ parts: Set<Mark.Part>,
                                 _ foot: PawPress, _ lift: CGFloat) {
        fill(g, spec.coat, paw)
        g.saveGState()
        clip(g, to: paw)
        drawMarks(g, marks(spec, parts), pawFrame(foot, lift))
        g.restoreGState()
    }

    // Returns the painted paw outline so the body edge can skip it.
    private static func drawFarFrontPaw(_ g: CGContext, _ spec: SkinSpec, lift: CGFloat, foot: PawPress,
                                        tuck: CGFloat) -> CGPath? {
        if tuck >= 0.999 { return nil }
        let paw = farFrontPawOutline(lift, foot)
        let paint = {
            paintPaw(g, paw, spec, [.farPaw, .paws], foot, lift)
            drawFurOutline(g, paw, spec)
            drawToeCreases(g, foot, spec.crease)
        }
        if tuck <= 0 {
            paint()
            return paw
        }
        // The paw retracts into the belly; the body hides whatever has already gone inside.
        var fold = tuckTransform(70, tuck)
        g.saveGState()
        g.concatenate(fold)
        paint()
        g.restoreGState()
        return paw.copy(using: &fold)
    }

    private static func drawNearFrontPaw(_ g: CGContext, _ body: CGPath, _ spec: SkinSpec,
                                         lift: CGFloat, press: CGFloat, tuck: CGFloat) {
        if tuck >= 0.999 { return }
        let foot = PawPress(contact: nearContact, direction: 1, press: press)
        let paw = nearFrontPawOutline(lift, foot)
        let paint = { (bodyHere: CGPath) in
            paintPaw(g, paw, spec, [.nearPaw, .paws], foot, lift)
            g.saveGState()
            clip(g, outside: bodyHere)
            drawFurOutline(g, paw, spec)
            g.restoreGState()
            drawToeCreases(g, foot, spec.crease)
        }
        if tuck <= 0 {
            paint(body)
            return
        }
        // The paw retracts into the belly; the body hides whatever has already gone inside.
        g.saveGState()
        defer { g.restoreGState() }
        clip(g, outside: body)
        let fold = tuckTransform(107, tuck)
        g.concatenate(fold)
        var unfold = fold.inverted()
        paint(body.copy(using: &unfold) ?? body)
    }

    // MARK: Floating symbols

    private static func drawDreams(_ g: CGContext, _ pose: Pose) {
        for i in 0..<pose.dreamCount {
            let phase = (pose.dreamAge - CGFloat(i) * dreamStagger) / dreamLifetime
            if phase <= 0 || phase >= 1 { continue }

            let opacity = sin(.pi * phase)
            let x = 98 + 14 * CGFloat(i) + 12 * phase
            let y = 38 - 7 * CGFloat(i) - 18 * phase
            let size = 6.7 + 1.6 * CGFloat(i) + 1.7 * phase
            let letter = CGMutablePath()
            letter.addLines(between: [pt(x, y), pt(x + size, y), pt(x, y + size), pt(x + size, y + size)])
            strokePath(g, letter, Argb((96 * opacity).rounded(.down), 46, 53, 52), 2.5, join: .round)
            strokePath(g, letter, Argb((163 * opacity).rounded(.down), 235, 241, 227), 1.35, join: .round)
        }
    }

    private static func drawPetSymbols(_ g: CGContext, _ pose: Pose) {
        for i in 0..<pose.petSymbolCount {
            let phase = (pose.petSymbolAge - CGFloat(i) * petSymbolStagger) / petSymbolLifetime
            if phase <= 0 || phase >= 1 { continue }

            let opacity = min(1, phase * 7) * min(1, (1 - phase) * 2.6)
            let x = 102 + 13 * CGFloat(i) + 9 * phase
            let y = 32 - 7 * CGFloat(i) - 19 * phase
            let size = (i == 0 ? 5.2 : 4.2) * (0.76 + 0.24 * min(1, phase * 6))
            let p = { (dx: CGFloat, dy: CGFloat) in pt(x + size * dx, y + size * dy) }
            let symbol = pose.petIsNote
                // One continuous eighth-note silhouette stays legible at desktop scale.
                ? outline([p(0, -1.25), p(0.18, -1.05), p(0.99, -0.95), p(0.81, -0.18),
                           p(0.72, -0.43), p(0.47, -0.5), p(0.25, -0.54),
                           p(0.25, -0.2), p(0.25, 0.26), p(0.25, 0.56),
                           p(0.25, 1.08), p(-0.71, 1.32), p(-0.84, 0.85),
                           p(-1.0, 0.42), p(-0.4, 0.09), p(0, 0.25),
                           p(0, 0), p(0, -0.87), p(0, -1.25)])
                : outline([p(0, -0.29), p(-0.46, -1.09), p(-1.23, -0.67), p(-0.96, -0.04),
                           p(-0.77, 0.43), p(-0.27, 0.70), p(0, 0.99),
                           p(0.27, 0.70), p(0.77, 0.43), p(0.96, -0.04),
                           p(1.23, -0.67), p(0.46, -1.09), p(0, -0.29)])
            let fillAlpha = (246 * opacity).rounded(.down), edgeAlpha = (220 * opacity).rounded(.down)
            fill(g, pose.petIsNote ? Argb(fillAlpha, 43, 43, 46) : Argb(fillAlpha, 255, 151, 184), symbol)
            strokePath(g, symbol, pose.petIsNote ? Argb(edgeAlpha, 115, 115, 119) : Argb(edgeAlpha, 185, 82, 119), 0.85,
                       join: .round)
        }
    }
}
