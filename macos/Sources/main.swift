import AppKit

struct DisplayedFrame {
    var windowID: CGWindowID
    var placement: Placement
    var pose: Pose
    var skin: String
    var resting: Bool
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    let overlay = OverlayWindow()
    let animation = Animation()
    var displayed: DisplayedFrame?
    var statusItem: NSStatusItem?
    var timer: Timer?
    var link: AnyObject?
    var linkDisplay: NSNumber?
    var interval: TimeInterval = 0
    var lastFrameTime: TimeInterval = 0
    let windowQueue = DispatchQueue(label: "io.github.viztopia.meltycat.windows", qos: .userInteractive)
    var frontWindow: FrontWindow?
    var presence = PerchPresence()
    var poofs: [PoofWindow] = []
    var appearStart: TimeInterval?
    var appearCloud = false
    var poofGate = PoofGate()
    var availableUpdate: String?
    var lookingUp = false
    var screensAsleep = false
    var appearanceObservation: NSKeyValueObservation?
    var skin = Settings.skin
    var spec = BuiltinSkins.spec(.black)
    lazy var skinWindow: SkinWindowController = {
        let controller = SkinWindowController()
        controller.onImported = { [weak self] imported in self?.choose(.custom(imported.id)) }
        controller.onSelected = { [weak self] choice in self?.choose(choice) }
        controller.onDeleted = { [weak self] id in
            guard let self else { return }
            if skin == .custom(id) { choose(.builtin(.black)) } else { rebuildMenu() }
        }
        return controller
    }()

    func applicationDidFinishLaunching(_ notification: Notification) {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        loadSkin()
        rebuildMenu()
        // The menu bar turns dark or light with the wallpaper and system appearance.
        appearanceObservation = statusItem?.button?.observe(\.effectiveAppearance) { [weak self] _, _ in
            DispatchQueue.main.async { self?.rebuildMenu() }
        }

        autoCheckForUpdates()
        // The app runs for days, so the daily check needs a clock of its own.
        Timer.scheduledTimer(withTimeInterval: 60 * 60, repeats: true) { [weak self] _ in self?.autoCheckForUpdates() }

        let workspace = NSWorkspace.shared.notificationCenter
        // Switching apps changes the perch, so look right away instead of at the next slow poll.
        workspace.addObserver(self, selector: #selector(appActivated), name: NSWorkspace.didActivateApplicationNotification,
                              object: nil)
        for name in [NSWorkspace.screensDidSleepNotification, NSWorkspace.willSleepNotification] {
            workspace.addObserver(self, selector: #selector(pause), name: name, object: nil)
        }
        for name in [NSWorkspace.screensDidWakeNotification, NSWorkspace.didWakeNotification] {
            workspace.addObserver(self, selector: #selector(resume), name: name, object: nil)
        }
        schedule(1.0 / 60)
    }

    // Double-clicked or dragged-onto-the-app .meltycat files.
    func application(_ application: NSApplication, open urls: [URL]) {
        openSkinWindow()
        for url in urls {
            if let data = try? Data(contentsOf: url) { skinWindow.importData(data) }
        }
    }

    // While perched, frames follow the display's refresh: a free-running timer drifts against it and now and then
    // shows one frame twice, which reads as a stutter in the swaying tail.
    func schedule(_ interval: TimeInterval) {
        let screen = interval < 0.25 ? overlay.screen ?? NSScreen.main : nil
        let display = screen?.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber
        if interval == self.interval, display == linkDisplay, timer != nil || link != nil { return }
        stopFrames()
        self.interval = interval
        if #available(macOS 14, *), let screen {
            let next = screen.displayLink(target: self, selector: #selector(displayFrame(_:)))
            let rate = Float(1 / interval)
            next.preferredFrameRateRange = CAFrameRateRange(minimum: rate, maximum: rate, preferred: rate)
            // Common modes keep the cat moving while menus are open or windows are dragged.
            next.add(to: .main, forMode: .common)
            link = next
            linkDisplay = display
            return
        }
        let next = Timer(timeInterval: interval, repeats: true) { [weak self] _ in self?.frame() }
        next.tolerance = interval * 0.1
        RunLoop.main.add(next, forMode: .common)
        timer = next
    }

    func stopFrames() {
        timer?.invalidate()
        timer = nil
        if #available(macOS 14, *) { (link as? CADisplayLink)?.invalidate() }
        link = nil
        linkDisplay = nil
    }

    @available(macOS 14, *)
    @objc func displayFrame(_ link: CADisplayLink) {
        frame(at: link.targetTimestamp)
    }

    @objc func appActivated() {
        if !screensAsleep { frame() }
    }

    @objc func pause() {
        screensAsleep = true
        stopFrames()
        poofs.forEach { $0.orderOut(nil) }
        poofs = []
    }

    @objc func resume() {
        screensAsleep = false
        schedule(1.0 / 60)
    }

    // A custom skin whose file has gone falls back to the default cat.
    func loadSkin() {
        skin = Settings.skin
        switch skin {
        case .builtin(let builtin):
            spec = BuiltinSkins.spec(builtin)
        case .custom(let id):
            if let custom = CustomSkins.spec(id) {
                spec = custom
            } else {
                skin = .builtin(.black)
                spec = BuiltinSkins.spec(.black)
            }
        }
    }

    func choose(_ choice: SkinChoice) {
        Settings.skin = choice
        loadSkin()
        rebuildMenu()
        skinWindow.update(current: skin, spec: spec)
    }

    func rebuildMenu() {
        let spec = spec
        let language = Settings.language
        statusItem?.button?.image = NSImage(size: NSSize(width: 18, height: 18), flipped: true) { _ in
            guard let g = NSGraphicsContext.current?.cgContext else { return false }
            let dark = NSAppearance.currentDrawing().bestMatch(from: [.darkAqua, .aqua]) == .darkAqua
            drawMenuIcon(in: g, size: 18, spec: spec, darkBar: dark)
            return true
        }
        let menu = NSMenu()
        let addSkin = { (title: String, choice: SkinChoice) in
            let item = menu.addItem(withTitle: title, action: #selector(self.chooseSkin(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = choice.setting
            item.state = choice == self.skin ? .on : .off
        }
        for (choice, name) in zip(Skin.menuOrder, language.skinNames) {
            if choice == .orangeTabby { menu.addItem(.separator()) }
            addSkin(name, .builtin(choice))
        }
        let customs = CustomSkins.all()
        if !customs.isEmpty {
            menu.addItem(.separator())
            for custom in customs { addSkin(custom.spec.name, .custom(custom.id)) }
        }
        menu.addItem(.separator())
        let changeSkin = menu.addItem(withTitle: language.skinText.changeSkin, action: #selector(openSkinWindow), keyEquivalent: "")
        changeSkin.target = self
        menu.addItem(.separator())
        let languages = NSMenu()
        for choice in Language.all {
            let item = languages.addItem(withTitle: choice.name, action: #selector(chooseLanguage(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = choice.setting
            item.state = choice == language ? .on : .off
        }
        menu.addItem(withTitle: language.menuLabel, action: nil, keyEquivalent: "").submenu = languages
        let poofMenu = NSMenu()
        for (mode, title) in zip(PoofMode.allCases, language.poofText.modes) {
            let item = poofMenu.addItem(withTitle: title, action: #selector(choosePoof(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = mode.setting
            item.state = mode == Settings.poof ? .on : .off
        }
        menu.addItem(withTitle: language.poofText.title, action: nil, keyEquivalent: "").submenu = poofMenu
        menu.addItem(.separator())
        // One item: it names a newer release once one is known, and the automatic toggle lives in its alert.
        let updates = language.updateText
        let updateTitle = availableUpdate.map { updates.available(versionName($0)) } ?? updates.checkNow
        menu.addItem(withTitle: updateTitle, action: #selector(checkForUpdatesNow), keyEquivalent: "").target = self
        menu.addItem(.separator())
        menu.addItem(withTitle: language.quit, action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        statusItem?.menu = menu
    }

    @objc func openSkinWindow() {
        skinWindow.show(current: skin, spec: spec)
    }

    @objc func chooseLanguage(_ sender: NSMenuItem) {
        guard let setting = sender.representedObject as? String, let language = Language(setting: setting) else { return }
        Settings.language = language
        rebuildMenu()
    }

    func versionName(_ tag: String) -> String { String(tag.drop(while: { $0 == "v" })) }

    // A failed check is retried at the next hourly tick; only an answer starts the next day's wait.
    func autoCheckForUpdates() {
        guard Updates.autoCheckDue(now: Date(), last: Settings.lastUpdateCheck, enabled: Settings.autoCheckUpdates,
                                   current: Updates.current) else { return }
        Updates.fetchLatestTag { [weak self] tag in
            guard let self, let tag else { return }
            Settings.lastUpdateCheck = Date()
            self.availableUpdate = Updates.isNewer(tag, than: Updates.current) ? tag : nil
            self.rebuildMenu()
        }
    }

    @objc func checkForUpdatesNow() {
        Updates.fetchLatestTag { [weak self] tag in
            guard let self else { return }
            let text = Settings.language.updateText
            let alert = NSAlert()
            alert.messageText = "meltycat"
            if let tag {
                Settings.lastUpdateCheck = Date()
                let newer = Updates.isNewer(tag, than: Updates.current)
                self.availableUpdate = newer ? tag : nil
                self.rebuildMenu()
                if newer {
                    alert.informativeText = text.newer(self.versionName(tag), Updates.current)
                    alert.addButton(withTitle: text.download)
                    alert.addButton(withTitle: text.later)
                } else {
                    alert.informativeText = text.upToDate(Updates.current)
                    alert.addButton(withTitle: text.ok)
                }
            } else {
                alert.informativeText = text.failed
                alert.addButton(withTitle: text.ok)
            }
            alert.showsSuppressionButton = true
            alert.suppressionButton?.title = text.automatically
            alert.suppressionButton?.state = Settings.autoCheckUpdates ? .on : .off
            // A menu bar app has no window of its own, so bring the alert forward.
            NSApp.activate(ignoringOtherApps: true)
            let response = alert.runModal()
            Settings.autoCheckUpdates = alert.suppressionButton?.state == .on
            if response == .alertFirstButtonReturn, self.availableUpdate != nil, tag != nil {
                self.openDownloadPage()
            }
        }
    }

    func openDownloadPage() {
        NSWorkspace.shared.open(Updates.downloadPage)
    }

    @objc func choosePoof(_ sender: NSMenuItem) {
        guard let setting = sender.representedObject as? String, let mode = PoofMode(setting: setting) else { return }
        Settings.poof = mode
        rebuildMenu()
    }

    @objc func chooseSkin(_ sender: NSMenuItem) {
        guard let setting = sender.representedObject as? String, let choice = SkinChoice(setting: setting) else { return }
        choose(choice)
    }

    // Frames draw from the latest answer instead of waiting for the window server, so the perch can trail a
    // moving window by one frame but the cat never stalls.
    func lookUpFrontWindow() {
        guard let app = NSWorkspace.shared.frontmostApplication,
              app.processIdentifier != ProcessInfo.processInfo.processIdentifier else {
            frontWindow = nil
            return
        }
        if lookingUp { return }
        lookingUp = true
        let pid = app.processIdentifier
        windowQueue.async { [weak self] in
            let window = queryFrontWindow(of: pid)
            DispatchQueue.main.async {
                guard let self else { return }
                self.lookingUp = false
                // An answer for an app that has since lost focus is stale.
                guard NSWorkspace.shared.frontmostApplication?.processIdentifier == pid else { return }
                let moved = window?.id != self.frontWindow?.id
                self.frontWindow = window
                if moved && !self.screensAsleep { self.frame() }
            }
        }
    }

    var reduceMotion: Bool { NSWorkspace.shared.accessibilityDisplayShouldReduceMotion }

    // The leaving cat is drawn in the puff's own window, so the overlay can already be elsewhere.
    func puffOut(_ displayed: DisplayedFrame, now: TimeInterval, cloud: Bool) {
        let placement = displayed.placement
        let frame = flipToTopLeft(CGRect(origin: placement.origin, size: placement.size), primaryHeight: primaryScreenHeight)
        let poof = PoofWindow(around: frame, placement: placement, start: now, leaving: (displayed.pose, spec),
                              reduceMotion: !cloud)
        poof.orderFrontRegardless()
        poof.display()
        poofs.append(poof)
    }

    func puffIn(around frame: NSRect, placement: Placement, now: TimeInterval) {
        let poof = PoofWindow(around: frame, placement: placement, start: now, leaving: nil, reduceMotion: false)
        poof.orderFrontRegardless()
        poofs.append(poof)
    }

    // Display-linked frames are timed for when they reach the screen, so motion advances evenly.
    func frame(at time: TimeInterval = CACurrentMediaTime()) {
        lookUpFrontWindow()
        let observed = frontWindow.flatMap(findPerch(on:))
        let now = max(time, lastFrameTime)
        lastFrameTime = now
        let holding = CGEventSource.buttonState(.combinedSessionState, button: .left)
        let event = presence.update(now: now, observed: observed?.windowID, holding: holding)
        let puff = event.map { poofGate.cloud(for: $0, at: now, mode: Settings.poof) } == true
        let style = poofStyle(puff: puff, reduceMotion: reduceMotion)
        if event == .disappear, style != .none, let displayed { puffOut(displayed, now: now, cloud: style == .cloud) }
        let appearing = event == .appear
        if appearing {
            appearCloud = style == .cloud
            appearStart = style == .none ? nil : now
            if style == .none { overlay.alphaValue = 1 }
        }
        poofs = poofs.filter { $0.advance(to: now) }

        // Until a change has settled the cat stays where it was last drawn.
        var perch: Perch?
        if let shown = presence.shown {
            if observed?.windowID == shown {
                perch = observed
            } else if let displayed, displayed.windowID == shown {
                perch = Perch(windowID: shown, placement: displayed.placement)
            }
        }
        let height = primaryScreenHeight
        let mouse = NSEvent.mouseLocation
        let cursor = CGPoint(x: mouse.x, y: height - mouse.y)
        let newPerch = displayed == nil || displayed?.windowID != perch?.windowID
        let samePlacement = perch != nil && displayed?.placement == perch?.placement
        var overCat = false
        if samePlacement, let perch, let displayed {
            let placement = perch.placement
            overCat = CatRenderer.hitsPettingArea(placement.localCursor(cursor), displayed.pose, spec: spec,
                                                  headroom: placement.topPadding / placement.scale)
        }
        // Mouse, buttons, wheel and keys alike; only the time is read, never the event.
        let idle = CGEventSource.secondsSinceLastEventType(.combinedSessionState, eventType: CGEventType(rawValue: ~0)!)
        animation.observe(now: now, cursor: cursor, inputTime: now - idle, scale: perch?.placement.scale ?? 1,
                          overCat: overCat)

        guard let perch else {
            if displayed != nil { overlay.orderOut(nil) }
            displayed = nil
            appearStart = nil
            schedule(poofs.isEmpty ? frameInterval(perched: false, pose: nil, secondsSinceInput: idle) : 1.0 / 60)
            return
        }
        let placement = perch.placement
        let pose = animation.sample(now: now, cursor: placement.localCursor(cursor),
                                    tailDirection: placement.tailDirection, newPerch: newPerch)
        let resting = pose.alert == 0 && pose.tailMotion == 0 && pose.dreamCount == 0 && pose.earTwitch == 0
            && pose.squint == 0 && pose.petSymbolCount == 0
        // True through the frame that lands the cat in its exact pose, which must be drawn.
        let arriving = appearStart != nil
        if let start = appearStart {
            let progress = max(0, CGFloat((now - start) / poofDuration))
            let landed = progress >= 1
            if landed { appearStart = nil }
            overlay.alphaValue = landed ? 1 : appearCloud ? poofCatAlpha(appearing: true, progress: progress) : progress
        }
        let effects = !poofs.isEmpty || appearStart != nil
        schedule(effects ? 1.0 / 60 : frameInterval(perched: true, pose: pose, secondsSinceInput: idle))
        let skinID = skin.setting
        if resting && samePlacement && displayed?.resting == true && displayed?.skin == skinID && !arriving { return }

        let frame = flipToTopLeft(CGRect(origin: placement.origin, size: placement.size), primaryHeight: height)
        if overlay.frame != frame { overlay.setFrame(frame, display: false) }
        overlay.catView.pose = pose
        overlay.catView.spec = spec
        overlay.catView.placement = placement
        overlay.catView.needsDisplay = true
        if newPerch { overlay.orderFrontRegardless() }
        // Ordered after the overlay so the cloud is in front of the cat it hides.
        if appearing && appearCloud { puffIn(around: frame, placement: placement, now: now) }
        displayed = DisplayedFrame(windowID: perch.windowID, placement: placement, pose: pose, skin: skinID,
                                   resting: resting)
    }
}

if let id = Bundle.main.bundleIdentifier, NSRunningApplication.runningApplications(withBundleIdentifier: id).count > 1 {
    exit(0)
}
let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.run()
