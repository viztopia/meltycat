#include "animation.h"
#include "tray_menu.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <iterator>
#include <optional>

namespace {

constexpr wchar_t windowClass[] = L"MeltyCat.Overlay";
constexpr UINT trayMessage = WM_APP + 1;
constexpr UINT_PTR frameTimer = 1;
constexpr UINT_PTR trayTimer = 2;

class Surface {
public:
    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    ~Surface() { Reset(); }

    bool Resize(int width, int height) {
        if (bitmap_ && size_.cx == width && size_.cy == height) return true;
        Reset();

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        dc_ = CreateCompatibleDC(nullptr);
        if (!dc_) return false;
        void* pixels = nullptr;
        bitmap_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap_) return false;
        previous_ = SelectObject(dc_, bitmap_);
        if (!previous_) {
            Reset();
            return false;
        }
        pixels_ = static_cast<BYTE*>(pixels);
        size_ = {width, height};
        return true;
    }

    HDC Dc() const { return dc_; }
    HBITMAP Handle() const { return bitmap_; }
    BYTE* Pixels() const { return pixels_; }
    SIZE Size() const { return size_; }

private:
    void Reset() {
        if (previous_) SelectObject(dc_, previous_);
        if (bitmap_) DeleteObject(bitmap_);
        if (dc_) DeleteDC(dc_);
        dc_ = nullptr;
        bitmap_ = nullptr;
        previous_ = nullptr;
        pixels_ = nullptr;
        size_ = {};
    }

    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    BYTE* pixels_ = nullptr;
    SIZE size_{};
};

HICON MakeTrayIcon(cat::Skin skin) {
    Surface image;
    if (!image.Resize(32, 32)) return nullptr;
    {
        Gdiplus::Bitmap bitmap(32, 32, 32 * 4, PixelFormat32bppPARGB, image.Pixels());
        Gdiplus::Graphics graphics(&bitmap);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        cat::DrawTrayIcon(graphics, skin);
    }

    std::array<BYTE, 32 * 4> maskBits{};
    const HBITMAP mask = CreateBitmap(32, 32, 1, 1, maskBits.data());
    if (!mask) return nullptr;
    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmMask = mask;
    info.hbmColor = image.Handle();
    const HICON icon = CreateIconIndirect(&info);
    DeleteObject(mask);
    return icon;
}

bool IsShellSurface(HWND window) {
    if (window == GetDesktopWindow() || window == GetShellWindow()) return true;

    wchar_t name[128]{};
    GetClassNameW(window, name, static_cast<int>(std::size(name)));
    for (const wchar_t* shellClass : {L"Progman", L"WorkerW", L"Shell_TrayWnd",
                                     L"Shell_SecondaryTrayWnd", L"MultitaskingViewFrame",
                                     L"LockScreenBackstopFrame",
                                     L"Windows.UI.Core.CoreWindow", L"NotifyIconOverflowWindow",
                                     L"TopLevelWindowForOverflowXamlIsland"}) {
        if (std::wcscmp(name, shellClass) == 0) return true;
    }
    return false;
}

struct Placement {
    HWND target = nullptr;
    POINT origin{};
    float scale = 1.0f;
    float tailDirection = 1.0f;
    LONG topPadding = 0;

    bool operator==(const Placement& other) const {
        return target == other.target && origin.x == other.origin.x && origin.y == other.origin.y &&
               scale == other.scale && tailDirection == other.tailDirection && topPadding == other.topPadding;
    }

    Gdiplus::PointF LocalCursor(POINT cursor) const {
        return {(cursor.x - origin.x) / scale, (cursor.y - origin.y - topPadding) / scale};
    }
};

bool FindPlacement(HWND foreground, HWND overlay, Placement& placement) {
    const HWND target = GetAncestor(foreground, GA_ROOT);
    if (!target || target == overlay || !IsWindowVisible(target) || IsIconic(target) ||
        IsZoomed(target) || IsShellSurface(target)) {
        return false;
    }

    DWORD cloaked = 0;
    DwmGetWindowAttribute(target, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return false;

    RECT frame{};
    if (FAILED(DwmGetWindowAttribute(target, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame))) &&
        !GetWindowRect(target, &frame)) {
        return false;
    }
    if (IsRectEmpty(&frame)) return false;

    const POINT corner{frame.right - 1, frame.top};
    HMONITOR monitor = MonitorFromPoint(corner, MONITOR_DEFAULTTONULL);
    if (!monitor) monitor = MonitorFromRect(&frame, MONITOR_DEFAULTTONULL);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) return false;

    const RECT& work = info.rcWork;
    const RECT& screen = info.rcMonitor;
    if (frame.left <= screen.left && frame.top <= screen.top &&
        frame.right >= screen.right && frame.bottom >= screen.bottom) {
        return false;
    }

    DEVICE_SCALE_FACTOR factor = SCALE_100_PERCENT;
    GetScaleFactorForMonitor(monitor, &factor);
    const float scale = std::min({factor / 100.0f,
                                  (work.right - work.left) / static_cast<float>(cat::width),
                                  (work.bottom - work.top) / static_cast<float>(cat::height)});
    if (scale <= 0) return false;

    const auto pixels = [scale](int value) { return std::lround(value * scale); };
    const LONG desiredX = frame.right - pixels(cat::cornerX);
    const bool tailOutside = desiredX + pixels(cat::width) <= work.right;
    const LONG rightLimit = work.right - pixels(tailOutside ? cat::width : cat::foldedRight);
    if (frame.right <= work.left || frame.left >= work.right ||
        frame.bottom <= work.top || frame.top >= work.bottom) {
        return false;
    }
    // Keep the head visible near screen edges and fold the tail inward if it has no room.
    const LONG normalTop = std::clamp(frame.top - pixels(cat::cornerY), work.top,
                                     work.bottom - pixels(cat::height));
    const LONG topPadding = std::min(pixels(cat::topPadding), normalTop - work.top);
    placement = {target,
                 {std::clamp(desiredX, work.left, rightLimit), normalTop - topPadding},
                 scale, tailOutside ? 1.0f : -1.0f, topPadding};
    return true;
}

struct DisplayedFrame {
    Placement placement;
    cat::Pose pose;
    cat::Skin skin;
    bool resting;
};

struct App {
    HWND overlay = nullptr;
    HWND menuOwner = nullptr;
    UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    bool menuOpen = false;
    std::optional<DisplayedFrame> displayed;
    POINT cursor{};
    Surface surface;
    cat::Animation animation;
    NOTIFYICONDATAW tray{};
    TrayMenu trayMenu;

    ~App() {
        if (menuOwner) DestroyWindow(menuOwner);
        if (tray.hIcon) DestroyIcon(tray.hIcon);
    }

    void RegisterTrayIcon() {
        if (Shell_NotifyIconW(NIM_ADD, &tray)) {
            KillTimer(overlay, trayTimer);
        } else {
            // Explorer can be temporarily unavailable during sign-in or a restart.
            SetTimer(overlay, trayTimer, 2000, nullptr);
        }
    }

    void Frame(HWND foreground) {
        if (menuOpen) return;
        Placement placement{};
        const bool hasPerch = FindPlacement(foreground, overlay, placement);
        const auto now = cat::Animation::Clock::now();
        POINT position{};
        const bool hasCursor = GetCursorPos(&position) != FALSE;
        if (hasCursor) cursor = position;
        const bool newPerch = !displayed || displayed->placement.target != placement.target;
        const bool samePlacement = hasPerch && displayed && displayed->placement == placement;
        const float headroom = placement.topPadding / placement.scale;
        const auto localCursor = placement.LocalCursor(cursor);
        const bool overCat = hasCursor && samePlacement &&
                             cat::HitsPettingArea(localCursor, displayed->pose, displayed->skin, headroom);
        LASTINPUTINFO input{sizeof(input), 0};
        const auto inputTick = GetLastInputInfo(&input) ? std::optional<DWORD>(input.dwTime) : std::nullopt;
        animation.Observe(now, cursor, inputTick, placement.scale, overCat);

        if (!hasPerch) {
            if (displayed) ShowWindow(overlay, SW_HIDE);
            displayed.reset();
            return;
        }

        const auto pose = animation.Sample(now, localCursor, placement.tailDirection, newPerch);
        const bool resting = pose.alert == 0 && pose.tailMotion == 0 && pose.dreamCount == 0 &&
                             pose.earTwitch == 0 && pose.squint == 0 && pose.petSymbolCount == 0;
        const auto skin = trayMenu.Skin();
        if (resting && samePlacement && displayed->resting && displayed->skin == skin) return;

        const auto width = static_cast<int>(std::lround(cat::width * placement.scale));
        const auto height = static_cast<int>(std::lround(cat::height * placement.scale) + placement.topPadding);
        if (!surface.Resize(width, height)) return;
        {
            Gdiplus::Bitmap frame(width, height, width * 4, PixelFormat32bppPARGB, surface.Pixels());
            Gdiplus::Graphics graphics(&frame);
            graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
            graphics.TranslateTransform(0.0f, static_cast<float>(placement.topPadding));
            graphics.ScaleTransform(placement.scale, placement.scale);
            cat::Draw(graphics, pose, skin, headroom);
        }

        POINT source{};
        SIZE size = surface.Size();
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(overlay, nullptr, &placement.origin, &size,
                                 surface.Dc(), &source, 0, &blend, ULW_ALPHA)) {
            return;
        }
        if (newPerch) {
            SetWindowPos(overlay, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        displayed = DisplayedFrame{placement, pose, skin, resting};
    }

    void HandleTrayCommand(UINT command) {
        const auto action = trayMenu.Select(command);
        if (action == TrayMenu::Action::Exit) {
            DestroyWindow(overlay);
            return;
        }
        if (action != TrayMenu::Action::SkinChanged) return;
        const HICON icon = MakeTrayIcon(trayMenu.Skin());
        if (!icon) return;
        const HICON previousIcon = tray.hIcon;
        tray.hIcon = icon;
        if (!Shell_NotifyIconW(NIM_MODIFY, &tray)) RegisterTrayIcon();
        if (previousIcon) DestroyIcon(previousIcon);
    }

    void ShowTrayMenu() {
        if (menuOpen) return;
        const HMENU menu = trayMenu.Create();
        if (!menu) return;
        menuOpen = true;
        POINT menuPosition{};
        GetCursorPos(&menuPosition);
        const HWND previousForeground = GetForegroundWindow();
        ShowWindow(menuOwner, SW_SHOWNOACTIVATE);
        SetForegroundWindow(menuOwner);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                            menuPosition.x, menuPosition.y, 0, menuOwner, nullptr);
        DestroyMenu(menu);
        PostMessageW(menuOwner, WM_NULL, 0, 0);
        if (GetForegroundWindow() == menuOwner && previousForeground) {
            SetForegroundWindow(previousForeground);
        }
        ShowWindow(menuOwner, SW_HIDE);
        menuOpen = false;
        HandleTrayCommand(command);
    }
};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    if (message && message == app->taskbarCreated) {
        app->RegisterTrayIcon();
        return 0;
    }

    switch (message) {
    case WM_TIMER:
        if (wParam == frameTimer) app->Frame(GetForegroundWindow());
        if (wParam == trayTimer) app->RegisterTrayIcon();
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case trayMessage:
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) app->ShowTrayMenu();
        return 0;
    case WM_DESTROY:
        KillTimer(window, frameTimer);
        KillTimer(window, trayTimer);
        Shell_NotifyIconW(NIM_DELETE, &app->tray);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int Run(HINSTANCE instance) {
    App app;
    // Keep tray commands available even when the cat overlay itself is hidden.
    app.menuOwner = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"STATIC", L"", WS_POPUP,
                                    0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!app.menuOwner || !SetLayeredWindowAttributes(app.menuOwner, 0, 0, LWA_ALPHA)) return 1;

    const WNDCLASSW windowType{0, WindowProc, 0, 0, instance, nullptr, nullptr, nullptr,
                               nullptr, windowClass};
    if (!RegisterClassW(&windowType)) return 1;

    app.overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                                      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                  windowClass, L"meltycat", WS_POPUP,
                                  0, 0, 0, 0, nullptr, nullptr, instance, &app);
    if (!app.overlay) return 1;

    app.tray.cbSize = sizeof(app.tray);
    app.tray.hWnd = app.overlay;
    app.tray.uID = 1;
    app.tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    app.tray.uCallbackMessage = trayMessage;
    app.tray.hIcon = MakeTrayIcon(app.trayMenu.Skin());
    wcscpy_s(app.tray.szTip, L"meltycat");
    if (!app.tray.hIcon || !SetTimer(app.overlay, frameTimer, 16, nullptr)) {
        DestroyWindow(app.overlay);
        return 1;
    }

    app.RegisterTrayIcon();
    app.Frame(GetForegroundWindow());
    MSG message{};
    BOOL result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (result == -1) {
        DestroyWindow(app.overlay);
        return 1;
    }
    return static_cast<int>(message.wParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const HANDLE singleInstance = CreateMutexW(nullptr, FALSE, L"Local\\MeltyCat.Prototype");
    const DWORD mutexError = GetLastError();
    if (!singleInstance) return 1;
    if (mutexError == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleInstance);
        return 0;
    }

    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR gdiplus = 0;
    int result = 1;
    if (Gdiplus::GdiplusStartup(&gdiplus, &input, nullptr) == Gdiplus::Ok) {
        result = Run(instance);
        Gdiplus::GdiplusShutdown(gdiplus);
    }
    CloseHandle(singleInstance);
    if (result != 0) {
        MessageBoxW(nullptr, L"启动失败。请从 Windows 桌面重新运行 meltycat。",
                    L"meltycat", MB_OK | MB_ICONERROR);
    }
    return result;
}
