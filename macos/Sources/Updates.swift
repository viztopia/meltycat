import Foundation

// Asks GitHub for the latest release's tag. Only that one request is made; nothing about the user is sent.
enum Updates {
    static let latestRelease = URL(string: "https://api.github.com/repos/viztopia/meltycat/releases/latest")!
    static let downloadPage = URL(string: "https://github.com/viztopia/meltycat/releases/latest")!
    static let interval: TimeInterval = 24 * 60 * 60

    static var current: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0"
    }

    // "v0.2.1" and "0.2.1-dev" both read as [0, 2, 1]; anything else reads as no version.
    static func parts(_ version: String) -> [Int]? {
        let core = version.drop(while: { $0 == "v" }).prefix(while: { $0.isNumber || $0 == "." })
        let numbers = core.split(separator: ".").map { Int($0) }
        guard !numbers.isEmpty, !numbers.contains(nil) else { return nil }
        return numbers.compactMap { $0 }
    }

    static func isNewer(_ tag: String, than current: String) -> Bool {
        guard let new = parts(tag), let old = parts(current) else { return false }
        for i in 0..<max(new.count, old.count) {
            let a = i < new.count ? new[i] : 0, b = i < old.count ? old[i] : 0
            if a != b { return a > b }
        }
        return false
    }

    static func latestTag(from data: Data) -> String? {
        let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        return object?["tag_name"] as? String
    }

    // CI builds without a tag are 0.0.0 and would always look out of date.
    static func autoCheckDue(now: Date, last: Date?, enabled: Bool, current: String) -> Bool {
        guard enabled, current != "0.0.0" else { return false }
        guard let last else { return true }
        return now.timeIntervalSince(last) >= interval
    }

    static func fetchLatestTag(completion: @escaping (String?) -> Void) {
        var request = URLRequest(url: latestRelease, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 15)
        request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        request.setValue("meltycat", forHTTPHeaderField: "User-Agent")
        URLSession(configuration: .ephemeral).dataTask(with: request) { data, response, _ in
            let ok = (response as? HTTPURLResponse)?.statusCode == 200
            let tag = ok ? data.flatMap(latestTag(from:)) : nil
            DispatchQueue.main.async { completion(tag) }
        }.resume()
    }
}
