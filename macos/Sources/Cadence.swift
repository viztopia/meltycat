import Foundation

// Full rate whenever something visibly moves, as the Windows build always runs at 60 Hz. A swaying tail at 30 Hz
// looks stepped; only a still, awake cat (blinks and slow gaze) drops to half rate. A sleeping cat's tail keeps
// settling for seconds after it nods off, so the rate drops only once the tail is still and no Zs or twitches show.
func frameInterval(perched: Bool, pose: Pose?, secondsSinceInput: TimeInterval) -> TimeInterval {
    guard perched, let pose else { return 0.25 }
    let busy = secondsSinceInput < 0.8 || pose.hunt > 0 || pose.brace > 0 || pose.chatter > 0 || pose.squint > 0
        || pose.petSymbolCount > 0 || pose.tailMotion > 0 || pose.dreamCount > 0 || pose.earTwitch > 0
    if busy { return 1.0 / 60 }
    return pose.alert > 0 ? 1.0 / 30 : 1.0 / 15
}
