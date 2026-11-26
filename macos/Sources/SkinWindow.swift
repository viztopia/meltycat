import AppKit
import UniformTypeIdentifiers

// The cat on a window ledge, as it will appear, for checking a skin before and after import.
final class SkinPreview: NSView {
    var spec: SkinSpec? { didSet { needsDisplay = true } }

    override var isFlipped: Bool { true }
    override var intrinsicContentSize: NSSize { NSSize(width: 260, height: 226) }

    override func draw(_ dirtyRect: NSRect) {
        guard let spec, let g = NSGraphicsContext.current?.cgContext else { return }
        // Views no longer clip to their bounds by default on recent macOS.
        g.clip(to: bounds)
        // The ledge starts a little left of the head and the longhair tail reaches x = 200: centre that span.
        let left: CGFloat = -6, right: CGFloat = 200
        g.translateBy(x: (bounds.width - left - right) / 2, y: 6)
        let ledge = CGRect(x: left, y: CGFloat(CatGeometry.topPadding + CatGeometry.cornerY),
                           width: CGFloat(CatGeometry.cornerX) - left, height: bounds.height)
        g.addPath(CGPath(roundedRect: ledge, cornerWidth: 8, cornerHeight: 8, transform: nil))
        g.setFillColor(NSColor.windowBackgroundColor.blended(withFraction: 0.5, of: .gray)?.cgColor ?? .white)
        g.fillPath()
        g.translateBy(x: 0, y: CGFloat(CatGeometry.topPadding))
        CatRenderer.draw(in: g, pose: .awake, spec: spec, headroom: CGFloat(CatGeometry.topPadding))
    }
}

// Accepts dropped files or text anywhere in the window, and ⌘V.
final class DropView: NSView {
    var onData: ((Data) -> Void)?
    var onPaste: (() -> Void)?

    override init(frame: NSRect) {
        super.init(frame: frame)
        registerForDraggedTypes([.fileURL, .string])
    }

    required init?(coder: NSCoder) { nil }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation { .copy }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        guard let data = DropView.data(from: sender.draggingPasteboard) else { return false }
        onData?(data)
        return true
    }

    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        if event.modifierFlags.intersection(.deviceIndependentFlagsMask) == .command,
           event.charactersIgnoringModifiers == "v" {
            onPaste?()
            return true
        }
        return super.performKeyEquivalent(with: event)
    }

    static func data(from pasteboard: NSPasteboard) -> Data? {
        if let url = (pasteboard.readObjects(forClasses: [NSURL.self]) as? [URL])?.first(where: \.isFileURL) {
            return try? Data(contentsOf: url)
        }
        return pasteboard.string(forType: .string).map { Data($0.utf8) }
    }
}

final class SkinWindowController: NSObject {
    var onImported: ((CustomSkin) -> Void)?
    var onDeleted: ((String) -> Void)?
    var onSelected: ((SkinChoice) -> Void)?

    private let window: NSWindow
    private let preview = SkinPreview()
    private let hint = NSTextField(wrappingLabelWithString: "")
    private let status = NSTextField(wrappingLabelWithString: "")
    private let chooseButton = NSButton(title: "", target: nil, action: nil)
    private let pasteButton = NSButton(title: "", target: nil, action: nil)
    private let guideButton = NSButton(title: "", target: nil, action: nil)
    private let customs = NSPopUpButton()
    private let deleteButton = NSButton(title: "", target: nil, action: nil)
    private lazy var customRow = NSStackView(views: [customs, deleteButton])
    private var text = Settings.language.skinText

    override init() {
        let content = DropView(frame: NSRect(x: 0, y: 0, width: 420, height: 430))
        window = NSWindow(contentRect: content.frame, styleMask: [.titled, .closable], backing: .buffered, defer: false)
        window.isReleasedWhenClosed = false
        window.contentView = content
        super.init()

        hint.alignment = .center
        hint.textColor = .secondaryLabelColor
        status.alignment = .center
        status.isSelectable = true
        for (button, action) in [(chooseButton, #selector(chooseFile)), (pasteButton, #selector(paste)),
                                 (guideButton, #selector(copyGuide)), (deleteButton, #selector(deleteCustom))] {
            button.bezelStyle = .rounded
            button.target = self
            button.action = action
        }
        customs.target = self
        customs.action = #selector(selectCustom)

        let buttons = NSStackView(views: [chooseButton, pasteButton, guideButton])
        let stack = NSStackView(views: [preview, hint, buttons, status, customRow])
        stack.orientation = .vertical
        stack.alignment = .centerX
        stack.spacing = 12
        stack.edgeInsets = NSEdgeInsets(top: 16, left: 20, bottom: 20, right: 20)
        stack.translatesAutoresizingMaskIntoConstraints = false
        content.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: content.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: content.trailingAnchor),
            stack.topAnchor.constraint(equalTo: content.topAnchor),
            stack.bottomAnchor.constraint(lessThanOrEqualTo: content.bottomAnchor),
            hint.widthAnchor.constraint(equalToConstant: 360),
            status.widthAnchor.constraint(equalToConstant: 360),
        ])
        content.onData = { [weak self] in self?.importData($0) }
        content.onPaste = { [weak self] in self?.paste() }
    }

    func show(current: SkinChoice, spec: SkinSpec) {
        text = Settings.language.skinText
        window.title = text.changeSkin.trimmingCharacters(in: CharacterSet(charactersIn: "…."))
        hint.stringValue = text.dropHint
        chooseButton.title = text.chooseFile
        pasteButton.title = text.paste
        guideButton.title = text.copyGuide
        deleteButton.title = text.delete
        update(current: current, spec: spec)
        if !window.isVisible {
            status.stringValue = ""
            window.center()
        }
        NSApp.activate(ignoringOtherApps: true)
        window.makeKeyAndOrderFront(nil)
    }

    func update(current: SkinChoice, spec: SkinSpec) {
        preview.spec = spec
        let all = CustomSkins.all()
        customs.removeAllItems()
        for skin in all {
            customs.addItem(withTitle: skin.spec.name)
            customs.lastItem?.representedObject = skin.id
        }
        if case .custom(let id) = current, let index = all.firstIndex(where: { $0.id == id }) {
            customs.selectItem(at: index)
        }
        customRow.isHidden = all.isEmpty
    }

    func importData(_ data: Data) {
        do {
            let skin = try CustomSkins.save(data)
            status.textColor = .labelColor
            status.stringValue = text.imported + Settings.language.colon + skin.spec.name
            onImported?(skin)
        } catch {
            status.textColor = .systemRed
            status.stringValue = text.importFailed + Settings.language.colon + "\(error)"
        }
    }

    @objc private func chooseFile() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url, let data = try? Data(contentsOf: url) else { return }
            self?.importData(data)
        }
    }

    @objc private func paste() {
        guard let data = DropView.data(from: .general) else { return }
        importData(data)
    }

    // The guide plus two complete skins for the AI to start from.
    @objc private func copyGuide() {
        guard let resources = Bundle.main.resourceURL,
              let guide = try? String(contentsOf: resources.appendingPathComponent("AI-GUIDE.md"), encoding: .utf8) else { return }
        var text = guide
        for (title, file) in [("shorthair template (li-hua)", "li-hua"), ("longhair template (ragdoll)", "ragdoll")] {
            if let skin = try? String(contentsOf: resources.appendingPathComponent("skins/\(file).meltycat"), encoding: .utf8) {
                text += "\n\n## Appendix: \(title)\n\n```json\n\(skin)```\n"
            }
        }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        status.textColor = .labelColor
        status.stringValue = self.text.guideCopied
    }

    @objc private func selectCustom() {
        guard let id = customs.selectedItem?.representedObject as? String else { return }
        onSelected?(.custom(id))
    }

    @objc private func deleteCustom() {
        guard let id = customs.selectedItem?.representedObject as? String else { return }
        CustomSkins.delete(id)
        status.stringValue = ""
        onDeleted?(id)
    }
}
