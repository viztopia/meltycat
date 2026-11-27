#pragma once

#include "cat.h"

#include <chrono>
#include <optional>
#include <random>

namespace cat {

class Animation {
public:
    using Clock = std::chrono::steady_clock;

    void Observe(Clock::time_point now, POINT cursor, std::optional<DWORD> inputTick, float scale, bool overCat);
    Pose Sample(Clock::time_point now, Gdiplus::PointF cursor, float tailDirection, bool newPerch);

private:
    enum class HuntStage { Ordinary, LevelOne, LevelTwo, LevelThree };

    void BeginSleep(Clock::time_point now);
    void ObserveHunt(Clock::time_point now, float elapsed, Gdiplus::PointF delta, float distance);
    void ObservePetting(Clock::time_point now, POINT cursor, float scale, float elapsed, float distance, bool overCat);
    std::string_view AdvanceChatter(float dt);

    std::optional<Clock::time_point> lastObservation_;
    std::optional<Clock::time_point> stationaryInputAt_;
    std::optional<Clock::time_point> lastActivity_;
    std::optional<Clock::time_point> lastFrame_;
    std::optional<DWORD> previousInputTick_;
    POINT previousCursor_{};
    float mouseTravel_ = 0;
    float fastMotionTime_ = 0;
    float fastTravel_ = 0;
    Gdiplus::PointF fastDisplacement_{};
    HuntStage huntStage_ = HuntStage::Ordinary;
    Clock::time_point lastHuntMotion_{};
    Clock::time_point lastLevelTwoFastSample_{};
    float stageMotionTime_ = 0;

    std::optional<Clock::time_point> petStarted_;
    Clock::time_point lastPetMotion_{};
    Clock::time_point lastPetEligible_{};
    Clock::time_point nextPetSymbol_{};
    POINT petAnchor_{};
    float petTravel_ = 0;
    bool petting_ = false;
    float squint_ = 0;
    bool nextPetIsNote_ = false;
    bool petIsNote_ = false;
    Clock::time_point petSymbolsStarted_{};
    int petSymbolCount_ = 0;

    Clock::time_point sleepStarted_{};
    Gdiplus::PointF gaze_{};
    float alert_ = 0;
    float hunt_ = 0;
    float brace_ = 0;
    float chatter_ = 0;
    float chatterAge_ = 0;
    int chatterWord_ = 0;
    std::size_t chatterLetter_ = 0;
    int foodRepeats_ = 0;
    float tailTipPhase_ = 0;
    float tailMotion_ = 1;
    float animationTime_ = 0;
    std::mt19937 random_{std::random_device{}()};
    std::uniform_real_distribution<float> curlInterval_{10.5f, 21.0f};
    std::uniform_real_distribution<float> blinkInterval_{3.0f, 7.0f};
    std::uniform_real_distribution<float> dreamInterval_{5.0f, 8.0f};
    std::uniform_real_distribution<float> earTwitchInterval_{10.0f, 15.0f};
    std::uniform_int_distribution<int> firstPetSymbolDelayMs_{4000, 6000};
    std::uniform_int_distribution<int> petSymbolIntervalMs_{4000, 8000};
    std::bernoulli_distribution longPetPause_{0.25};
    std::bernoulli_distribution pickPetNote_{0.2};
    std::uniform_int_distribution<int> chatterAlternate_{1, 3};
    std::uniform_int_distribution<int> dreamSize_{1, 3};
    std::uniform_int_distribution<int> earTwitchSize_{1, 2};
    std::uniform_int_distribution<int> petHeartSize_{1, 2};
    std::bernoulli_distribution twitchRightEar_{0.5};
    std::uniform_int_distribution<int> pawTuckRoll_{0, 9};
    std::uniform_real_distribution<float> pawTuckDelay_{1.5f, 4.0f};
    std::uniform_real_distribution<float> tailPoseInterval_{28.0f, 52.0f};
    std::uniform_int_distribution<int> waveCycleCount_{1, 2};
    float nextCurl_ = curlInterval_(random_);
    float nextBlink_ = blinkInterval_(random_);
    float nextTailPose_ = tailPoseInterval_(random_);
    float tailPoseStarted_ = -100.0f;
    int waveCycles_ = 1;
    float nextDream_ = 0;
    float dreamStarted_ = 0;
    int dreamCount_ = 0;
    float nextEarTwitch_ = 0;
    float earTwitchStarted_ = 0;
    float earTwitchSide_ = 0;
    int earTwitchCount_ = 0;
    float pawTuckAt_ = 0;
    bool tuckLeft_ = false;
    bool tuckRight_ = false;
    float pawTuckLeft_ = 0;
    float pawTuckRight_ = 0;
};

} // namespace cat
