import CoreGraphics

// Shared by the golden generator and the regression test, so both render the same poses.
func goldenPoses() -> [(name: String, pose: Pose, headroom: CGFloat)] {
    var lookUp = Pose.awake
    lookUp.cursor = CGPoint(x: 150, y: 20)
    var lookDown = Pose.awake
    lookDown.cursor = CGPoint(x: -100, y: 120); lookDown.swing = 0.6; lookDown.curl = 0.5
    lookDown.tailWave = 0.5; lookDown.tailWavePhase = 1
    var asleep = Pose.awake
    asleep.alert = 0; asleep.eyeOpen = 0; asleep.tailMotion = 0; asleep.dreamCount = 2; asleep.dreamAge = 0.9
    asleep.earTwitch = 0.7; asleep.pawTuckLeft = 0.5; asleep.pawTuckRight = 1
    var petting = Pose.awake
    petting.squint = 1; petting.eyeOpen = 0.07; petting.petSymbolCount = 2; petting.petSymbolAge = 0.6
    var braced = Pose.awake
    braced.hunt = 1; braced.brace = 1; braced.tailTwitch = 0.4; braced.cursor = CGPoint(x: 120, y: -40)
    var chatter = braced
    chatter.chatter = 1; chatter.chatterCode = "-.-"; chatter.chatterAge = 0.3
    var folded = Pose.awake
    folded.tailDirection = -1; folded.swing = 0.4
    return [("look-up", lookUp, 24), ("look-down", lookDown, 24), ("asleep", asleep, 24), ("petting", petting, 24),
            ("braced", braced, 24), ("chatter", chatter, 24), ("folded", folded, 24), ("braced-low", braced, 8)]
}
