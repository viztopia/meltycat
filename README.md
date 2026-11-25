# meltycat

[简体中文](README.zh-CN.md)

meltycat is a desktop cat that melts onto the upper-right corner of your active window. It watches your pointer, dozes off when you go quiet, and comes in eight skins.

| Platform | Status |
| --- | --- |
| Windows 10 / 11 (x64) | Available |
| macOS 11+ (Apple Silicon and Intel) | In development |

## Preview

<!-- Add screenshots and GIFs to the assets/ folder and reference them here, for example:
![Idle](assets/idle.gif)
![Hunt](assets/hunt.gif)
-->

Previews are coming soon.

## Features

- Shake the pointer and the cat goes hunting. Keep shaking to see how seriously it takes the job.
- Slowly stroke the cat's head, body, or paws with the pointer to pet it.
- The skin menu groups Ragdoll, Maine Coon, and Siberian under longhair, followed by the shorthair cats Orange Tabby, Black Cat, Li Hua, Tuxedo Cat, and Uni.
- 15 interface languages. Your skin and language choices are remembered.

## Download and install

Grab the latest build from the [Releases page](../../releases/latest). Each release includes a `SHA256SUMS.txt` file for verifying downloads.

### Windows

1. Download `meltycat-windows-x64.zip` and extract it to a folder you can write to. Preferences are saved next to the executable, so avoid `Program Files`.
2. Double-click `meltycat.exe`. No installer or extra runtime is needed.
3. The cat appears on your active window, and a cat icon appears in the system tray. If you do not see it, check the hidden icons under the `^` arrow.
4. If Windows SmartScreen shows "Windows protected your PC", click **More info**, then **Run anyway**. The executable is not code-signed.

To uninstall, choose **Exit** from the tray menu and delete the folder.

### macOS

macOS support is in development and the steps below may change.

1. Download `meltycat-macos-universal.zip`, unzip it, and drag `meltycat.app` into **Applications**.
2. On first launch macOS may say the app cannot be verified, because it is not notarized. On macOS 15 or later, open **System Settings > Privacy & Security**, scroll to the Security section, and click **Open Anyway**. On earlier versions, right-click the app and choose **Open**.
3. The cat appears on your active window, and its menu lives in the menu bar. The app does not show a Dock icon.

To uninstall, quit from the menu bar and move `meltycat.app` to the Trash.

## Usage

- **Hunt:** shake the pointer back and forth quickly.
- **Pet:** move the pointer slowly over the cat's head, body, or paws.
- **Menu:** right-click the tray icon (Windows) or open the menu bar icon (macOS) to change skins, pick a language, or exit.

## Build from source

**Windows (x64):** install MSVC C++ Build Tools and a Windows SDK, run `build.cmd`, then open `build\meltycat.exe`.

**macOS:** install Xcode or the Xcode Command Line Tools, run `bash macos/build.sh`, then open `macos/build/meltycat.app`.

## License

Free for personal, educational, and other noncommercial use. You are welcome to modify and share it, but commercial use is not allowed. Licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE), which has the full terms.

## Acknowledgments

Inspired by [KuroCat](https://github.com/chanke17/KuroCat) and [kurocat](https://github.com/qianyiai/kurocat).
