import Foundation

enum Settings {
    // Built-in values match the Windows build's meltycat-skin.txt; imported skins are "custom:<id>".
    static var skin: SkinChoice {
        get { UserDefaults.standard.string(forKey: "skin").flatMap(SkinChoice.init(setting:)) ?? .builtin(.black) }
        set { UserDefaults.standard.set(newValue.setting, forKey: "skin") }
    }

    static var poof: PoofMode {
        get { UserDefaults.standard.string(forKey: "poof").flatMap(PoofMode.init(setting:)) ?? .quiet }
        set { UserDefaults.standard.set(newValue.setting, forKey: "poof") }
    }

    static var autoCheckUpdates: Bool {
        get { UserDefaults.standard.object(forKey: "autoCheckUpdates") as? Bool ?? true }
        set { UserDefaults.standard.set(newValue, forKey: "autoCheckUpdates") }
    }

    static var lastUpdateCheck: Date? {
        get { UserDefaults.standard.object(forKey: "lastUpdateCheck") as? Date }
        set { UserDefaults.standard.set(newValue, forKey: "lastUpdateCheck") }
    }

    // Follows the system until a language is picked from the menu.
    static var language: Language {
        get { UserDefaults.standard.string(forKey: "language").flatMap(Language.init(setting:)) ?? Language.system() }
        set { UserDefaults.standard.set(newValue.setting, forKey: "language") }
    }
}
