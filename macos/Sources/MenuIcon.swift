import CoreGraphics

func menuIconNeedsHalo(spec: SkinSpec, darkBar: Bool) -> Bool {
    let coat = spec.headColor
    let luminance = (0.2126 * coat.r + 0.7152 * coat.g + 0.0722 * coat.b) / 255
    return darkBar && luminance < 0.25
}

// Draws the 32-unit tray head into a y-down context of the given size.
func drawMenuIcon(in g: CGContext, size: CGFloat, spec: SkinSpec, darkBar: Bool) {
    g.saveGState()
    defer { g.restoreGState() }
    g.scaleBy(x: size / 32, y: size / 32)
    if menuIconNeedsHalo(spec: spec, darkBar: darkBar), let rim = tintedSilhouette(spec) {
        // A dark coat vanishes on a dark menu bar, so stamp a light copy of the silhouette around it.
        for step in 0..<8 {
            let angle = CGFloat(step) * .pi / 4
            g.saveGState()
            g.translateBy(x: 1.2 * cos(angle), y: 32 + 1.2 * sin(angle))
            g.scaleBy(x: 1, y: -1)
            g.draw(rim, in: CGRect(x: 0, y: 0, width: 32, height: 32))
            g.restoreGState()
        }
    }
    CatRenderer.drawTrayIcon(in: g, spec: spec)
}

private func tintedSilhouette(_ spec: SkinSpec) -> CGImage? {
    let pixels = 128
    guard let mask = CGContext(data: nil, width: pixels, height: pixels, bitsPerComponent: 8, bytesPerRow: 0,
                               space: CGColorSpace(name: CGColorSpace.sRGB)!,
                               bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return nil }
    mask.translateBy(x: 0, y: CGFloat(pixels))
    mask.scaleBy(x: CGFloat(pixels) / 32, y: -CGFloat(pixels) / 32)
    CatRenderer.drawTrayIcon(in: mask, spec: spec)
    mask.setBlendMode(.sourceIn)
    mask.setFillColor(Argb(235, 240, 240, 240).cg)
    mask.fill(CGRect(x: 0, y: 0, width: 32, height: 32))
    return mask.makeImage()
}
