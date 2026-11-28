import Foundation

func updateTests() {
    check(Updates.isNewer("v0.2.2", than: "0.2.1"), "a later patch is newer")
    check(Updates.isNewer("v0.10.0", than: "0.9.9"), "versions compare as numbers, not text")
    check(Updates.isNewer("v1.0", than: "0.9.9"), "a missing part counts as zero")
    check(!Updates.isNewer("v0.2.1", than: "0.2.1"), "the same version is not newer")
    check(!Updates.isNewer("v0.2.1", than: "0.2.1-dev"), "a dev build of the same version is not behind")
    check(!Updates.isNewer("v0.2.0", than: "0.2.1"), "an older release is not newer")
    check(!Updates.isNewer("nightly", than: "0.2.1"), "a tag that isn't a version is never newer")

    check(Updates.latestTag(from: Data(#"{"tag_name": "v0.2.1", "name": "MeltyCat 0.2.1"}"#.utf8)) == "v0.2.1",
          "reads the tag of the latest release")
    check(Updates.latestTag(from: Data(#"{"message": "Not Found"}"#.utf8)) == nil, "no tag in an error reply")
    check(Updates.latestTag(from: Data("<html>".utf8)) == nil, "no tag in a reply that isn't JSON")

    let now = Date(timeIntervalSince1970: 1_000_000)
    let hours = { (h: Double) in now.addingTimeInterval(-h * 3600) }
    check(Updates.autoCheckDue(now: now, last: nil, enabled: true, current: "0.2.1"), "checks on the first launch")
    check(!Updates.autoCheckDue(now: now, last: hours(23), enabled: true, current: "0.2.1"), "at most once a day")
    check(Updates.autoCheckDue(now: now, last: hours(25), enabled: true, current: "0.2.1"), "checks again after a day")
    check(!Updates.autoCheckDue(now: now, last: nil, enabled: false, current: "0.2.1"), "never when turned off")
    check(!Updates.autoCheckDue(now: now, last: nil, enabled: true, current: "0.0.0"),
          "untagged builds don't nag about the latest release")
}
