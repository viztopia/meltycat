#include "animation.h"

#include <algorithm>
#include <cmath>

namespace cat {
namespace {

using namespace std::chrono_literals;

constexpr auto idleDuration = 6s;
constexpr auto inputPairWindow = 700ms;
constexpr float mouseWakeDistance = 20.0f;
constexpr float huntSpeed = 1450.0f;
constexpr float huntMotionTime = 0.115f;
constexpr float sustainHuntSpeed = 700.0f;
constexpr float sustainHuntMotionTime = 0.085f;
constexpr auto huntHold = 1250ms;
constexpr auto huntMotionGap = 240ms;
constexpr float levelTwoDelay = 3.0f;
constexpr float levelThreeDelay = 5.0f;
constexpr auto levelTwoHold = 2s;
constexpr auto levelTwoResumeGrace = 120ms;
constexpr float huntRelease = 0.55f;
constexpr float braceReleaseSpeed = 0.7f;
constexpr float chatterLetterPause = 0.13f;
constexpr float chatterWordPause = 0.62f;
constexpr float pi = 3.14159265f;
constexpr float tailFadeStarts = 5.0f;
constexpr float tailStops = 12.5f;
constexpr float earTwitchDuration = 0.15f;
constexpr float earTwitchSpacing = 0.22f;
constexpr float maxPetSpeed = 460.0f;
constexpr float petStep = 2.5f;
constexpr float minPetTravel = 8.0f;
constexpr auto petOnset = 3s;
constexpr auto petMotionGap = 400ms;
constexpr auto petSlipGrace = 350ms;
constexpr float petSquintClose = 0.28f;
constexpr float petSquintOpen = 1.0f;
constexpr float pawTuckDuration = 0.7f;
constexpr float pawUntuckDuration = 0.25f;
constexpr float waveCycleDuration = 1.1f;
constexpr float waveFadeDuration = 1.4f;

constexpr std::string_view chatterWords[]{"food", "chicken", "salmon", "fish"};
constexpr std::string_view morseLetters[]{
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
    "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..",
};

float Approach(float value, float target, float step) {
    return value + std::clamp(target - value, -step, step);
}

} // namespace

void Animation::BeginSleep(Clock::time_point now) {
    sleepStarted_ = now;
    nextDream_ = tailStops + dreamInterval_(random_);
    dreamCount_ = 0;
    nextEarTwitch_ = nextDream_ + earTwitchInterval_(random_);
    earTwitchCount_ = 0;
    // 40% of naps keep both paws out, 30% tuck one, 30% tuck both, all before the tail begins to slow.
    const int roll = pawTuckRoll_(random_);
    pawTuckAt_ = pawTuckDelay_(random_);
    tuckRight_ = roll >= 7 || (roll >= 4 && pickRightPaw_(random_));
    tuckLeft_ = roll >= 7 || (roll >= 4 && !tuckRight_);
}

void Animation::ObserveHunt(Clock::time_point now, float elapsed, Gdiplus::PointF delta, float distance) {
    const auto gap = now - lastHuntMotion_;
    if (gap > huntMotionGap) {
        stageMotionTime_ = 0;
        if (huntStage_ == HuntStage::LevelThree) huntStage_ = HuntStage::LevelTwo;
    }
    if (huntStage_ == HuntStage::LevelTwo && gap >= levelTwoHold &&
        now - lastLevelTwoFastSample_ > levelTwoResumeGrace) {
        huntStage_ = HuntStage::Ordinary;
        fastMotionTime_ = 0;
        fastTravel_ = 0;
        fastDisplacement_ = {};
    }
    if (huntStage_ == HuntStage::LevelOne && gap >= huntHold) huntStage_ = HuntStage::Ordinary;

    const float recent = std::exp(-elapsed / 0.22f);
    fastMotionTime_ *= recent;
    fastTravel_ *= recent;
    fastDisplacement_.X *= recent;
    fastDisplacement_.Y *= recent;

    // Once the cat is hunting, ordinary changes of pace within a shake can sustain it.
    const bool hunting = huntStage_ != HuntStage::Ordinary;
    const float speed = hunting ? sustainHuntSpeed : huntSpeed;
    if (elapsed <= 0 || elapsed > 0.08f || distance < speed * elapsed) return;

    // Give a shake that started just before the hold expired time to reverse direction.
    if (huntStage_ == HuntStage::LevelTwo && gap < levelTwoHold) lastLevelTwoFastSample_ = now;

    // A quick sweep in one direction has nearly the same travel and displacement.
    const float weight = std::min(1.0f, 4200.0f * elapsed / distance);
    fastTravel_ += distance * weight;
    fastDisplacement_.X += delta.X * weight;
    fastDisplacement_.Y += delta.Y * weight;
    fastMotionTime_ += std::min(elapsed, 0.036f);
    const float motionTime = hunting ? sustainHuntMotionTime : huntMotionTime;
    const float displacementRatio = hunting ? 0.86f : 0.72f;
    if (fastMotionTime_ >= motionTime &&
        std::hypot(fastDisplacement_.X, fastDisplacement_.Y) < displacementRatio * fastTravel_) {
        if (huntStage_ == HuntStage::Ordinary) {
            huntStage_ = HuntStage::LevelOne;
        } else if (huntStage_ != HuntStage::LevelThree) {
            // Tolerate brief reversals, but only count samples that meet the hunt conditions.
            stageMotionTime_ += elapsed;
            const bool levelOne = huntStage_ == HuntStage::LevelOne;
            if (stageMotionTime_ >= (levelOne ? levelTwoDelay : levelThreeDelay)) {
                huntStage_ = levelOne ? HuntStage::LevelTwo : HuntStage::LevelThree;
                stageMotionTime_ = 0;
            }
        }
        lastHuntMotion_ = now;
    }
}

void Animation::ObservePetting(Clock::time_point now, POINT cursor, float scale, float elapsed, float distance,
                               bool overCat) {
    const bool eligible = overCat && elapsed > 0 && elapsed <= 0.15f && distance <= maxPetSpeed * elapsed;
    if (eligible) lastPetEligible_ = now;
    // A stroke in progress forgives a brief slip off the head or a short pause.
    const bool slipped = !eligible && (!petStarted_ || now - lastPetEligible_ > petSlipGrace);
    const auto motionGap = petStarted_ ? petMotionGap + petSlipGrace : petMotionGap;
    if (slipped || (petStarted_ && now - lastPetMotion_ > motionGap)) {
        petStarted_.reset();
        petting_ = false;
    }
    if (!eligible || distance == 0) return;

    if (!petStarted_) {
        petStarted_ = lastPetMotion_ = now;
        petAnchor_ = cursor;
        petTravel_ = 0;
        return;
    }
    // Measure from the last accepted position so single-pixel tremors cannot sustain a stroke.
    const float step = std::hypot((cursor.x - petAnchor_.x) / scale, (cursor.y - petAnchor_.y) / scale);
    if (step < petStep) return;
    lastPetMotion_ = now;
    petAnchor_ = cursor;
    if (!petting_) {
        petTravel_ += step;
        if (petTravel_ < minPetTravel || now - *petStarted_ < petOnset) return;
        petting_ = true;
        nextPetIsNote_ = false;
        nextPetSymbol_ = now + std::chrono::milliseconds(firstPetSymbolDelayMs_(random_));
        return;
    }
    if (now < nextPetSymbol_) return;
    petSymbolsStarted_ = now;
    petIsNote_ = nextPetIsNote_;
    petSymbolCount_ = petIsNote_ ? 1 : petHeartSize_(random_);
    nextPetIsNote_ = pickPetNote_(random_);
    const auto interval = std::chrono::milliseconds(petSymbolIntervalMs_(random_));
    nextPetSymbol_ = now + interval + (longPetPause_(random_) ? 4s : 0s);
}

void Animation::Observe(Clock::time_point now, POINT cursor, std::optional<DWORD> inputTick, float scale,
                        bool overCat) {
    if (lastObservation_) {
        const float elapsed = std::chrono::duration<float>(now - *lastObservation_).count();
        const Gdiplus::PointF delta{(cursor.x - previousCursor_.x) / scale, (cursor.y - previousCursor_.y) / scale};
        const float distance = std::hypot(delta.X, delta.Y);
        mouseTravel_ = mouseTravel_ * std::exp(-elapsed / 0.38f) + distance;
        ObservePetting(now, cursor, scale, elapsed, distance, overCat);
        if (petting_) {
            huntStage_ = HuntStage::Ordinary;
            fastMotionTime_ = 0;
            fastTravel_ = 0;
            fastDisplacement_ = {};
            stageMotionTime_ = 0;
        } else {
            ObserveHunt(now, elapsed, delta, distance);
        }

        // Session input includes mouse, buttons, wheel and keyboard without exposing keys.
        const bool newInput = inputTick && previousInputTick_ && *inputTick != *previousInputTick_;
        const bool stationaryInput = newInput && distance == 0;
        const bool inputBurst = stationaryInput && stationaryInputAt_ &&
                                now - *stationaryInputAt_ <= inputPairWindow;
        if (stationaryInput) stationaryInputAt_ = now;

        if (lastActivity_ && now - *lastActivity_ >= idleDuration) {
            BeginSleep(*lastActivity_ + idleDuration);
            lastActivity_.reset();
        }
        const bool mouseBurst = distance > 0 && mouseTravel_ >= mouseWakeDistance;
        if (petting_ || mouseBurst || inputBurst || (lastActivity_ && (newInput || distance > 0))) {
            lastActivity_ = now;
        }
    } else {
        BeginSleep(now);
    }
    previousCursor_ = cursor;
    if (inputTick) previousInputTick_ = inputTick;
    lastObservation_ = now;
}

std::string_view Animation::AdvanceChatter(float dt) {
    const auto word = chatterWords[chatterWord_];
    const auto code = morseLetters[word[chatterLetter_] - 'a'];
    const bool lastLetter = chatterLetter_ + 1 == word.size();
    const float visibleTime = chatterMarkLifetime + chatterMarkStagger * static_cast<float>(code.size() - 1);
    const float letterTime = visibleTime + (lastLetter ? chatterWordPause : chatterLetterPause);
    chatterAge_ += dt;
    if (chatterAge_ < visibleTime) return code;
    if (chatterAge_ < letterTime) return {};

    chatterAge_ -= letterTime;
    if (lastLetter) {
        chatterLetter_ = 0;
        if (chatterWord_ == 0 && ++foodRepeats_ == 3) {
            chatterWord_ = chatterAlternate_(random_);
            foodRepeats_ = 0;
        } else {
            chatterWord_ = 0;
        }
    } else {
        ++chatterLetter_;
    }
    return morseLetters[chatterWords[chatterWord_][chatterLetter_] - 'a'];
}

Pose Animation::Sample(Clock::time_point now, Gdiplus::PointF cursor, float tailDirection, bool newPerch) {
    const float elapsed = lastFrame_ ? std::chrono::duration<float>(now - *lastFrame_).count() : 0.0f;
    const float dt = std::min(elapsed, 0.08f);
    lastFrame_ = now;

    const bool awake = lastActivity_.has_value();
    const bool hunting = awake && huntStage_ != HuntStage::Ordinary;
    const bool chattering = awake && huntStage_ == HuntStage::LevelThree;
    const bool bracing = chattering || (awake && huntStage_ == HuntStage::LevelTwo);

    if (newPerch) {
        gaze_ = cursor;
        squint_ = 0;
        petSymbolCount_ = 0;
        if (!awake) alert_ = 0;
        if (!hunting) hunt_ = 0;
        if (!bracing) brace_ = 0;
        if (!chattering) chatter_ = 0;
    }
    const float follow = 1.0f - std::exp(-19.0f * dt);
    gaze_.X += (cursor.X - gaze_.X) * follow;
    gaze_.Y += (cursor.Y - gaze_.Y) * follow;

    alert_ = Approach(alert_, awake ? 1.0f : 0.0f, dt / (awake ? 0.24f : 0.48f));
    const float previousHunt = hunt_;
    const float release = brace_ > 0 ? huntRelease / braceReleaseSpeed : huntRelease;
    hunt_ = awake ? Approach(hunt_, hunting ? 1.0f : 0.0f, dt / (hunting ? 0.18f : release)) : 0.0f;
    const float hunt = hunt_ * hunt_ * (3.0f - 2.0f * hunt_);
    if (bracing) {
        brace_ = Approach(brace_, 1.0f, dt / 1.25f);
    } else if (!hunting && previousHunt > 0) {
        // A partly raised body relaxes over the same interval as the pupils.
        brace_ *= hunt_ / previousHunt;
    } else {
        brace_ = Approach(brace_, 0.0f, dt / release);
    }
    const float brace = brace_ * brace_ * (3.0f - 2.0f * brace_);
    tailTipPhase_ = brace > 0 ? std::fmod(tailTipPhase_ + dt * (5.0f + 34.0f * brace), 2.0f * pi) : 0.0f;
    const float tailTwitch = 0.62f * brace * (0.45f + 0.55f * brace) * std::sin(tailTipPhase_);

    chatter_ = Approach(chatter_, chattering ? 1.0f : 0.0f, dt / (chattering ? 0.16f : 0.23f));
    std::string_view chatterCode;
    if (chattering) {
        chatterCode = AdvanceChatter(dt);
    } else {
        chatterAge_ = 0;
        chatterWord_ = 0;
        chatterLetter_ = 0;
        foodRepeats_ = 0;
    }

    float dreamAge = 0;
    int visibleDreams = 0;
    float earTwitch = 0;
    bool tuckPaws = false;
    if (awake) {
        tailMotion_ = Approach(tailMotion_, 1.0f, dt / 0.35f);
    } else {
        const float sleepAge = std::chrono::duration<float>(now - sleepStarted_).count();
        const float settle = std::clamp((sleepAge - tailFadeStarts) / (tailStops - tailFadeStarts), 0.0f, 1.0f);
        tailMotion_ = 1.0f - settle * settle * (3.0f - 2.0f * settle);
        tuckPaws = sleepAge >= pawTuckAt_;
        if (sleepAge >= nextDream_) {
            dreamStarted_ = nextDream_;
            dreamCount_ = dreamSize_(random_);
            nextDream_ = sleepAge + dreamInterval_(random_);
        }
        dreamAge = sleepAge - dreamStarted_;
        if (dreamCount_ > 0 && dreamAge < dreamLifetime + (dreamCount_ - 1) * dreamStagger) {
            visibleDreams = dreamCount_;
        }
        if (sleepAge >= nextEarTwitch_) {
            earTwitchStarted_ = nextEarTwitch_;
            nextEarTwitch_ = sleepAge + earTwitchInterval_(random_);
            earTwitchCount_ = earTwitchSize_(random_);
            earTwitchSide_ = twitchRightEar_(random_) ? 1.0f : -1.0f;
        }
        const float twitchAge = sleepAge - earTwitchStarted_;
        if (earTwitchCount_ > 0 && twitchAge < earTwitchCount_ * earTwitchSpacing) {
            const float phase = std::fmod(twitchAge, earTwitchSpacing) / earTwitchDuration;
            if (phase < 1.0f) earTwitch = earTwitchSide_ * std::sin(pi * phase);
        }
    }

    const float tuckStep = dt / (tuckPaws ? pawTuckDuration : pawUntuckDuration);
    pawTuckLeft_ = Approach(pawTuckLeft_, tuckPaws && tuckLeft_ ? 1.0f : 0.0f, tuckStep);
    pawTuckRight_ = Approach(pawTuckRight_, tuckPaws && tuckRight_ ? 1.0f : 0.0f, tuckStep);
    const auto smooth = [](float v) { return v * v * (3.0f - 2.0f * v); };

    if (tailMotion_ > 0) animationTime_ += dt;
    const float t = animationTime_;
    constexpr float curlDuration = 2.2f;
    constexpr float blinkDuration = 0.16f;
    if (t > nextCurl_ + curlDuration) nextCurl_ = t + curlInterval_(random_);
    if (t > nextBlink_ + blinkDuration) nextBlink_ = t + blinkInterval_(random_);

    const float curlPhase = std::clamp((t - nextCurl_) / curlDuration, 0.0f, 1.0f);
    const float blinkPhase = std::clamp((t - nextBlink_) / blinkDuration, 0.0f, 1.0f);
    const float curl = std::sin(pi * curlPhase);
    // Now and then the tail ripples once or twice, instead of its usual swing and curl.
    if (awake && t >= nextTailPose_) {
        waveCycles_ = waveCycleCount_(random_);
        tailPoseStarted_ = t;
        nextTailPose_ = t + waveCycles_ * waveCycleDuration + waveFadeDuration + tailPoseInterval_(random_);
    }
    const float poseDuration = waveCycles_ * waveCycleDuration + waveFadeDuration;
    const float posePhase = (t - tailPoseStarted_) / poseDuration;
    float poseAmount = 0;
    if (posePhase > 0 && posePhase < 1) {
        const float edge = std::min(1.0f, std::min(posePhase / 0.18f, (1.0f - posePhase) / 0.4f));
        poseAmount = edge * edge * (3.0f - 2.0f * edge) * tailMotion_ * (1.0f - curl * curl);
    }
    const float swing = 0.53f * std::sin(1.84f * t) + 0.23f * std::sin(2.73f * t + 0.6f);
    squint_ = Approach(squint_, petting_ ? 1.0f : 0.0f, dt / (petting_ ? petSquintClose : petSquintOpen));
    const float squint = squint_ * squint_ * (3.0f - 2.0f * squint_);
    const float blink = std::sin(pi * blinkPhase) * (1.0f - hunt) * (1.0f - squint);
    const float eyelid = alert_ * (1.0f - blink) * (1.0f - 0.93f * squint);
    float petSymbolAge = 0;
    if (petSymbolCount_ > 0) {
        petSymbolAge = std::chrono::duration<float>(now - petSymbolsStarted_).count();
        if (petSymbolAge >= petSymbolLifetime + (petSymbolCount_ - 1) * petSymbolStagger) petSymbolCount_ = 0;
    }
    // While the tail ripples, the ordinary swing eases down to half its reach.
    return {gaze_, swing * (1.0f - 0.5f * poseAmount) * tailMotion_, curl * curl * tailMotion_ * alert_, eyelid,
            tailDirection, alert_,
            tailMotion_, dreamAge, visibleDreams, hunt, brace, tailTwitch, chatter_,
            chatterCode.empty() ? -1.0f : chatterAge_, earTwitch, chatterCode, squint, petSymbolAge, petSymbolCount_,
            smooth(pawTuckLeft_), smooth(pawTuckRight_), poseAmount,
            6.2831853f * waveCycles_ * std::clamp(posePhase, 0.0f, 1.0f), petIsNote_};
}

} // namespace cat
