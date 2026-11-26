import CoreGraphics
import Foundation

enum PerchEvent: Equatable {
    case appear
    case disappear
    case move
}

// Which window the cat is on. Appearing and leaving wait until the change has held for a moment, so a window
// dragged along an edge doesn't puff again and again; moving to another window happens at once. A cat that has
// left doesn't come back while the mouse button is held, so it waits until a dragged window is dropped.
struct PerchPresence {
    static let settle: TimeInterval = 0.12

    private struct Pending {
        var id: CGWindowID?
        var since: TimeInterval
    }

    private(set) var shown: CGWindowID?
    private var pending: Pending?

    mutating func update(now: TimeInterval, observed: CGWindowID?, holding: Bool) -> PerchEvent? {
        if observed == shown {
            pending = nil
            return nil
        }
        if shown != nil, let observed {
            shown = observed
            pending = nil
            return .move
        }
        if pending == nil || pending?.id != observed { pending = Pending(id: observed, since: now) }
        guard let pending, now - pending.since >= Self.settle, observed == nil || !holding else { return nil }
        shown = observed
        self.pending = nil
        return observed == nil ? .disappear : .appear
    }
}

struct PoofPuff {
    var center: CGPoint
    var radius: CGFloat
    var alpha: CGFloat
}

let poofDuration: TimeInterval = 0.55
// The moment the cloud is fullest; the cat is swapped in or out behind it.
let poofPeak: CGFloat = 0.24
// Room around the cat's canvas for the cloud and the flying wisps.
let poofMargin: CGFloat = 32

private let poofCenter = CGPoint(x: 100, y: 70)

private struct Billow {
    var dx, dy, radius: CGFloat
    // Fractions of the poof: when this billow starts to swell and when it starts to shrink.
    var swell, fade: CGFloat
    // Where it drifts while breaking up, in canvas units; smoke rises, so mostly up.
    var driftX, driftY: CGFloat
}

// Uneven sizes and timings so the cloud reads as smoke rather than a cluster of equal balls. Placed so the
// fullest cloud hides the head, back, paws and haunch.
private let billows: [Billow] = [
    Billow(dx: -2, dy: 0, radius: 36, swell: 0.03, fade: 0.55, driftX: 2, driftY: -10),
    Billow(dx: -40, dy: -10, radius: 27, swell: 0, fade: 0.38, driftX: -10, driftY: -8),
    Billow(dx: 34, dy: -8, radius: 24, swell: 0.02, fade: 0.46, driftX: 9, driftY: -12),
    Billow(dx: 48, dy: 26, radius: 19, swell: 0.05, fade: 0.3, driftX: 12, driftY: 2),
    Billow(dx: 6, dy: 24, radius: 22, swell: 0.04, fade: 0.33, driftX: 3, driftY: 6),
    Billow(dx: -28, dy: 24, radius: 15, swell: 0.06, fade: 0.26, driftX: -9, driftY: 4),
    Billow(dx: -18, dy: -32, radius: 13, swell: 0.01, fade: 0.29, driftX: -4, driftY: -11),
]

private func clamp01(_ value: CGFloat) -> CGFloat { min(max(value, 0), 1) }

private func smoothstep(_ edge0: CGFloat, _ edge1: CGFloat, _ x: CGFloat) -> CGFloat {
    let t = clamp01((x - edge0) / (edge1 - edge0))
    return t * t * (3 - 2 * t)
}

private func easeOut(_ t: CGFloat) -> CGFloat { 1 - pow(1 - clamp01(t), 3) }

// A gentle overshoot as each billow puffs out.
private func swellCurve(_ t: CGFloat) -> CGFloat {
    let c: CGFloat = 1.2
    let u = clamp01(t) - 1
    return 1 + (c + 1) * u * u * u + c * u * u
}

func poofCatAlpha(appearing: Bool, progress: CGFloat) -> CGFloat {
    let shown = smoothstep(poofPeak - 0.07, poofPeak + 0.07, progress)
    return appearing ? shown : 1 - shown
}

// Canvas coordinates, like the cat.
func poofPuffs(progress: CGFloat) -> [PoofPuff] {
    let t = clamp01(progress)
    // Breaking up, the large billows shrink only partly and dissolve while the small ones go early and stay close,
    // so the cloud never parts into separate balls.
    let alpha = 1 - smoothstep(0.5, 1, t)
    return billows.map { billow -> PoofPuff in
        let grow = t <= billow.swell ? 0 : swellCurve((t - billow.swell) / (poofPeak - 0.02))
        let shrink = pow(clamp01((t - billow.fade) / (0.98 - billow.fade)), 1.2)
        let drift = 0.5 * easeOut((t - poofPeak) / (1 - poofPeak))
        let shrinkBy: CGFloat = billow.radius < 20 ? 1 : 0.55
        let spread = 0.7 + 0.3 * easeOut(t / poofPeak)
        return PoofPuff(center: CGPoint(x: poofCenter.x + billow.dx * spread + billow.driftX * drift,
                                        y: poofCenter.y + billow.dy * spread + billow.driftY * drift),
                        radius: max(0, billow.radius * grow * (1 - shrinkBy * shrink)), alpha: alpha)
    }
}

// A soft cartoon cloud: one faint outline around the union of the puffs, a pale shade, and a lighter top. As it
// breaks up it turns into soft-edged haze, so no hard-edged discs are left hanging in the air.
func drawPoof(in g: CGContext, progress: CGFloat) {
    let puffs = poofPuffs(progress: progress).filter { $0.radius > 0.3 }
    guard let alpha = puffs.first?.alpha, alpha > 0 else { return }
    let haze = smoothstep(poofPeak + 0.08, 0.62, progress)
    let circle = { (center: CGPoint, radius: CGFloat) in
        CGRect(x: center.x - radius, y: center.y - radius, width: radius * 2, height: radius * 2)
    }
    g.saveGState()
    if haze < 1 {
        g.setAlpha(alpha * 0.96 * (1 - haze))
        g.beginTransparencyLayer(auxiliaryInfo: nil)
        g.setStrokeColor(CGColor(srgbRed: 0.55, green: 0.53, blue: 0.52, alpha: 0.45))
        g.setLineWidth(1.6)
        for puff in puffs { g.strokeEllipse(in: circle(puff.center, puff.radius)) }
        g.setFillColor(CGColor(srgbRed: 0.93, green: 0.92, blue: 0.9, alpha: 1))
        for puff in puffs { g.fillEllipse(in: circle(puff.center, puff.radius)) }
        g.setFillColor(CGColor(srgbRed: 1, green: 0.995, blue: 0.985, alpha: 1))
        for puff in puffs {
            let center = CGPoint(x: puff.center.x - 0.08 * puff.radius, y: puff.center.y - 0.12 * puff.radius)
            g.fillEllipse(in: circle(center, puff.radius * 0.84))
        }
        g.endTransparencyLayer()
    }
    if haze > 0 {
        g.setAlpha(alpha * haze)
        let colors = [CGColor(srgbRed: 1, green: 0.995, blue: 0.985, alpha: 0.75),
                      CGColor(srgbRed: 1, green: 0.995, blue: 0.985, alpha: 0)] as CFArray
        if let gradient = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB), colors: colors, locations: [0.35, 1]) {
            for puff in puffs {
                g.drawRadialGradient(gradient, startCenter: puff.center, startRadius: 0, endCenter: puff.center,
                                     endRadius: puff.radius * 1.2, options: [])
            }
        }
    }
    g.restoreGState()
}
