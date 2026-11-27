import CoreGraphics
import Foundation
import ImageIO

BuiltinSkins.directory = URL(fileURLWithPath: CommandLine.arguments[1])
let goldenDirectory = URL(fileURLWithPath: CommandLine.arguments[2])

var failures = 0

func check(_ condition: Bool, _ message: String) {
    if !condition {
        failures += 1
        print("FAIL: \(message)")
    }
}

func render(_ pose: Pose, skin: Skin = .black) -> [UInt8] { render(pose, spec: BuiltinSkins.spec(skin)) }

func render(_ pose: Pose, spec: SkinSpec) -> [UInt8] {
    let width = CatGeometry.width, height = CatGeometry.height
    let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4,
                            space: CGColorSpace(name: CGColorSpace.sRGB)!,
                            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    context.translateBy(x: 0, y: CGFloat(height))
    context.scaleBy(x: 1, y: -1)
    CatRenderer.draw(in: context, pose: pose, spec: spec, headroom: CGFloat(CatGeometry.topPadding))
    let bytes = context.data!.assumingMemoryBound(to: UInt8.self)
    return Array(UnsafeBufferPointer(start: bytes, count: width * height * 4))
}

func expectPixel(_ pixels: [UInt8], _ x: Int, _ y: Int, _ rgba: (Int, Int, Int, Int),
                 _ label: String) {
    let i = (y * CatGeometry.width + x) * 4
    let actual = (Int(pixels[i]), Int(pixels[i + 1]), Int(pixels[i + 2]), Int(pixels[i + 3]))
    let close = abs(actual.0 - rgba.0) <= 3 && abs(actual.1 - rgba.1) <= 3 && abs(actual.2 - rgba.2) <= 3
        && abs(actual.3 - rgba.3) <= 3
    check(close, "\(label) at (\(x), \(y)): expected \(rgba), got \(actual)")
}

let coat = (25, 25, 29, 255)
let iris = (207, 245, 79, 255)
let clear = (0, 0, 0, 0)

let pixels = render(.awake)
expectPixel(pixels, 4, 4, clear, "empty corner")
expectPixel(pixels, 56, 36, coat, "forehead")
expectPixel(pixels, 42, 52, coat, "left pupil")
expectPixel(pixels, 75, 52, iris, "right iris")
expectPixel(pixels, 37, 52, iris, "left iris")
expectPixel(pixels, 150, 100, coat, "haunch")
expectPixel(pixels, 130, 100, clear, "gap under belly")
expectPixel(pixels, 161, 150, coat, "tail")
expectPixel(pixels, 70, 86, coat, "far paw")

// Uni's pale belly reaches the body's lower edge between the paws, leaving no strip of black fur under it.
do {
    let uni = render(.awake, skin: .uni)
    for x in [84, 88, 92] {
        for y in 77...78 {
            check(uni[(y * CatGeometry.width + x) * 4] > 150, "uni belly is pale at (\(x), \(y)) between the paws")
        }
    }
}


struct SplitMix64: RandomNumberGenerator {
    var state: UInt64
    mutating func next() -> UInt64 {
        state &+= 0x9E37_79B9_7F4A_7C15
        var z = state
        z = (z ^ (z >> 30)) &* 0xBF58_476D_1CE4_E5B9
        z = (z ^ (z >> 27)) &* 0x94D0_49BB_1331_11EB
        return z ^ (z >> 31)
    }
}

func near(_ a: CGFloat, _ b: CGFloat, _ tolerance: CGFloat = 1e-4) -> Bool { abs(a - b) <= tolerance }

do {
    let animation = Animation(random: SplitMix64(state: 1))
    let start = CGPoint(x: 10, y: 20), target = CGPoint(x: 110, y: 60)
    let first = animation.sample(now: 0, cursor: start, tailDirection: 1, newPerch: true)
    check(first.cursor == start, "new perch snaps gaze to cursor, got \(first.cursor)")
    let follow = 1 - exp(-19 * 0.016)
    let second = animation.sample(now: 0.016, cursor: target, tailDirection: 1, newPerch: false)
    check(near(second.cursor.x, 10 + 100 * follow) && near(second.cursor.y, 20 + 40 * follow),
          "gaze eases toward cursor, got \(second.cursor)")
    let clamped = animation.sample(now: 5, cursor: start, tailDirection: 1, newPerch: false)
    let step = 1 - exp(-19 * 0.08)
    check(near(clamped.cursor.x, second.cursor.x + (start.x - second.cursor.x) * step),
          "frame gap is capped at 0.08 s, got \(clamped.cursor)")
}

struct Driver {
    let animation: Animation
    var time: Double = 0
    var lastInput: Double = 0
    var cursor = CatRenderer.faceCenter
    var first = true

    init(seed: UInt64) { animation = Animation(random: SplitMix64(state: seed)) }

    mutating func step(to next: CGPoint, overCat: Bool = false, input: Bool = false) -> Pose {
        time += 1.0 / 60
        if next != cursor || input { lastInput = time }
        cursor = next
        animation.observe(now: time, cursor: next, inputTime: lastInput, scale: 1, overCat: overCat)
        let pose = animation.sample(now: time, cursor: next, tailDirection: 1, newPerch: first)
        first = false
        return pose
    }

    mutating func run(_ seconds: Double, overCat: Bool = false, path: (Int) -> CGPoint) -> [Pose] {
        (0..<Int((seconds * 60).rounded())).map { step(to: path($0), overCat: overCat) }
    }

    mutating func still(_ seconds: Double) -> [Pose] { let c = cursor; return run(seconds) { _ in c } }
    // Small back-and-forth moves: enough travel to wake the cat, far too slow to hunt.
    mutating func jiggle(_ seconds: Double) -> [Pose] {
        run(seconds) { CGPoint(x: 56 + ($0 % 2 == 0 ? 0 : 3), y: 53) }
    }
    mutating func shake(_ seconds: Double) -> [Pose] {
        run(seconds) { CGPoint(x: 56 + ($0 % 2 == 0 ? -25 : 25), y: 53) }
    }
    mutating func stroke(_ seconds: Double) -> [Pose] {
        run(seconds, overCat: true) { i in
            let phase = i % 20
            return CGPoint(x: 50 + 3 * CGFloat(phase < 10 ? phase : 20 - phase), y: 40)
        }
    }
}

do {
    var d = Driver(seed: 7)
    check(d.still(2).last!.alert == 0, "starts asleep")
    var minEye: CGFloat = 1, maxCurl: CGFloat = 0, maxWave: CGFloat = 0
    var swings = Set<Int>()
    let poses = d.jiggle(0.5) + (0..<12).flatMap { _ in d.jiggle(5) }
    for pose in poses.dropFirst(60) {
        minEye = min(minEye, pose.eyeOpen)
        maxCurl = max(maxCurl, pose.curl)
        maxWave = max(maxWave, pose.tailWave)
        swings.insert(Int((pose.swing * 10).rounded()))
    }
    check(poses.dropFirst(60).allSatisfy { $0.alert == 1 }, "stays awake while the mouse moves")
    check(minEye < 0.18, "blinks within a minute, min eyeOpen \(minEye)")
    check(maxCurl > 0.9, "curls its tail within a minute, max curl \(maxCurl)")
    check(maxWave > 0.9, "waves its tail within a minute, max wave \(maxWave)")
    check(swings.count > 8, "tail swings, distinct swing values \(swings.count)")
}

do {
    var d = Driver(seed: 3)
    _ = d.still(1)
    check(d.jiggle(0.5).last!.alert == 1, "a burst of mouse travel wakes the cat")
    check(d.still(5.5).last!.alert == 1, "stays awake for 6 s after the last activity")
    check(d.still(1).last!.alert == 0, "falls asleep after 6 s idle")

    let sleep = d.still(40)
    check(abs(sleep[4 * 60].tailMotion - 1) < 1e-6, "tail keeps swinging for the first 5 s of sleep")
    check(sleep.last!.tailMotion == 0, "tail settles by 12.5 s of sleep")
    check(sleep.contains { $0.dreamCount > 0 }, "dreams float up once asleep")
    check(sleep.contains { $0.earTwitch != 0 }, "ears twitch during sleep")
    check(sleep.allSatisfy { $0.eyeOpen == 0 && $0.alert == 0 }, "eyes stay shut while asleep")
}

do {
    var d = Driver(seed: 5)
    _ = d.still(1)
    _ = d.step(to: d.cursor, input: true)
    check(d.still(1).last!.alert == 0, "a single key press does not wake the cat")
    _ = d.step(to: d.cursor, input: true)
    _ = d.still(0.3)
    _ = d.step(to: d.cursor, input: true)
    check(d.still(0.5).last!.alert == 1, "two presses within 700 ms wake the cat")
}

do {
    var d = Driver(seed: 11)
    _ = d.jiggle(0.5)
    let levelOne = d.shake(0.3)
    check(levelOne.last!.hunt > 0.5 && levelOne.last!.brace == 0, "a fast shake rounds the pupils (Level 1)")
    check(d.shake(1.5).allSatisfy { $0.brace == 0 }, "1.5 s more of shaking is not yet Level 2")
    let levelTwo = d.shake(0.7)
    check(levelTwo.last!.brace > 0 && levelTwo.last!.chatter == 0, "about 2 s of shaking raises the body (Level 2)")
    check(d.shake(2.5).allSatisfy { $0.chatter < 0.1 }, "2.5 s more is not yet Level 3")
    let levelThree = d.shake(9.5)
    check(levelThree.prefix(70).contains { $0.chatter > 0.9 }, "about 3.5 s more starts chattering (Level 3)")
    check(levelThree.last!.chatter > 0.9 && levelThree.last!.brace > 0.99, "keeps chattering while the shaking goes on")
    var codes: [String] = []
    for pose in levelThree where codes.last != pose.chatterCode { codes.append(pose.chatterCode) }
    codes.removeAll { $0.isEmpty }
    check(codes.prefix(4) == ["..-.", "---", "---", "-.."], "chatters FOOD in Morse, got \(codes.prefix(6))")
    let hold = d.still(0.5)
    check(hold.last!.chatter < 0.1 && hold.last!.brace > 0.99, "stopping drops to Level 2 but holds the pose")
    let release = d.still(3)
    check(release.last!.brace < 0.05 && release.last!.hunt < 0.05, "relaxes after the 2 s hold")
}

do {
    var d = Driver(seed: 13)
    _ = d.jiggle(0.5)
    let onset = d.stroke(2.8)
    check(onset.last!.squint == 0, "petting needs 3 s of stroking before it starts")
    let petting = d.stroke(0.7)
    check(petting.last!.squint > 0.9 && petting.last!.alert == 1, "stroking the head narrows the eyes")
    let symbols = d.stroke(9)
    check(symbols.contains { $0.petSymbolCount > 0 }, "hearts or a note appear while petting")
    check(symbols.allSatisfy { $0.hunt == 0 }, "no hunting while petting")
}

do {
    let awake = Pose.awake
    let headroom = CGFloat(CatGeometry.topPadding)
    check(CatRenderer.hitsPettingArea(CatRenderer.faceCenter, awake, spec: BuiltinSkins.spec(.black), headroom: headroom), "the head is pettable")
    check(CatRenderer.hitsPettingArea(CGPoint(x: 150, y: 100), awake, spec: BuiltinSkins.spec(.black), headroom: headroom), "the haunch is pettable")
    check(!CatRenderer.hitsPettingArea(CGPoint(x: 130, y: 100), awake, spec: BuiltinSkins.spec(.black), headroom: headroom), "the gap under the belly is not")
    check(!CatRenderer.hitsPettingArea(CGPoint(x: 4, y: 4), awake, spec: BuiltinSkins.spec(.black), headroom: headroom), "empty space is not")
}

do {
    var dreaming = Pose.awake
    dreaming.alert = 0; dreaming.eyeOpen = 0; dreaming.dreamCount = 1; dreaming.dreamAge = 0.9
    let dreamPixels = render(dreaming)
    check(dreamPixels[(29 * CatGeometry.width + 108) * 4 + 3] > 100, "draws a Z while dreaming")

    var hearts = Pose.awake
    hearts.petSymbolCount = 1; hearts.petSymbolAge = 0.775
    let heartPixels = render(hearts)
    expectPixel(heartPixels, 106, 23, (246, 146, 177, 246), "pink heart")

    var chattering = Pose.awake
    chattering.chatter = 1
    let fangPixels = render(chattering)
    expectPixel(fangPixels, 51, 68, (255, 246, 221, 255), "left fang")

    // Over the ten equally likely rolls: 40% of naps keep both paws out, 20% tuck the left, 20% the right, 20% both.
    let tucks = (0...9).map { Animation.pawTuck(roll: $0) }
    check(tucks.filter { !$0.left && !$0.right }.count == 4 && tucks.filter { $0.left && !$0.right }.count == 2
          && tucks.filter { !$0.left && $0.right }.count == 2 && tucks.filter { $0.left && $0.right }.count == 2,
          "paw tucks split 40/20/20/20")

    var tucked = Pose.awake
    tucked.pawTuckLeft = 1; tucked.pawTuckRight = 1
    let tuckedPixels = render(tucked)
    let untuckedPixels = render(.awake)
    expectPixel(untuckedPixels, 108, 90, coat, "near paw")
    expectPixel(tuckedPixels, 70, 86, clear, "far paw tucked away")
    expectPixel(tuckedPixels, 108, 90, clear, "near paw tucked away")
}

do {
    // Top-left origin, like CGWindowList: a 1440x900 display with a 25 pt menu bar.
    let screen = CGRect(x: 0, y: 0, width: 1440, height: 900)
    let work = CGRect(x: 0, y: 25, width: 1440, height: 875)

    let normal = findPlacement(frame: CGRect(x: 100, y: 300, width: 800, height: 500), screen: screen, work: work)
    check(normal?.origin == CGPoint(x: 748, y: 204) && normal?.tailDirection == 1 && normal?.topPadding == 24
          && normal?.scale == 1, "perches on the top-right corner, got \(String(describing: normal))")
    if let normal {
        check(normal.localCursor(CGPoint(x: 748 + 56, y: 204 + 24 + 53)) == CGPoint(x: 56, y: 53),
              "maps screen cursor to cat-local coordinates")
    }

    let rightEdge = findPlacement(frame: CGRect(x: 630, y: 300, width: 800, height: 500), screen: screen, work: work)
    check(rightEdge?.origin.x == 1440 - 167 && rightEdge?.tailDirection == -1,
          "folds the tail inward at the right edge, got \(String(describing: rightEdge))")

    let nearTop = findPlacement(frame: CGRect(x: 100, y: 110, width: 800, height: 500), screen: screen, work: work)
    check(nearTop?.origin.y == 25 && nearTop?.topPadding == 13,
          "shrinks the headroom near the top, got \(String(describing: nearTop))")

    let flush = findPlacement(frame: CGRect(x: 100, y: 97, width: 800, height: 500), screen: screen, work: work)
    check(flush?.origin.y == 25 && flush?.topPadding == 0, "lies on a window with just enough room above it, got \(String(describing: flush))")

    // Pushed back inside the screen the cat would float over the window instead of lying on its corner.
    check(findPlacement(frame: CGRect(x: 100, y: 95, width: 800, height: 500), screen: screen, work: work) == nil,
          "hides when the window is too close to the top")
    check(findPlacement(frame: CGRect(x: 100, y: 25, width: 800, height: 500), screen: screen, work: work) == nil,
          "hides on a window right under the menu bar")
    check(findPlacement(frame: CGRect(x: 700, y: 300, width: 800, height: 500), screen: screen, work: work) == nil,
          "hides when the window's top-right corner is off the right edge")
    check(findPlacement(frame: CGRect(x: -100, y: 300, width: 200, height: 500), screen: screen, work: work) == nil,
          "hides when the head would be off the left edge")
    check(findPlacement(frame: CGRect(x: 100, y: 784, width: 800, height: 500), screen: screen, work: work) == nil,
          "hides when the cat would hang below the bottom")
    check(findPlacement(frame: CGRect(x: 100, y: 782, width: 800, height: 500), screen: screen, work: work) != nil,
          "still lies on a low window that leaves room for the cat")

    check(findPlacement(frame: screen, screen: screen, work: work) == nil, "skips full-screen windows")
    check(findPlacement(frame: work, screen: screen, work: work) == nil, "skips zoomed windows that fill the work area")
    check(findPlacement(frame: CGRect(x: 1500, y: 300, width: 400, height: 300), screen: screen, work: work) == nil,
          "skips windows outside the work area")

    let small = CGRect(x: 0, y: 0, width: 200, height: 150)
    let scaled = findPlacement(frame: CGRect(x: 40, y: 57, width: 120, height: 80), screen: small, work: small)
    check(scaled.map { abs($0.scale - 150.0 / 190) < 1e-6 } == true, "shrinks on a tiny work area, got \(String(describing: scaled))")

    check(flipToTopLeft(CGRect(x: 10, y: 0, width: 100, height: 50), primaryHeight: 900)
          == CGRect(x: 10, y: 850, width: 100, height: 50), "converts AppKit rects to top-left coordinates")
}


do {
    check(Skin.menuOrder.map(\.setting) == ["ragdoll", "ragdoll-mix", "maine-coon", "siberian", "orange-tabby", "black",
                                           "li-hua", "classic-tuxedo", "tuxedo"], "menu order and saved values match Windows")
    check(Skin(setting: "tuxedo") == .uni && Skin(setting: "unknown") == nil, "legacy and unknown settings")
    check(Skin.menuOrder.filter(\.isLonghair) == [.ragdoll, .ragdollMix, .maineCoon, .siberian], "four longhair skins")

    let irises: [(Skin, (Int, Int, Int, Int))] = [
        (.ragdoll, (125, 176, 210, 255)), (.ragdollMix, (110, 174, 200, 255)), (.maineCoon, (223, 187, 132, 255)), (.siberian, (163, 185, 195, 255)),
        (.orangeTabby, (188, 217, 114, 255)), (.black, (207, 245, 79, 255)), (.liHua, (190, 204, 128, 255)),
        (.tuxedo, (200, 214, 141, 255)), (.uni, (242, 204, 86, 255)),
    ]
    for (skin, iris) in irises {
        expectPixel(render(.awake, skin: skin), 75, 52, iris, "\(skin.setting) iris")
    }
    expectPixel(render(.awake, skin: .ragdollMix), 56, 36, (253, 247, 236, 255), "ragdoll mix ivory forehead")
    expectPixel(render(.awake, skin: .ragdoll), 56, 50, (253, 251, 248, 255), "ragdoll white blaze between the eyes")
    let english = Language(setting: "en")!, chinese = Language(setting: "zh-CN")!
    check(english.skinNames[1] == "Ragdoll Mix" && chinese.skinNames[1] == "布偶混血", "the ragdoll mix is named")

    // Asleep, a shorthair's lower left cheek spreads where the face presses on the ledge, and a longhair's cheek
    // fur rests on it.
    let asleepPose = goldenPoses().first { $0.0 == "asleep" }!.1
    func leftmost(_ pixels: [UInt8], row y: Int) -> Int {
        (0..<CatGeometry.width).first { pixels[(y * CatGeometry.width + $0) * 4 + 3] > 128 } ?? CatGeometry.width
    }
    check(leftmost(render(asleepPose, skin: .black), row: 80) <= 34, "a sleeping shorthair's cheek spreads onto the ledge")
    for skin in Skin.menuOrder where skin.isLonghair {
        let pixels = render(asleepPose, skin: skin)
        let alpha = { (x: Int, y: Int) in pixels[(y * CatGeometry.width + x) * 4 + 3] }
        check(alpha(33, 85) == 0, "\(skin.setting): the ledge holds the sleeping cheek fur level instead of letting it sag")
        check(alpha(33, 81) > 250, "\(skin.setting): the levelled cheek fur is still there")
    }

    // Asleep with both paws tucked, every cat lies flat: the bottom of the head lines up with the belly, as a loaf.
    var loaf = asleepPose
    loaf.pawTuckLeft = 1
    loaf.pawTuckRight = 1
    for skin in Skin.menuOrder {
        let pixels = render(loaf, skin: skin)
        func bottom(_ columns: Range<Int>) -> Int {
            let rows: [Int] = columns.map { x in (0..<120).filter { pixels[($0 * CatGeometry.width + x) * 4 + 3] > 128 }.max() ?? 0 }
            return rows.sorted()[rows.count / 2]
        }
        let head = bottom(30..<50), belly = bottom(105..<130)
        check(abs(head - belly) <= 1, "\(skin.setting) sleeps flat: head bottom \(head), belly bottom \(belly)")
    }

    var folded = Pose.awake
    folded.tailDirection = -1
    for skin in Skin.menuOrder where skin.isLonghair {
        let pixels = render(folded, skin: skin)
        var outside = 0
        for y in 0..<CatGeometry.height {
            for x in CatGeometry.foldedRight..<CatGeometry.width where pixels[(y * CatGeometry.width + x) * 4 + 3] > 0 {
                outside += 1
            }
        }
        check(outside == 0, "\(skin.setting) folded plume stays inside x < \(CatGeometry.foldedRight), \(outside) pixels out")
    }
}


do {
    var asleep = Pose.awake
    asleep.alert = 0; asleep.eyeOpen = 0; asleep.tailMotion = 0
    var hunting = Pose.awake
    hunting.hunt = 0.5
    var petting = Pose.awake
    petting.squint = 1
    check(frameInterval(perched: false, pose: nil, secondsSinceInput: 0) == 0.25, "polls slowly with no perch")
    check(frameInterval(perched: true, pose: .awake, secondsSinceInput: 0.2) == 1.0 / 60, "full rate right after input")
    check(frameInterval(perched: true, pose: hunting, secondsSinceInput: 5) == 1.0 / 60, "full rate while hunting")
    check(frameInterval(perched: true, pose: petting, secondsSinceInput: 5) == 1.0 / 60, "full rate while petting")
    check(frameInterval(perched: true, pose: .awake, secondsSinceInput: 3) == 1.0 / 60, "full rate while the tail sways")
    var stillTail = Pose.awake
    stillTail.tailMotion = 0
    check(frameInterval(perched: true, pose: stillTail, secondsSinceInput: 20) == 1.0 / 30, "half rate when awake and still")
    check(frameInterval(perched: true, pose: asleep, secondsSinceInput: 30) == 1.0 / 15, "quarter rate asleep")
    check(frameInterval(perched: true, pose: asleep, secondsSinceInput: 0.1) == 1.0 / 60, "input while asleep wakes the rate")
    var dozing = asleep
    dozing.alert = 0.5; dozing.eyeOpen = 0.5; dozing.tailMotion = 1
    check(frameInterval(perched: true, pose: dozing, secondsSinceInput: 7) == 1.0 / 60, "full rate while dozing off")
    var settling = asleep
    settling.tailMotion = 0.3
    check(frameInterval(perched: true, pose: settling, secondsSinceInput: 15) == 1.0 / 60, "full rate until the tail is still")
    var dreaming = asleep
    dreaming.dreamCount = 2; dreaming.dreamAge = 0.5
    check(frameInterval(perched: true, pose: dreaming, secondsSinceInput: 25) == 1.0 / 60, "full rate while Zs float")
    var twitching = asleep
    twitching.earTwitch = 0.6
    check(frameInterval(perched: true, pose: twitching, secondsSinceInput: 30) == 1.0 / 60, "full rate during an ear twitch")
}

do {
    // Hunting must still trigger when the shake begins during the 15 Hz sleep cadence and the app speeds up.
    var d = Driver(seed: 17)
    _ = d.jiggle(0.5)
    for i in 0..<6 {
        d.time += 1.0 / 15 - 1.0 / 60
        _ = d.step(to: CGPoint(x: 56 + (i % 2 == 0 ? -25 : 25), y: 53))
    }
    check(d.shake(0.3).last!.hunt > 0.5, "hunting still triggers after a slow-cadence start")
}


do {
    check(Language.all.map(\.setting) == ["zh-CN", "zh-TW", "en", "ja", "ko", "es", "fr", "ru", "th", "vi", "de", "id",
                                         "pt", "tr", "it"], "15 languages in the Windows order")
    check(Language.all.allSatisfy { $0.skinNames.count == Skin.menuOrder.count }, "every language names every skin")
    check(Language(setting: "ja")?.quit == "meltycat を終了" && Language(setting: "xx") == nil, "language lookup")
    check(Language.all[0].skinNames[0] == "布偶猫" && Language.all[2].skinNames[8] == "Uni", "skin names in menu order")
}

do {
    check(menuIconNeedsHalo(spec: BuiltinSkins.spec(.black), darkBar: true), "a black cat needs a rim on a dark menu bar")
    check(!menuIconNeedsHalo(spec: BuiltinSkins.spec(.black), darkBar: false), "but not on a light one")
    check(!menuIconNeedsHalo(spec: BuiltinSkins.spec(.ragdoll), darkBar: true), "a pale cat stands out on a dark bar by itself")

    func icon(_ darkBar: Bool) -> [UInt8] {
        let g = CGContext(data: nil, width: 32, height: 32, bitsPerComponent: 8, bytesPerRow: 32 * 4,
                          space: CGColorSpace(name: CGColorSpace.sRGB)!,
                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        g.translateBy(x: 0, y: 32)
        g.scaleBy(x: 1, y: -1)
        drawMenuIcon(in: g, size: 32, spec: BuiltinSkins.spec(.black), darkBar: darkBar)
        return Array(UnsafeBufferPointer(start: g.data!.assumingMemoryBound(to: UInt8.self), count: 32 * 32 * 4))
    }
    let beside = (18 * 32 + 2) * 4
    check(icon(true)[beside + 3] > 60 && icon(true)[beside] > 150, "draws a light rim beside the cheek on a dark bar")
    check(icon(false)[beside + 3] == 0, "no rim on a light bar")
}


do {
    let pick = { (preferred: [String]) in Language.system(preferred: preferred).setting }
    check(pick(["zh-Hans-CN"]) == "zh-CN" && pick(["zh-Hans-SG"]) == "zh-CN", "simplified Chinese")
    check(pick(["zh-Hant-TW"]) == "zh-TW" && pick(["zh-Hant-HK"]) == "zh-TW" && pick(["zh-HK"]) == "zh-TW",
          "traditional Chinese, Hong Kong included")
    check(pick(["ja-JP"]) == "ja" && pick(["pt-BR"]) == "pt" && pick(["en-GB"]) == "en", "region variants")
    check(pick(["sv-SE", "de-DE"]) == "de", "skips unsupported languages in preference order")
    check(pick(["sv-SE"]) == "en" && pick([]) == "en", "falls back to English")
}


// Reference renders of every built-in skin. Most come from the hand-written renderer from before skins became data;
// whatever was later changed on purpose (Uni's chest, the sleeping cheek, the longhair chin, the new Ragdoll) was
// re-rendered after review.
func goldenPixels(_ url: URL) -> [UInt8]? {
    guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else { return nil }
    let width = image.width, height = image.height
    let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4,
                            space: CGColorSpace(name: CGColorSpace.sRGB)!,
                            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    return Array(UnsafeBufferPointer(start: context.data!.assumingMemoryBound(to: UInt8.self), count: width * height * 4))
}

do {
    let width = CatGeometry.width, height = CatGeometry.height + 24
    var worst: [(String, Int)] = []
    for skin in Skin.menuOrder {
        for (name, pose, headroom) in goldenPoses() {
            let label = "\(skin.setting)-\(name)"
            guard let expected = goldenPixels(goldenDirectory.appendingPathComponent("\(label).png")) else {
                check(false, "missing golden \(label)")
                continue
            }
            let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4,
                                    space: CGColorSpace(name: CGColorSpace.sRGB)!,
                                    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
            context.translateBy(x: 0, y: CGFloat(height))
            context.scaleBy(x: 1, y: -1)
            context.translateBy(x: 0, y: headroom)
            CatRenderer.draw(in: context, pose: pose, spec: BuiltinSkins.spec(skin), headroom: headroom)
            let actual = UnsafeBufferPointer(start: context.data!.assumingMemoryBound(to: UInt8.self), count: width * height * 4)
            var differing = 0
            for i in stride(from: 0, to: actual.count, by: 4) {
                let delta = (0..<4).map { abs(Int(actual[i + $0]) - Int(expected[i + $0])) }.max()!
                if delta > 24 { differing += 1 }
            }
            worst.append((label, differing))
        }
    }
    worst.sort { $0.1 > $1.1 }
    print("golden: most differing pixels:", worst.prefix(6).map { "\($0.0)=\($0.1)" }.joined(separator: ", "))
    for (label, differing) in worst {
        check(differing <= 4, "\(label) differs from the hand-written skin in \(differing) pixels")
    }
}

do {
    let minimal = ##"{"formatVersion": 1, "name": "Test", "body": "shorthair", "palette": {"coat": "#808080", "iris": "#00FF00"}}"##
    func parse(_ text: String) -> Result<SkinSpec, Error> { Result { try SkinSpec.parse(Data(text.utf8)) } }
    func error(_ text: String) -> String {
        if case .failure(let error) = parse(text) { return "\(error)" }
        return "(parsed)"
    }
    func edit(_ replacements: [(String, String)]) -> String {
        replacements.reduce(minimal) { $0.replacingOccurrences(of: $1.0, with: $1.1) }
    }

    if case .success(let spec) = parse(minimal) {
        check(spec.faceColor.r == 128 && spec.ear.r == 128 && spec.outline == nil && spec.pupil.r == 25,
              "colours default sensibly")
    } else {
        check(false, "parses a minimal skin")
    }
    for skin in Skin.menuOrder { _ = BuiltinSkins.spec(skin) }
    check(BuiltinSkins.spec(.ragdollMix).marks.count == 11 && BuiltinSkins.spec(.ragdoll).marks.count == 13
          && BuiltinSkins.spec(.orangeTabby).tailBands.count == 5,
          "built-in skins load with their marks")

    check(error(edit([("\"coat\": \"#808080\", ", "")])).contains("palette.coat: required"), "coat is required")
    check(error(edit([("#808080", "grey")])).contains("palette.coat: expected a colour"), "bad colours are named")
    check(error(edit([("\"formatVersion\": 1", "\"formatVersion\": 2")])).contains("newer meltycat"), "future versions")
    check(error(edit([("shorthair", "sphynx")])).contains("body: unknown value"), "unknown body")
    check(error("[1, 2]").contains("JSON object") && error("{nope").contains("not valid JSON"), "not an object")
    check(error(edit([("\"name\": \"Test\"", "\"name\": \"  \"")])).contains("name: required"), "blank names")
    check((try? parse(minimal).get())?.missingFrontPaws.isEmpty == true, "front paws are all there by default")
    if case .success(let spec) = parse(edit([("\"body\"", "\"missingFrontPaws\": [\"right\"], \"body\"")])) {
        check(spec.missingFrontPaws == [.right], "parses a missing front paw")
    } else {
        check(false, "parses a missing front paw")
    }
    check(error(edit([("\"body\"", "\"missingFrontPaws\": [\"left\", \"middle\"], \"body\"")])).contains("missingFrontPaws[1]"),
          "names a bad paw side")
    check(error(edit([("\"body\"", "\"missingFrontPaws\": \"right\", \"body\"")])).contains("missingFrontPaws: must be an array"),
          "missing paws must be a list")
    if case .success(let spec) = parse(edit([("\"body\"", "\"sparkles\": 3, \"eyeShine\": 7, \"body\"")])) {
        check(spec.eyeShine == 1, "unknown fields are ignored and numbers clamped")
    } else {
        check(false, "unknown fields are ignored")
    }

    func withMark(_ mark: String) -> String {
        edit([("\"palette\"", "\"marks\": [\(mark)], \"palette\"")])
    }
    let blaze = ##"{"part": "head", "space": "head", "color": "#FFFFFF", "path": "M 0 -10 L 4 0 L -4 0 Z"}"##
    if case .success(let spec) = parse(withMark(blaze)) {
        check(spec.marks.first?.path == [.move(CGPoint(x: 0, y: -10)), .line(CGPoint(x: 4, y: 0)),
                                         .line(CGPoint(x: -4, y: 0)), .close], "parses a head mark")
    } else {
        check(false, "parses a head mark: \(error(withMark(blaze)))")
    }
    check(error(withMark(blaze.replacingOccurrences(of: "M 0 -10", with: "M 0 -10 A 1 1 0 0 1 2 2"))).contains("unsupported command \"A\""),
          "arcs are rejected by name")
    check(error(withMark(blaze.replacingOccurrences(of: "\"head\", \"color\"", with: "\"tail\", \"color\""))).contains("takes space"),
          "parts only take their spaces")
    check(error(withMark(blaze.replacingOccurrences(of: "\"part\": \"head\", \"space\": \"head\"",
                                                    with: "\"part\": \"tail\", \"space\": \"tail\""))).contains("only on longhair"),
          "shorthair bodies have no plume")
    check(error(withMark(blaze.replacingOccurrences(of: "}", with: ##", "braced": "M 0 -10 L 4 0 Z"}"##))).contains("same commands"),
          "morph targets must match the path")
    check(error(withMark(blaze.replacingOccurrences(of: "\"color\": \"#FFFFFF\", ", with: ""))).contains("color: required"),
          "marks need a colour")

    let relative = try? SkinSpec.parsePath("m 1 2 3 4 h 5 v -6 c 1 1 2 2 3 3 q 1 0 2 1 z", at: "p")
    check(relative == [.move(CGPoint(x: 1, y: 2)), .line(CGPoint(x: 4, y: 6)), .line(CGPoint(x: 9, y: 6)),
                       .line(CGPoint(x: 9, y: 0)), .cubic(CGPoint(x: 10, y: 1), CGPoint(x: 11, y: 2), CGPoint(x: 12, y: 3)),
                       .quad(CGPoint(x: 13, y: 3), CGPoint(x: 14, y: 4)), .close],
          "relative commands, implicit lines, H and V, got \(String(describing: relative))")
    check((try? SkinSpec.parsePath("M1e1,-2.5L.5-1-1-1", at: "p")) == [.move(CGPoint(x: 10, y: -2.5)),
                                                                     .line(CGPoint(x: 0.5, y: -1)),
                                                                     .line(CGPoint(x: -1, y: -1))],
          "compact number syntax")
}


do {
    for language in Language.all {
        let text = language.skinText
        let all = [text.changeSkin, text.dropHint, text.chooseFile, text.paste, text.copyGuide, text.guideCopied,
                   text.delete, text.imported, text.importFailed, text.custom]
        check(all.allSatisfy { !$0.isEmpty }, "\(language.setting) has every skin window string")
        check(!all.joined().lowercased().contains("coat"), "\(language.setting) calls them skins, not coats")
    }
    for language in Language.all {
        let text = language.poofText
        check(language.setting == "en" || text.title != Language(setting: "en")!.poofText.title,
              "\(language.setting) has its own poof strings")
        check(text.modes.count == PoofMode.allCases.count && !text.modes.contains(where: \.isEmpty),
              "\(language.setting) names every poof mode")
    }
    check(Language(setting: "zh-CN")?.skinText.changeSkin == "换皮肤…", "Chinese menu item")
    check(Language(setting: "en")?.skinText.changeSkin == "Change Skin…", "English menu item")
}


do {
    // The guide's complete example must import, and its braced path must follow the formula the guide gives.
    let guide = (try? String(contentsOf: BuiltinSkins.directory!.appendingPathComponent("AI-GUIDE.md"), encoding: .utf8)) ?? ""
    let section = guide.components(separatedBy: "## Complete minimal example").last ?? ""
    let json = section.components(separatedBy: "```json").dropFirst().first?.components(separatedBy: "```").first ?? ""
    do {
        let spec = try SkinSpec.parse(Data(json.utf8))
        let bib = spec.marks[1]
        let lifted = bib.path.map { command in
            command.with(command.points.map { p in
                let clamp = { (v: CGFloat) in min(max(v, 0), 1) }
                return CGPoint(x: p.x, y: p.y - 23 * clamp((129 - p.x) / 59) * clamp((79 - p.y) / 40))
            })
        }
        let matches = zip(lifted, bib.braced ?? []).allSatisfy { a, b in
            zip(a.points, b.points).allSatisfy { abs($0.x - $1.x) < 0.06 && abs($0.y - $1.y) < 0.06 }
        }
        check(matches && bib.braced?.count == lifted.count, "the guide's braced example follows its formula")
    } catch {
        check(false, "the guide's complete example imports: \(error)")
    }
}


do {
    let folder = FileManager.default.temporaryDirectory.appendingPathComponent("meltycat-tests-\(UUID().uuidString)")
    CustomSkins.directory = folder
    defer { try? FileManager.default.removeItem(at: folder) }
    let valid = Data(##"{"formatVersion": 1, "name": "Mochi", "body": "longhair", "palette": {"coat": "#EEEEEE", "iris": "#88AAFF"}}"##.utf8)
    check((try? CustomSkins.save(Data("{}".utf8))) == nil && CustomSkins.all().isEmpty, "invalid skins are not saved")
    if let saved = try? CustomSkins.save(valid) {
        check(CustomSkins.all().map(\.spec.name) == ["Mochi"] && CustomSkins.spec(saved.id)?.isLonghair == true,
              "saved skins are listed and load")
        check(SkinChoice(setting: SkinChoice.custom(saved.id).setting) == .custom(saved.id)
              && SkinChoice(setting: "ragdoll") == .builtin(.ragdoll), "skin choices round-trip through settings")
        CustomSkins.delete(saved.id)
        check(CustomSkins.all().isEmpty, "deleted skins are gone")
    } else {
        check(false, "saves a valid skin")
    }
}

do {
    // A three-legged cat: a missing front paw stays tucked in every pose. The cat's right paw is the far one, drawn
    // on the viewer's left; its left paw is the near one.
    let black = BuiltinSkins.spec(.black)
    var noRight = black; noRight.missingFrontPaws = [.right]
    var noLeft = black; noLeft.missingFrontPaws = [.left]
    var lookDown = Pose.awake; lookDown.cursor = CGPoint(x: -60, y: 120)
    let braced = goldenPoses().first { $0.name == "braced" }!.pose
    for (name, pose) in [("awake", lookDown), ("braced", braced)] {
        var farTucked = pose; farTucked.pawTuckLeft = 1
        var nearTucked = pose; nearTucked.pawTuckRight = 1
        check(render(pose, spec: noRight) == render(farTucked, spec: black), "a missing right paw stays tucked (\(name))")
        check(render(pose, spec: noLeft) == render(nearTucked, spec: black), "a missing left paw stays tucked (\(name))")
        check(render(pose, spec: noRight) != render(pose, spec: black), "a missing paw changes the drawing (\(name))")
    }
    let farPaw = CGPoint(x: 71, y: 84)
    let headroom = CGFloat(CatGeometry.topPadding)
    check(CatRenderer.hitsPettingArea(farPaw, lookDown, spec: black, headroom: headroom), "the far paw is pettable")
    check(!CatRenderer.hitsPettingArea(farPaw, lookDown, spec: noRight, headroom: headroom), "a missing paw is not")
}

poofTests()

print(failures == 0 ? "OK" : "\(failures) failure(s)")
exit(failures == 0 ? 0 : 1)
