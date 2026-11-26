import AppKit

final class CatView: NSView {
    var pose = Pose.awake
    var spec: SkinSpec?
    var placement: Placement?

    override var isFlipped: Bool { true }

    override func draw(_ dirtyRect: NSRect) {
        guard let placement, let spec, let g = NSGraphicsContext.current?.cgContext else { return }
        g.clear(bounds)
        g.translateBy(x: 0, y: placement.topPadding)
        g.scaleBy(x: placement.scale, y: placement.scale)
        CatRenderer.draw(in: g, pose: pose, spec: spec, headroom: placement.topPadding / placement.scale)
    }
}

final class OverlayWindow: NSPanel {
    let catView = CatView()

    init() {
        super.init(contentRect: .zero, styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        isOpaque = false
        backgroundColor = .clear
        hasShadow = false
        ignoresMouseEvents = true
        isReleasedWhenClosed = false
        level = .floating
        collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary, .ignoresCycle]
        // The system's own zoom and fade when ordering in and out would fight the cat's animation.
        animationBehavior = .none
        contentView = catView
    }

    override var canBecomeKey: Bool { false }
    override var canBecomeMain: Bool { false }
}

// A puff of cloud where the cat arrives or leaves. A leaving cat is drawn here too, so the main overlay is free to
// show the cat somewhere else at the same time.
final class PoofView: NSView {
    var progress: CGFloat = 0
    var leaving: (pose: Pose, spec: SkinSpec)?
    var placement: Placement?
    var reduceMotion = false

    override var isFlipped: Bool { true }

    override func draw(_ dirtyRect: NSRect) {
        guard let placement, let g = NSGraphicsContext.current?.cgContext else { return }
        g.clear(bounds)
        g.translateBy(x: poofMargin * placement.scale, y: poofMargin * placement.scale + placement.topPadding)
        g.scaleBy(x: placement.scale, y: placement.scale)
        if let leaving {
            g.saveGState()
            g.setAlpha(reduceMotion ? 1 - progress : poofCatAlpha(appearing: false, progress: progress))
            g.beginTransparencyLayer(auxiliaryInfo: nil)
            CatRenderer.draw(in: g, pose: leaving.pose, spec: leaving.spec, headroom: placement.topPadding / placement.scale)
            g.endTransparencyLayer()
            g.restoreGState()
        }
        if !reduceMotion { drawPoof(in: g, progress: progress) }
    }
}

final class PoofWindow: NSPanel {
    let poofView = PoofView()
    let start: TimeInterval

    // `frame` is the cat's overlay frame in AppKit coordinates.
    init(around frame: NSRect, placement: Placement, start: TimeInterval, leaving: (pose: Pose, spec: SkinSpec)?,
         reduceMotion: Bool) {
        self.start = start
        let margin = poofMargin * placement.scale
        super.init(contentRect: frame.insetBy(dx: -margin, dy: -margin), styleMask: [.borderless, .nonactivatingPanel],
                   backing: .buffered, defer: false)
        isOpaque = false
        backgroundColor = .clear
        hasShadow = false
        ignoresMouseEvents = true
        isReleasedWhenClosed = false
        level = .floating
        collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary, .ignoresCycle]
        // The system's own zoom and fade when ordering in and out would fight the cat's animation.
        animationBehavior = .none
        poofView.placement = placement
        poofView.leaving = leaving
        poofView.reduceMotion = reduceMotion
        contentView = poofView
    }

    override var canBecomeKey: Bool { false }
    override var canBecomeMain: Bool { false }

    // False once the effect is over and the window has gone.
    func advance(to now: TimeInterval) -> Bool {
        let progress = CGFloat((now - start) / poofDuration)
        if progress >= 1 {
            orderOut(nil)
            return false
        }
        poofView.progress = max(0, progress)
        poofView.needsDisplay = true
        return true
    }
}

struct Perch {
    var windowID: CGWindowID
    var placement: Placement
}

var primaryScreenHeight: CGFloat { NSScreen.screens.first?.frame.height ?? 0 }

struct FrontWindow: Equatable {
    var pid: pid_t
    var id: CGWindowID
    var frame: CGRect
}

// Reads only window bounds, owner and layer; window titles would trigger the screen recording prompt.
// Safe off the main thread, which matters: the window server can take several milliseconds to answer while busy.
func queryFrontWindow(of pid: pid_t) -> FrontWindow? {
    guard let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID)
            as? [[String: Any]] else { return nil }
    let target = windows.first { info in
        info[kCGWindowOwnerPID as String] as? pid_t == pid
            && info[kCGWindowLayer as String] as? Int == 0
            && (info[kCGWindowAlpha as String] as? Double ?? 1) > 0
    }
    guard let target, let id = target[kCGWindowNumber as String] as? CGWindowID,
          let bounds = target[kCGWindowBounds as String],
          let frame = CGRect(dictionaryRepresentation: bounds as! CFDictionary), !frame.isEmpty else { return nil }
    return FrontWindow(pid: pid, id: id, frame: frame)
}

func findPerch(on window: FrontWindow) -> Perch? {
    let frame = window.frame
    let height = primaryScreenHeight
    let screens = NSScreen.screens.map {
        (frame: flipToTopLeft($0.frame, primaryHeight: height), work: flipToTopLeft($0.visibleFrame, primaryHeight: height))
    }
    let corner = CGPoint(x: frame.maxX - 1, y: frame.minY)
    let area = { (rect: CGRect) in rect.isNull ? 0 : rect.width * rect.height }
    guard let screen = screens.first(where: { $0.frame.contains(corner) })
            ?? screens.filter({ $0.frame.intersects(frame) }).max(by: {
                area($0.frame.intersection(frame)) < area($1.frame.intersection(frame))
            }),
          let placement = findPlacement(frame: frame, screen: screen.frame, work: screen.work) else { return nil }
    return Perch(windowID: window.id, placement: placement)
}
