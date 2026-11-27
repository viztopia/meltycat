import CoreGraphics

func poofTests() {
    // Appearing and leaving wait until the perch has settled, so a window hovering at an edge doesn't puff
    // over and over; moving to another window happens at once.
    var presence = PerchPresence()
    check(presence.update(now: 0, observed: 7, holding: false) == nil, "waits before the first appearance")
    check(presence.update(now: 0.05, observed: 7, holding: false) == nil, "still settling")
    check(presence.update(now: 0.13, observed: 7, holding: false) == .appear, "appears once settled")
    check(presence.shown == 7, "shows the settled window")
    check(presence.update(now: 0.2, observed: 7, holding: false) == nil, "nothing while the perch holds")
    check(presence.update(now: 0.3, observed: nil, holding: false) == nil, "keeps the cat through a brief loss")
    check(presence.update(now: 0.35, observed: 7, holding: false) == nil, "a perch that comes back is no event")
    check(presence.update(now: 0.5, observed: nil, holding: false) == nil, "loses the perch again")
    check(presence.update(now: 0.63, observed: nil, holding: false) == .disappear, "leaves once the loss holds")
    check(presence.shown == nil, "shows nothing after leaving")
    check(presence.update(now: 0.7, observed: 9, holding: false) == nil, "settling on a new window")
    check(presence.update(now: 0.9, observed: 9, holding: false) == .appear, "appears on the new window")
    check(presence.update(now: 1.0, observed: 4, holding: false) == .move, "jumps to another window at once")
    check(presence.shown == 4, "follows the jump")

    // While a window is being dragged the button stays down; a cat that has left waits for the drop.
    var dragged = PerchPresence()
    _ = dragged.update(now: 0, observed: 3, holding: false)
    _ = dragged.update(now: 0.2, observed: 3, holding: false)
    _ = dragged.update(now: 0.3, observed: nil, holding: true)
    check(dragged.update(now: 0.45, observed: nil, holding: true) == .disappear, "leaves even while dragging")
    check(dragged.update(now: 0.5, observed: 3, holding: true) == nil, "dragged back into place")
    check(dragged.update(now: 0.9, observed: 3, holding: true) == nil, "stays away while the button is held")
    check(dragged.update(now: 0.92, observed: 3, holding: false) == .appear, "comes back as soon as the window is dropped")

    // The cloud hides the swap, but stays light: few billows of uneven size that break up at different times.
    check(poofCatAlpha(appearing: true, progress: 0) == 0 && poofCatAlpha(appearing: true, progress: 1) == 1,
          "an appearing cat goes from hidden to shown")
    check(poofCatAlpha(appearing: false, progress: 0) == 1 && poofCatAlpha(appearing: false, progress: 1) == 0,
          "a leaving cat goes from shown to hidden")
    check(poofCatAlpha(appearing: true, progress: poofPeak - 0.09) < 0.1
          && poofCatAlpha(appearing: true, progress: poofPeak + 0.09) > 0.9, "the cat is swapped around the peak")
    let peak = poofPuffs(progress: poofPeak)
    for point in [CGPoint(x: 56, y: 53), CGPoint(x: 110, y: 58), CGPoint(x: 107, y: 85), CGPoint(x: 150, y: 100)] {
        check(peak.contains { hypot($0.center.x - point.x, $0.center.y - point.y) < $0.radius },
              "the cloud covers the cat at \(point) while it is swapped")
    }
    let billows = peak.filter { $0.radius > 8 }
    check(billows.count <= 7, "a few billows, not a crowd: \(billows.count)")
    let radii = billows.map(\.radius)
    check(radii.max()! / radii.min()! >= 2, "billows of clearly different sizes")
    check(Set(radii.map { Int($0) }).count >= 5, "hardly any two billows the same size")
    check(peak.count - billows.count <= 4, "only a few small wisps")
    let breaking = poofPuffs(progress: 0.6).map(\.radius)
    let ratios = zip(breaking, peak.map(\.radius)).filter { $0.1 > 8 }.map { $0.0 / $0.1 }
    // Billows only shrink halfway before dissolving, so staggered timing shows as a spread of about 0.15.
    check(ratios.max()! - ratios.min()! > 0.12, "billows break up at different times, not in step")
    check(poofPuffs(progress: 0).allSatisfy { $0.radius < 0.5 }, "the cloud starts from nothing")
    check(poofPuffs(progress: 1).allSatisfy { $0.radius < 0.5 || $0.alpha < 0.01 }, "the cloud is gone at the end")
}
