import Foundation

// The built-in skins, kept with the saved values of the Windows build; the Ragdoll Mix is new on the Mac.
enum Skin {
    case black, uni, orangeTabby, tuxedo, liHua, ragdoll, ragdollMix, maineCoon, siberian

    static let menuOrder: [Skin] = [.ragdoll, .ragdollMix, .maineCoon, .siberian, .orangeTabby, .black, .liHua, .tuxedo, .uni]

    init?(setting: String) {
        guard let skin = Skin.menuOrder.first(where: { $0.setting == setting }) else { return nil }
        self = skin
    }

    var setting: String {
        switch self {
        case .ragdoll: "ragdoll"
        case .ragdollMix: "ragdoll-mix"
        case .maineCoon: "maine-coon"
        case .siberian: "siberian"
        case .orangeTabby: "orange-tabby"
        case .black: "black"
        case .liHua: "li-hua"
        case .tuxedo: "classic-tuxedo"
        case .uni: "tuxedo" // The original setting identifies Uni.
        }
    }

    var isLonghair: Bool { [.ragdoll, .ragdollMix, .maineCoon, .siberian].contains(self) }
}

enum BuiltinSkins {
    // The app bundle's Resources/skins; tests point it at the repository's skins folder.
    static var directory = Bundle.main.resourceURL?.appendingPathComponent("skins")
    private static var cache: [String: SkinSpec] = [:]

    static func spec(_ skin: Skin) -> SkinSpec {
        if let cached = cache[skin.setting] { return cached }
        guard let directory,
              let data = try? Data(contentsOf: directory.appendingPathComponent("\(skin.setting).meltycat")) else {
            fatalError("missing built-in skin \(skin.setting)")
        }
        do {
            let spec = try SkinSpec.parse(data)
            cache[skin.setting] = spec
            return spec
        } catch {
            fatalError("built-in skin \(skin.setting): \(error)")
        }
    }
}

// What the menu and settings refer to: a built-in skin or an imported file.
enum SkinChoice: Equatable {
    case builtin(Skin)
    case custom(String)

    private static let customPrefix = "custom:"

    init?(setting: String) {
        if setting.hasPrefix(Self.customPrefix) {
            self = .custom(String(setting.dropFirst(Self.customPrefix.count)))
        } else if let skin = Skin(setting: setting) {
            self = .builtin(skin)
        } else {
            return nil
        }
    }

    var setting: String {
        switch self {
        case .builtin(let skin): skin.setting
        case .custom(let id): Self.customPrefix + id
        }
    }
}

struct CustomSkin {
    var id: String
    var spec: SkinSpec
}

// Imported skins live as their original text in Application Support, one file each.
enum CustomSkins {
    static var directory = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first?
        .appendingPathComponent("meltycat/skins", isDirectory: true)

    static func all() -> [CustomSkin] {
        guard let directory,
              let files = try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil) else {
            return []
        }
        return files.filter { $0.pathExtension == "meltycat" }.sorted { $0.lastPathComponent < $1.lastPathComponent }
            .compactMap { url in
                guard let data = try? Data(contentsOf: url), let spec = try? SkinSpec.parse(data) else { return nil }
                return CustomSkin(id: url.deletingPathExtension().lastPathComponent, spec: spec)
            }
    }

    static func spec(_ id: String) -> SkinSpec? {
        guard let url = directory?.appendingPathComponent("\(id).meltycat"),
              let data = try? Data(contentsOf: url) else { return nil }
        return try? SkinSpec.parse(data)
    }

    // Validates first, so only files that load are kept. Returns the new id.
    static func save(_ data: Data) throws -> CustomSkin {
        let spec = try SkinSpec.parse(data)
        guard let directory else { throw SkinError("no Application Support folder") }
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let stamp = Int(Date().timeIntervalSince1970 * 1000)
        let id = "skin-\(stamp)"
        try data.write(to: directory.appendingPathComponent("\(id).meltycat"), options: .atomic)
        return CustomSkin(id: id, spec: spec)
    }

    static func delete(_ id: String) {
        guard let url = directory?.appendingPathComponent("\(id).meltycat") else { return }
        try? FileManager.default.removeItem(at: url)
    }
}
