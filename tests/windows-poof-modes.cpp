#include "../src/poof.h"

#include <iostream>
#include <string_view>

namespace {

using namespace std::chrono_literals;

void Check(bool condition, std::string_view label, int& failures) {
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    failures += !condition;
}

void CheckQuiet(int& failures) {
    const PerchPresence::Clock::time_point start{};
    PoofGate gate;
    Check(gate.AllowsEffect(PerchEvent::Appear, start, PoofMode::Quiet),
          "the first appearance puffs", failures);
    Check(!gate.AllowsEffect(PerchEvent::Disappear, start + 20s, PoofMode::Quiet),
          "leaving soon after is instant", failures);
    Check(!gate.AllowsEffect(PerchEvent::Appear, start + 75s, PoofMode::Quiet),
          "a suppressed change restarts the quiet minute", failures);
    Check(!gate.AllowsEffect(PerchEvent::None, start + 134s, PoofMode::Quiet) &&
          gate.AllowsEffect(PerchEvent::Disappear, start + 135s, PoofMode::Quiet),
          "unchanged frames do not restart the minute and exactly 60 seconds is enough", failures);
    Check(!gate.AllowsEffect(PerchEvent::Appear, start + 136s, PoofMode::Quiet),
          "coming back soon after leaving is instant", failures);
    Check(!gate.AllowsEffect(PerchEvent::Move, start + 200s, PoofMode::Quiet),
          "a jump after a quiet minute is instant", failures);
    Check(!gate.AllowsEffect(PerchEvent::Disappear, start + 230s, PoofMode::Quiet),
          "a jump restarts the quiet minute", failures);
}

void CheckOtherModes(int& failures) {
    const PerchPresence::Clock::time_point start{};
    PoofGate always;
    Check(always.AllowsEffect(PerchEvent::Appear, start, PoofMode::Always) &&
          always.AllowsEffect(PerchEvent::Disappear, start + 1s, PoofMode::Always),
          "always puffs on consecutive appearances and disappearances", failures);
    Check(!always.AllowsEffect(PerchEvent::Move, start + 2s, PoofMode::Always),
          "always keeps a jump instant", failures);

    PoofGate off;
    Check(!off.AllowsEffect(PerchEvent::Appear, start, PoofMode::Off) &&
          !off.AllowsEffect(PerchEvent::Disappear, start + 100s, PoofMode::Off),
          "off stays instant even after a quiet minute", failures);
    Check(!off.AllowsEffect(PerchEvent::Appear, start + 101s, PoofMode::Quiet),
          "changes made while off count when switching to quiet", failures);
}

void CheckStyles(int& failures) {
    Check(PoofStyleFor(false, false) == PoofStyle::None, "no puff is instant", failures);
    Check(PoofStyleFor(false, true) == PoofStyle::None, "no puff is instant with reduced motion", failures);
    Check(PoofStyleFor(true, false) == PoofStyle::Cloud, "a puff shows a cloud", failures);
    Check(PoofStyleFor(true, true) == PoofStyle::Fade, "reduced motion replaces a cloud with a fade", failures);
}

} // namespace

int main() {
    int failures = 0;
    CheckQuiet(failures);
    CheckOtherModes(failures);
    CheckStyles(failures);
    return failures ? 1 : 0;
}
