import CoreGraphics
import Foundation

private func approach(_ value: CGFloat, _ target: CGFloat, _ step: CGFloat) -> CGFloat {
    value + min(max(target - value, -step), step)
}

private func clamp01(_ value: CGFloat) -> CGFloat { min(max(value, 0), 1) }

// random(in:using:) needs a concrete generator, so wrap the injected one.
struct AnyGenerator: RandomNumberGenerator {
    var base: any RandomNumberGenerator
    mutating func next() -> UInt64 { base.next() }
}

// Port of animation.cpp. Times are seconds on one monotonic clock.
final class Animation {
    private enum HuntStage { case ordinary, levelOne, levelTwo, levelThree }

    private static let idleDuration: TimeInterval = 6
    private static let inputPairWindow: TimeInterval = 0.7
    private static let mouseWakeDistance: CGFloat = 20
    private static let huntSpeed: CGFloat = 1450
    private static let huntMotionTime: CGFloat = 0.115
    private static let sustainHuntSpeed: CGFloat = 700
    private static let sustainHuntMotionTime: CGFloat = 0.085
    private static let huntHold: TimeInterval = 1.25
    private static let huntMotionGap: TimeInterval = 0.24
    private static let levelTwoDelay: CGFloat = 2
    private static let levelThreeDelay: CGFloat = 3.5
    private static let levelTwoHold: TimeInterval = 2
    private static let levelTwoResumeGrace: TimeInterval = 0.12
    private static let huntRelease: CGFloat = 0.55
    private static let braceReleaseSpeed: CGFloat = 0.7
    private static let chatterLetterPause: CGFloat = 0.13
    private static let chatterWordPause: CGFloat = 0.62
    private static let tailFadeStarts: CGFloat = 5
    private static let tailStops: CGFloat = 12.5
    private static let earTwitchDuration: CGFloat = 0.15
    private static let earTwitchSpacing: CGFloat = 0.22
    private static let maxPetSpeed: CGFloat = 460
    private static let petStep: CGFloat = 2.5
    private static let minPetTravel: CGFloat = 8
    private static let petOnset: TimeInterval = 3
    private static let petMotionGap: TimeInterval = 0.4
    private static let petSlipGrace: TimeInterval = 0.35
    private static let petSquintClose: CGFloat = 0.28
    private static let petSquintOpen: CGFloat = 1
    private static let pawTuckDuration: CGFloat = 0.7
    private static let pawUntuckDuration: CGFloat = 0.25
    private static let curlDuration: CGFloat = 2.2
    private static let blinkDuration: CGFloat = 0.16
    private static let waveCycleDuration: CGFloat = 1.1
    private static let waveFadeDuration: CGFloat = 1.4
    // The Mac reports seconds since the last input rather than a tick count, so allow for clock jitter.
    private static let inputJitter: TimeInterval = 0.004
    private static let longAgo: TimeInterval = -1e9

    private static let chatterWords = ["food", "chicken", "salmon", "fish"]
    private static let morseLetters = [
        ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
        "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..",
    ]

    private var random: AnyGenerator

    private var lastObservation: TimeInterval?
    private var stationaryInputAt: TimeInterval?
    private var lastActivity: TimeInterval?
    private var lastFrame: TimeInterval?
    private var previousInputTime: TimeInterval?
    private var previousCursor = CGPoint.zero
    private var mouseTravel: CGFloat = 0
    private var fastMotionTime: CGFloat = 0
    private var fastTravel: CGFloat = 0
    private var fastDisplacement = CGPoint.zero
    private var huntStage = HuntStage.ordinary
    private var lastHuntMotion = longAgo
    private var lastLevelTwoFastSample = longAgo
    private var stageMotionTime: CGFloat = 0

    private var petStarted: TimeInterval?
    private var lastPetMotion = longAgo
    private var lastPetEligible = longAgo
    private var nextPetSymbol = longAgo
    private var petAnchor = CGPoint.zero
    private var petTravel: CGFloat = 0
    private var petting = false
    private var squint: CGFloat = 0
    private var nextPetIsNote = false
    private var petIsNote = false
    private var petSymbolsStarted = longAgo
    private var petSymbolCount = 0

    private var sleepStarted: TimeInterval = 0
    private var gaze = CGPoint.zero
    private var alert: CGFloat = 0
    private var hunt: CGFloat = 0
    private var brace: CGFloat = 0
    private var chatter: CGFloat = 0
    private var chatterAge: CGFloat = 0
    private var chatterWord = 0
    private var chatterLetter = 0
    private var foodRepeats = 0
    private var tailTipPhase: CGFloat = 0
    private var tailMotion: CGFloat = 1
    private var animationTime: CGFloat = 0
    private var nextCurl: CGFloat = 0
    private var nextBlink: CGFloat = 0
    private var nextTailPose: CGFloat = 0
    private var tailPoseStarted: CGFloat = -100
    private var waveCycles = 1
    private var nextDream: CGFloat = 0
    private var dreamStarted: CGFloat = 0
    private var dreamCount = 0
    private var nextEarTwitch: CGFloat = 0
    private var earTwitchStarted: CGFloat = 0
    private var earTwitchSide: CGFloat = 0
    private var earTwitchCount = 0
    private var pawTuckAt: CGFloat = 0
    private var tuckLeft = false
    private var tuckRight = false
    private var pawTuckLeft: CGFloat = 0
    private var pawTuckRight: CGFloat = 0

    init(random: any RandomNumberGenerator = SystemRandomNumberGenerator()) {
        self.random = AnyGenerator(base: random)
        nextCurl = uniform(10.5...21)
        nextBlink = uniform(3...7)
        nextTailPose = uniform(28...52)
    }

    private func uniform(_ range: ClosedRange<CGFloat>) -> CGFloat { CGFloat.random(in: range, using: &random) }
    private func integer(_ range: ClosedRange<Int>) -> Int { Int.random(in: range, using: &random) }
    private func chance(_ probability: Double) -> Bool { Double.random(in: 0..<1, using: &random) < probability }

    private func beginSleep(_ now: TimeInterval) {
        sleepStarted = now
        nextDream = Self.tailStops + uniform(5...8)
        dreamCount = 0
        nextEarTwitch = nextDream + uniform(10...15)
        earTwitchCount = 0
        let roll = integer(0...9)
        pawTuckAt = uniform(1.5...4)
        (tuckLeft, tuckRight) = Self.pawTuck(roll: roll)
    }

    // 40% of naps keep both paws out, 20% tuck the left, 20% the right and 20% both, all before the tail slows.
    static func pawTuck(roll: Int) -> (left: Bool, right: Bool) {
        switch roll {
        case 4...5: (true, false)
        case 6...7: (false, true)
        case 8...9: (true, true)
        default: (false, false)
        }
    }

    private func observeHunt(_ now: TimeInterval, _ elapsed: CGFloat, _ delta: CGPoint, _ distance: CGFloat) {
        let gap = now - lastHuntMotion
        if gap > Self.huntMotionGap {
            stageMotionTime = 0
            if huntStage == .levelThree { huntStage = .levelTwo }
        }
        if huntStage == .levelTwo && gap >= Self.levelTwoHold && now - lastLevelTwoFastSample > Self.levelTwoResumeGrace {
            huntStage = .ordinary
            fastMotionTime = 0
            fastTravel = 0
            fastDisplacement = .zero
        }
        if huntStage == .levelOne && gap >= Self.huntHold { huntStage = .ordinary }

        let recent = exp(-elapsed / 0.22)
        fastMotionTime *= recent
        fastTravel *= recent
        fastDisplacement.x *= recent
        fastDisplacement.y *= recent

        // Once the cat is hunting, ordinary changes of pace within a shake can sustain it.
        let hunting = huntStage != .ordinary
        let speed = hunting ? Self.sustainHuntSpeed : Self.huntSpeed
        if elapsed <= 0 || elapsed > 0.08 || distance < speed * elapsed { return }

        // Give a shake that started just before the hold expired time to reverse direction.
        if huntStage == .levelTwo && gap < Self.levelTwoHold { lastLevelTwoFastSample = now }

        // A quick sweep in one direction has nearly the same travel and displacement.
        let weight = min(1, 4200 * elapsed / distance)
        fastTravel += distance * weight
        fastDisplacement.x += delta.x * weight
        fastDisplacement.y += delta.y * weight
        fastMotionTime += min(elapsed, 0.036)
        let motionTime = hunting ? Self.sustainHuntMotionTime : Self.huntMotionTime
        let displacementRatio: CGFloat = hunting ? 0.86 : 0.72
        if fastMotionTime >= motionTime && hypot(fastDisplacement.x, fastDisplacement.y) < displacementRatio * fastTravel {
            if huntStage == .ordinary {
                huntStage = .levelOne
            } else if huntStage != .levelThree {
                // Tolerate brief reversals, but only count samples that meet the hunt conditions.
                stageMotionTime += elapsed
                let levelOne = huntStage == .levelOne
                if stageMotionTime >= (levelOne ? Self.levelTwoDelay : Self.levelThreeDelay) {
                    huntStage = levelOne ? .levelTwo : .levelThree
                    stageMotionTime = 0
                }
            }
            lastHuntMotion = now
        }
    }

    private func observePetting(_ now: TimeInterval, _ cursor: CGPoint, _ scale: CGFloat, _ elapsed: CGFloat,
                                _ distance: CGFloat, _ overCat: Bool) {
        let eligible = overCat && elapsed > 0 && elapsed <= 0.15 && distance <= Self.maxPetSpeed * elapsed
        if eligible { lastPetEligible = now }
        // A stroke in progress forgives a brief slip off the head or a short pause.
        let slipped = !eligible && (petStarted == nil || now - lastPetEligible > Self.petSlipGrace)
        let motionGap = petStarted != nil ? Self.petMotionGap + Self.petSlipGrace : Self.petMotionGap
        if slipped || (petStarted != nil && now - lastPetMotion > motionGap) {
            petStarted = nil
            petting = false
        }
        if !eligible || distance == 0 { return }

        guard let started = petStarted else {
            petStarted = now
            lastPetMotion = now
            petAnchor = cursor
            petTravel = 0
            return
        }
        // Measure from the last accepted position so single-pixel tremors cannot sustain a stroke.
        let step = hypot((cursor.x - petAnchor.x) / scale, (cursor.y - petAnchor.y) / scale)
        if step < Self.petStep { return }
        lastPetMotion = now
        petAnchor = cursor
        if !petting {
            petTravel += step
            if petTravel < Self.minPetTravel || now - started < Self.petOnset { return }
            petting = true
            nextPetIsNote = false
            nextPetSymbol = now + Double(integer(4000...6000)) / 1000
            return
        }
        if now < nextPetSymbol { return }
        petSymbolsStarted = now
        petIsNote = nextPetIsNote
        petSymbolCount = petIsNote ? 1 : integer(1...2)
        nextPetIsNote = chance(0.2)
        let interval = Double(integer(4000...8000)) / 1000
        nextPetSymbol = now + interval + (chance(0.25) ? 4 : 0)
    }

    // cursor is in screen points with y down; inputTime is when the session last saw any input.
    func observe(now: TimeInterval, cursor: CGPoint, inputTime: TimeInterval?, scale: CGFloat, overCat: Bool) {
        if let last = lastObservation {
            let elapsed = CGFloat(now - last)
            let delta = CGPoint(x: (cursor.x - previousCursor.x) / scale, y: (cursor.y - previousCursor.y) / scale)
            let distance = hypot(delta.x, delta.y)
            mouseTravel = mouseTravel * exp(-elapsed / 0.38) + distance
            observePetting(now, cursor, scale, elapsed, distance, overCat)
            if petting {
                huntStage = .ordinary
                fastMotionTime = 0
                fastTravel = 0
                fastDisplacement = .zero
                stageMotionTime = 0
            } else {
                observeHunt(now, elapsed, delta, distance)
            }

            // Session input includes mouse, buttons, wheel and keyboard without exposing keys.
            var newInput = false
            if let inputTime, let previousInputTime { newInput = inputTime > previousInputTime + Self.inputJitter }
            let stationaryInput = newInput && distance == 0
            let inputBurst = stationaryInput && stationaryInputAt.map { now - $0 <= Self.inputPairWindow } == true
            if stationaryInput { stationaryInputAt = now }

            if let lastActivity, now - lastActivity >= Self.idleDuration {
                beginSleep(lastActivity + Self.idleDuration)
                self.lastActivity = nil
            }
            let mouseBurst = distance > 0 && mouseTravel >= Self.mouseWakeDistance
            if petting || mouseBurst || inputBurst || (lastActivity != nil && (newInput || distance > 0)) {
                lastActivity = now
            }
        } else {
            beginSleep(now)
        }
        previousCursor = cursor
        if let inputTime { previousInputTime = max(previousInputTime ?? inputTime, inputTime) }
        lastObservation = now
    }

    private func advanceChatter(_ dt: CGFloat) -> String {
        let word = Array(Self.chatterWords[chatterWord].utf8)
        let code = Self.morseLetters[Int(word[chatterLetter] - UInt8(ascii: "a"))]
        let lastLetter = chatterLetter + 1 == word.count
        let visibleTime = chatterMarkLifetime + chatterMarkStagger * CGFloat(code.count - 1)
        let letterTime = visibleTime + (lastLetter ? Self.chatterWordPause : Self.chatterLetterPause)
        chatterAge += dt
        if chatterAge < visibleTime { return code }
        if chatterAge < letterTime { return "" }

        chatterAge -= letterTime
        if lastLetter {
            chatterLetter = 0
            foodRepeats += chatterWord == 0 ? 1 : 0
            if chatterWord == 0 && foodRepeats == 3 {
                chatterWord = integer(1...3)
                foodRepeats = 0
            } else {
                chatterWord = 0
            }
        } else {
            chatterLetter += 1
        }
        let next = Array(Self.chatterWords[chatterWord].utf8)
        return Self.morseLetters[Int(next[chatterLetter] - UInt8(ascii: "a"))]
    }

    func sample(now: TimeInterval, cursor: CGPoint, tailDirection: CGFloat, newPerch: Bool) -> Pose {
        let elapsed = lastFrame.map { CGFloat(now - $0) } ?? 0
        let dt = min(elapsed, 0.08)
        lastFrame = now

        let awake = lastActivity != nil
        let hunting = awake && huntStage != .ordinary
        let chattering = awake && huntStage == .levelThree
        let bracing = chattering || (awake && huntStage == .levelTwo)

        if newPerch {
            gaze = cursor
            squint = 0
            petSymbolCount = 0
            if !awake { alert = 0 }
            if !hunting { hunt = 0 }
            if !bracing { brace = 0 }
            if !chattering { chatter = 0 }
        }
        let follow = 1 - exp(-19 * dt)
        gaze.x += (cursor.x - gaze.x) * follow
        gaze.y += (cursor.y - gaze.y) * follow

        alert = approach(alert, awake ? 1 : 0, dt / (awake ? 0.24 : 0.48))
        let previousHunt = hunt
        let release = brace > 0 ? Self.huntRelease / Self.braceReleaseSpeed : Self.huntRelease
        hunt = awake ? approach(hunt, hunting ? 1 : 0, dt / (hunting ? 0.18 : release)) : 0
        let smoothHunt = hunt * hunt * (3 - 2 * hunt)
        if bracing {
            brace = approach(brace, 1, dt / 1.25)
        } else if !hunting && previousHunt > 0 {
            // A partly raised body relaxes over the same interval as the pupils.
            brace *= hunt / previousHunt
        } else {
            brace = approach(brace, 0, dt / release)
        }
        let smoothBrace = brace * brace * (3 - 2 * brace)
        tailTipPhase = smoothBrace > 0
            ? fmod(tailTipPhase + dt * (5 + 34 * smoothBrace), 2 * .pi) : 0
        let tailTwitch = 0.62 * smoothBrace * (0.45 + 0.55 * smoothBrace) * sin(tailTipPhase)

        chatter = approach(chatter, chattering ? 1 : 0, dt / (chattering ? 0.16 : 0.23))
        var chatterCode = ""
        if chattering {
            chatterCode = advanceChatter(dt)
        } else {
            chatterAge = 0
            chatterWord = 0
            chatterLetter = 0
            foodRepeats = 0
        }

        var dreamAge: CGFloat = 0
        var visibleDreams = 0
        var earTwitch: CGFloat = 0
        var tuckPaws = false
        if awake {
            tailMotion = approach(tailMotion, 1, dt / 0.35)
        } else {
            let sleepAge = CGFloat(now - sleepStarted)
            let settle = clamp01((sleepAge - Self.tailFadeStarts) / (Self.tailStops - Self.tailFadeStarts))
            tailMotion = 1 - settle * settle * (3 - 2 * settle)
            tuckPaws = sleepAge >= pawTuckAt
            if sleepAge >= nextDream {
                dreamStarted = nextDream
                dreamCount = integer(1...3)
                nextDream = sleepAge + uniform(5...8)
            }
            dreamAge = sleepAge - dreamStarted
            if dreamCount > 0 && dreamAge < dreamLifetime + CGFloat(dreamCount - 1) * dreamStagger {
                visibleDreams = dreamCount
            }
            if sleepAge >= nextEarTwitch {
                earTwitchStarted = nextEarTwitch
                nextEarTwitch = sleepAge + uniform(10...15)
                earTwitchCount = integer(1...2)
                earTwitchSide = chance(0.5) ? 1 : -1
            }
            let twitchAge = sleepAge - earTwitchStarted
            if earTwitchCount > 0 && twitchAge < CGFloat(earTwitchCount) * Self.earTwitchSpacing {
                let phase = fmod(twitchAge, Self.earTwitchSpacing) / Self.earTwitchDuration
                if phase < 1 { earTwitch = earTwitchSide * sin(.pi * phase) }
            }
        }

        let tuckStep = dt / (tuckPaws ? Self.pawTuckDuration : Self.pawUntuckDuration)
        pawTuckLeft = approach(pawTuckLeft, tuckPaws && tuckLeft ? 1 : 0, tuckStep)
        pawTuckRight = approach(pawTuckRight, tuckPaws && tuckRight ? 1 : 0, tuckStep)
        let smooth = { (v: CGFloat) in v * v * (3 - 2 * v) }

        if tailMotion > 0 { animationTime += dt }
        let t = animationTime
        if t > nextCurl + Self.curlDuration { nextCurl = t + uniform(10.5...21) }
        if t > nextBlink + Self.blinkDuration { nextBlink = t + uniform(3...7) }

        let curlPhase = clamp01((t - nextCurl) / Self.curlDuration)
        let blinkPhase = clamp01((t - nextBlink) / Self.blinkDuration)
        let curl = sin(.pi * curlPhase)
        // Now and then the tail ripples once or twice, instead of its usual swing and curl.
        if awake && t >= nextTailPose {
            waveCycles = integer(1...2)
            tailPoseStarted = t
            nextTailPose = t + CGFloat(waveCycles) * Self.waveCycleDuration + Self.waveFadeDuration + uniform(28...52)
        }
        let poseDuration = CGFloat(waveCycles) * Self.waveCycleDuration + Self.waveFadeDuration
        let posePhase = (t - tailPoseStarted) / poseDuration
        var poseAmount: CGFloat = 0
        if posePhase > 0 && posePhase < 1 {
            let edge = min(1, min(posePhase / 0.18, (1 - posePhase) / 0.4))
            poseAmount = edge * edge * (3 - 2 * edge) * tailMotion * (1 - curl * curl)
        }
        let swing = 0.53 * sin(1.84 * t) + 0.23 * sin(2.73 * t + 0.6)
        squint = approach(squint, petting ? 1 : 0, dt / (petting ? Self.petSquintClose : Self.petSquintOpen))
        let smoothSquint = squint * squint * (3 - 2 * squint)
        let blink = sin(.pi * blinkPhase) * (1 - smoothHunt) * (1 - smoothSquint)
        let eyelid = alert * (1 - blink) * (1 - 0.93 * smoothSquint)
        var petSymbolAge: CGFloat = 0
        if petSymbolCount > 0 {
            petSymbolAge = CGFloat(now - petSymbolsStarted)
            if petSymbolAge >= petSymbolLifetime + CGFloat(petSymbolCount - 1) * petSymbolStagger { petSymbolCount = 0 }
        }

        var pose = Pose(cursor: gaze)
        // While the tail ripples, the ordinary swing eases down to half its reach.
        pose.swing = swing * (1 - 0.5 * poseAmount) * tailMotion
        pose.curl = curl * curl * tailMotion * alert
        pose.eyeOpen = eyelid
        pose.tailDirection = tailDirection
        pose.alert = alert
        pose.tailMotion = tailMotion
        pose.dreamAge = dreamAge
        pose.dreamCount = visibleDreams
        pose.hunt = smoothHunt
        pose.brace = smoothBrace
        pose.tailTwitch = tailTwitch
        pose.chatter = chatter
        pose.chatterAge = chatterCode.isEmpty ? -1 : chatterAge
        pose.earTwitch = earTwitch
        pose.chatterCode = chatterCode
        pose.squint = smoothSquint
        pose.petSymbolAge = petSymbolAge
        pose.petSymbolCount = petSymbolCount
        pose.pawTuckLeft = smooth(pawTuckLeft)
        pose.pawTuckRight = smooth(pawTuckRight)
        pose.tailWave = poseAmount
        pose.tailWavePhase = 2 * .pi * CGFloat(waveCycles) * clamp01(posePhase)
        pose.petIsNote = petIsNote
        return pose
    }
}
