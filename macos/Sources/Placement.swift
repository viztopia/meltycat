import CoreGraphics

// All rects here use a top-left origin with y down, like CGWindowList and the Windows build.
struct Placement: Equatable {
    var origin: CGPoint
    var scale: CGFloat
    var tailDirection: CGFloat
    var topPadding: CGFloat

    var size: CGSize {
        CGSize(width: CGFloat(CatGeometry.width) * scale, height: CGFloat(CatGeometry.height) * scale + topPadding)
    }

    func localCursor(_ cursor: CGPoint) -> CGPoint {
        CGPoint(x: (cursor.x - origin.x) / scale, y: (cursor.y - origin.y - topPadding) / scale)
    }
}

private func covers(_ outer: CGRect, _ inner: CGRect) -> Bool {
    outer.minX <= inner.minX + 1 && outer.minY <= inner.minY + 1
        && outer.maxX >= inner.maxX - 1 && outer.maxY >= inner.maxY - 1
}

func findPlacement(frame: CGRect, screen: CGRect, work: CGRect) -> Placement? {
    // Zoomed Mac windows fill the work area rather than the whole display.
    if covers(frame, screen) || covers(frame, work) { return nil }

    let scale = min(1, work.width / CGFloat(CatGeometry.width), work.height / CGFloat(CatGeometry.height))
    if scale <= 0 { return nil }
    if frame.maxX <= work.minX || frame.minX >= work.maxX || frame.maxY <= work.minY || frame.minY >= work.maxY {
        return nil
    }

    let points = { (value: Int) in CGFloat(value) * scale }
    let desiredX = frame.maxX - points(CatGeometry.cornerX)
    let normalTop = frame.minY - points(CatGeometry.cornerY)
    // A cat pushed back inside the screen would float over the window, so it only appears where it fits on the
    // corner. One point of slack absorbs rounding; the tail may still fold inward and the headroom shrink.
    if frame.maxX > work.maxX + 1 || desiredX < work.minX - 1 || normalTop < work.minY - 1
        || normalTop + points(CatGeometry.height) > work.maxY + 1 {
        return nil
    }
    let tailOutside = desiredX + points(CatGeometry.width) <= work.maxX
    let rightLimit = work.maxX - points(tailOutside ? CatGeometry.width : CatGeometry.foldedRight)
    let topPadding = max(0, min(points(CatGeometry.topPadding), normalTop - work.minY))
    return Placement(origin: CGPoint(x: min(max(desiredX, work.minX), rightLimit), y: normalTop - topPadding),
                     scale: scale, tailDirection: tailOutside ? 1 : -1, topPadding: topPadding)
}

// AppKit puts the origin at the bottom-left of the primary display; flipping is its own inverse.
func flipToTopLeft(_ rect: CGRect, primaryHeight: CGFloat) -> CGRect {
    CGRect(x: rect.minX, y: primaryHeight - rect.maxY, width: rect.width, height: rect.height)
}
