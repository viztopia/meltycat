#include "animation.h"
#include "poof.h"
#include "skin_window.h"

#include <dwmapi.h>
#include <roapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <iterator>
#include <list>
#include <optional>
#include <string_view>

namespace {

constexpr wchar_t windowClass[] = L"MeltyCat.Overlay";
constexpr UINT trayMessage = WM_APP + 1;
constexpr UINT_PTR trayTimer = 2;
constexpr ULONG_PTR importSkinMessage = 1;
using Clock = cat::Animation::Clock;
constexpr auto frameInterval = std::chrono::nanoseconds(1000000000) / 60;
constexpr auto noPerchInterval = std::chrono::milliseconds(250);

Clock::duration PerchFrameInterval(const cat::Pose& pose, bool recentInput) {
    if (recentInput || pose.hunt > 0 || pose.brace > 0 || pose.chatter > 0 ||
        pose.squint > 0 || pose.petSymbolCount > 0 || pose.tailMotion > 0 ||
        pose.dreamCount > 0 || pose.earTwitch > 0) {
        return frameInterval;
    }
    return pose.alert > 0 ? frameInterval * 2 : frameInterval * 4;
}

class FrameTimer {
public:
    FrameTimer() : timer_(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                TIMER_MODIFY_STATE | SYNCHRONIZE)) {
        if (!timer_) timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    FrameTimer(const FrameTimer&) = delete;
    FrameTimer& operator=(const FrameTimer&) = delete;
    ~FrameTimer() { if (timer_) CloseHandle(timer_); }

    bool Schedule(Clock::time_point frameStart, Clock::duration interval) {
        if (!timer_) return false;
        const auto now = Clock::now();
        const auto next = deadline_ + interval;
        deadline_ = interval == interval_ && next > now ? next : std::max(frameStart + interval, now);
        interval_ = interval;

        using TimerTicks = std::chrono::duration<LONGLONG, std::ratio<1, 10000000>>;
        LARGE_INTEGER due{};
        due.QuadPart = -std::max(1LL, std::chrono::duration_cast<TimerTicks>(deadline_ - now).count());
        return SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE) != FALSE;
    }

    DWORD Wait() const {
        return MsgWaitForMultipleObjectsEx(1, &timer_, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }

private:
    HANDLE timer_;
    Clock::time_point deadline_{};
    Clock::duration interval_{};
};

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

HWND CreateOverlay(const wchar_t* title, void* context = nullptr) {
    const HWND window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                                            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                        windowClass, title, WS_POPUP, 0, 0, 0, 0,
                                        nullptr, nullptr, GetModuleHandleW(nullptr), context);
    if (window) {
        const BOOL disabled = TRUE;
        DwmSetWindowAttribute(window, DWMWA_TRANSITIONS_FORCEDISABLED, &disabled, sizeof(disabled));
    }
    return window;
}

bool Present(HWND window, const Surface& surface, POINT position, BYTE opacity, bool raise) {
    POINT source{};
    SIZE size = surface.Size();
    BLENDFUNCTION blend{AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
    if (!UpdateLayeredWindow(window, nullptr, &position, &size, surface.Dc(), &source, 0, &blend, ULW_ALPHA)) {
        return false;
    }
    if (raise) {
        SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    return true;
}

HICON MakeTrayIcon(const cat::SkinSpec& skin) {
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

bool IsTrayOverflowClass(std::wstring_view name) {
    return name == L"NotifyIconOverflowWindow" || name == L"TopLevelWindowForOverflowXamlIsland";
}

bool IsTrayClass(std::wstring_view name) {
    return name == L"Shell_TrayWnd" || name == L"Shell_SecondaryTrayWnd" || IsTrayOverflowClass(name);
}

bool IsShellSurface(HWND window) {
    if (window == GetDesktopWindow() || window == GetShellWindow()) return true;

    wchar_t name[128]{};
    GetClassNameW(window, name, static_cast<int>(std::size(name)));
    if (IsTrayClass(name)) return true;
    for (const wchar_t* shellClass : {L"Progman", L"WorkerW", L"MultitaskingViewFrame",
                                     L"LockScreenBackstopFrame", L"Windows.UI.Core.CoreWindow"}) {
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
    const LONG normalTop = frame.top - pixels(cat::cornerY);
    if (frame.right <= work.left || frame.left >= work.right ||
        frame.bottom <= work.top || frame.top >= work.bottom) {
        return false;
    }
    // A perch moved away from the corner would leave the cat floating over the window.
    constexpr LONG slack = 1;
    if (frame.right > work.right + slack || desiredX < work.left - slack ||
        normalTop < work.top - slack || normalTop + pixels(cat::height) > work.bottom + slack) {
        return false;
    }
    const bool tailOutside = desiredX + pixels(cat::width) <= work.right;
    const LONG rightLimit = work.right - pixels(tailOutside ? cat::width : cat::foldedRight);
    const LONG topPadding = std::clamp(normalTop - work.top, 0L, pixels(cat::topPadding));
    placement = {target,
                 {std::clamp(desiredX, work.left, rightLimit), normalTop - topPadding},
                 scale, tailOutside ? 1.0f : -1.0f, topPadding};
    return true;
}

struct DisplayedFrame {
    Placement placement;
    cat::Pose pose;
    Skin skin;
    bool resting;
    BYTE opacity;
};

bool RenderCat(Surface& surface, const DisplayedFrame& frame) {
    const auto& placement = frame.placement;
    const LONG width = std::lround(cat::width * placement.scale);
    const LONG height = std::lround(cat::height * placement.scale) + placement.topPadding;
    if (!surface.Resize(width, height)) return false;
    Gdiplus::Bitmap bitmap(width, height, width * 4, PixelFormat32bppPARGB, surface.Pixels());
    Gdiplus::Graphics g(&bitmap);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    g.TranslateTransform(0.0f, static_cast<float>(placement.topPadding));
    g.ScaleTransform(placement.scale, placement.scale);
    cat::Draw(g, frame.pose, *frame.skin, placement.topPadding / placement.scale);
    return true;
}

bool ReduceMotion() {
    BOOL animate = TRUE;
    return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0) && !animate;
}

float PoofProgress(Clock::time_point start, Clock::time_point now) {
    return std::chrono::duration<float>(now - start) / std::chrono::duration<float>(poofDuration);
}

class PoofWindow {
public:
    PoofWindow(Placement placement, Clock::time_point start, bool reduceMotion,
               const DisplayedFrame* leaving = nullptr)
        : placement_(placement), start_(start), reduceMotion_(reduceMotion),
          window_(CreateOverlay(L"MeltyCat poof")) {
        if (leaving) {
            opacity_ = leaving->opacity;
            RenderCat(snapshot_, *leaving);
        }
    }

    ~PoofWindow() { if (window_) DestroyWindow(window_); }

    bool Advance(Clock::time_point now) {
        const float progress = PoofProgress(start_, now);
        if (!window_ || progress >= 1) return false;
        const float scale = placement_.scale;
        const LONG margin = std::lround(poofMargin * scale);
        const LONG width = std::lround(cat::width * scale) + 2 * margin;
        const LONG height = std::lround(cat::height * scale) + placement_.topPadding + 2 * margin;
        if (!surface_.Resize(width, height)) return false;
        {
            Gdiplus::Bitmap frame(width, height, width * 4, PixelFormat32bppPARGB, surface_.Pixels());
            Gdiplus::Graphics g(&frame);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));
            if (snapshot_.Handle()) {
                const auto size = snapshot_.Size();
                Gdiplus::Bitmap cat(size.cx, size.cy, size.cx * 4, PixelFormat32bppPARGB, snapshot_.Pixels());
                const auto offset = static_cast<float>(margin);
                DrawWithOpacity(g, cat, {offset, offset, static_cast<float>(size.cx), static_cast<float>(size.cy)},
                                PoofCatOpacity(false, reduceMotion_, progress) * (opacity_ / 255.0f));
            }
            if (!reduceMotion_) {
                g.TranslateTransform(static_cast<float>(margin), static_cast<float>(margin + placement_.topPadding));
                g.ScaleTransform(scale, scale);
                DrawPoof(g, progress, scale);
            }
        }
        const POINT position{placement_.origin.x - margin, placement_.origin.y - margin};
        if (!Present(window_, surface_, position, 255, !visible_)) return false;
        visible_ = true;
        return true;
    }

private:
    Placement placement_;
    Clock::time_point start_;
    bool reduceMotion_;
    BYTE opacity_ = 255;
    HWND window_;
    bool visible_ = false;
    Surface surface_;
    Surface snapshot_;
};

struct App {
    HWND overlay = nullptr;
    HWND menuOwner = nullptr;
    HWND trayPerch = nullptr;
    UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    bool menuOpen = false;
    std::optional<DisplayedFrame> displayed;
    struct Arrival { Clock::time_point start; bool reduceMotion; };
    std::optional<Arrival> arrival;
    PerchPresence presence;
    std::list<PoofWindow> poofs;
    POINT cursor{};
    Surface surface;
    cat::Animation animation;
    NOTIFYICONDATAW tray{};
    SkinLibrary skins;
    TrayMenu trayMenu{skins};
    SkinWindow skinWindow{skins, trayMenu, [this] { RefreshTrayIcon(); }};

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

    HWND PerchForeground(HWND foreground) {
        if (!foreground || (!trayPerch && !displayed)) return foreground;
        const HWND target = trayPerch ? trayPerch : displayed->placement.target;
        const HWND root = GetAncestor(foreground, GA_ROOT);
        if (root == menuOwner && menuOwner) return target;
        if (root != target) {
            wchar_t name[128]{};
            GetClassNameW(root, name, static_cast<int>(std::size(name)));
            // The overflow can take focus before Explorer sends an icon callback.
            if (trayPerch ? IsTrayClass(name) : IsTrayOverflowClass(name)) return target;
        }
        trayPerch = nullptr;
        return foreground;
    }

    Clock::duration Frame(HWND foreground) {
        if (menuOpen) return frameInterval;
        foreground = PerchForeground(foreground);
        Placement placement{};
        const bool observed = foreground != skinWindow.Handle() && FindPlacement(foreground, overlay, placement);
        const auto now = Clock::now();
        const int primaryButton = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        const bool holding = observed && !presence.Shown() && (GetAsyncKeyState(primaryButton) & 0x8000) != 0;
        const auto event = presence.Update(now, observed ? placement.target : nullptr, holding);
        if (event == PerchEvent::Disappear) {
            trayPerch = nullptr;
            if (displayed) poofs.emplace_back(displayed->placement, now, ReduceMotion(), &*displayed);
        }
        if (event == PerchEvent::Appear) arrival = Arrival{now, ReduceMotion()};
        if (event == PerchEvent::Move) arrival.reset();
        poofs.remove_if([now](PoofWindow& effect) { return !effect.Advance(now); });

        // A momentary loss of the perch keeps the last valid position.
        const bool hasPerch = presence.Shown() && (observed || displayed.has_value());
        if (hasPerch && !observed) placement = displayed->placement;
        POINT position{};
        const bool hasCursor = GetCursorPos(&position) != FALSE;
        if (hasCursor) cursor = position;
        const bool newPerch = !displayed || displayed->placement.target != placement.target;
        const bool samePlacement = hasPerch && displayed && displayed->placement == placement;
        const float headroom = placement.topPadding / placement.scale;
        const auto localCursor = placement.LocalCursor(cursor);
        const bool overCat = hasCursor && samePlacement &&
                             cat::HitsPettingArea(localCursor, displayed->pose, *displayed->skin, headroom);
        LASTINPUTINFO input{sizeof(input), 0};
        const auto inputTick = GetLastInputInfo(&input) ? std::optional<DWORD>(input.dwTime) : std::nullopt;
        animation.Observe(now, cursor, inputTick, placement.scale, overCat);

        if (!hasPerch) {
            if (displayed) ShowWindow(overlay, SW_HIDE);
            displayed.reset();
            arrival.reset();
            return poofs.empty() && !presence.Settling() ? noPerchInterval : frameInterval;
        }

        const auto pose = animation.Sample(now, localCursor, placement.tailDirection, newPerch);
        const bool arriving = arrival.has_value();
        BYTE opacity = 255;
        if (arrival) {
            const float progress = PoofProgress(arrival->start, now);
            opacity = static_cast<BYTE>(std::lround(255 * PoofCatOpacity(true, arrival->reduceMotion, progress)));
            if (progress >= 1) arrival.reset();
        }
        const auto nextFrame = (presence.Settling() || arrival || !poofs.empty()) ? frameInterval
            : PerchFrameInterval(pose, inputTick && GetTickCount() - *inputTick < 800);
        const bool resting = pose.alert == 0 && pose.tailMotion == 0 && pose.dreamCount == 0 &&
                             pose.earTwitch == 0 && pose.squint == 0 && pose.petSymbolCount == 0;
        const auto& skin = trayMenu.ActiveSkin();
        if (!arriving && resting && samePlacement && displayed->resting && displayed->skin == skin &&
            pose.pawTuckLeft == displayed->pose.pawTuckLeft && pose.pawTuckRight == displayed->pose.pawTuckRight) {
            return nextFrame;
        }

        const DisplayedFrame frame{placement, pose, skin, resting, opacity};
        if (!RenderCat(surface, frame)) return nextFrame;
        if (!Present(overlay, surface, placement.origin, opacity, newPerch)) return nextFrame;
        if (event == PerchEvent::Appear && arrival && !arrival->reduceMotion) {
            // The arrival cloud is above the main overlay.
            poofs.emplace_back(placement, now, false);
            if (!poofs.back().Advance(now)) poofs.pop_back();
        }
        displayed = frame;
        return nextFrame;
    }

    void HandleTrayCommand(UINT command) {
        const auto action = trayMenu.Select(command);
        if (action == TrayMenu::Action::Exit) {
            DestroyWindow(overlay);
            return;
        }
        if (action == TrayMenu::Action::ChangeSkin) {
            skinWindow.Show();
        } else {
            if (action == TrayMenu::Action::SkinChanged) {
                RefreshTrayIcon();
                const HWND foreground = GetForegroundWindow();
                Frame(foreground ? foreground : menuOwner);
            }
            if (command) skinWindow.Refresh();
        }
    }

    void RefreshTrayIcon() {
        const HICON icon = MakeTrayIcon(*trayMenu.ActiveSkin());
        if (!icon) return;
        const HICON previousIcon = tray.hIcon;
        tray.hIcon = icon;
        if (!Shell_NotifyIconW(NIM_MODIFY, &tray)) RegisterTrayIcon();
        if (previousIcon) DestroyIcon(previousIcon);
    }

    void RememberTrayPerch() {
        // Explorer may already have taken focus by the time the tray callback runs.
        trayPerch = displayed ? displayed->placement.target : nullptr;
    }

    void ShowTrayMenu() {
        if (menuOpen) return;
        const HMENU menu = trayMenu.Create();
        if (!menu) return;
        RememberTrayPerch();
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
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
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
    case WM_COPYDATA: {
        const auto data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (!data || data->dwData != importSkinMessage || !data->lpData ||
            data->cbData < 2 * sizeof(wchar_t) || data->cbData > 32768 * sizeof(wchar_t) ||
            data->cbData % sizeof(wchar_t) != 0) return FALSE;
        const auto text = static_cast<const wchar_t*>(data->lpData);
        const auto length = data->cbData / sizeof(wchar_t);
        if (std::wmemchr(text, L'\0', length) != text + length - 1) return FALSE;
        const std::filesystem::path file(text);
        if (!IsSkinFile(file)) return FALSE;
        app->skinWindow.Show(file);
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == trayTimer) app->RegisterTrayIcon();
        return 0;
    case trayMessage:
        if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN) app->RememberTrayPerch();
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) app->ShowTrayMenu();
        return 0;
    case WM_DESTROY:
        KillTimer(window, trayTimer);
        Shell_NotifyIconW(NIM_DELETE, &app->tray);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int Run(HINSTANCE instance, const std::filesystem::path& skinFile) {
    App app;
    // Keep tray commands available even when the cat overlay itself is hidden.
    app.menuOwner = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"STATIC", L"", WS_POPUP,
                                    0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!app.menuOwner || !SetLayeredWindowAttributes(app.menuOwner, 0, 0, LWA_ALPHA)) return 1;

    const WNDCLASSW windowType{0, WindowProc, 0, 0, instance, nullptr, nullptr, nullptr,
                               nullptr, windowClass};
    if (!RegisterClassW(&windowType)) return 1;

    app.overlay = CreateOverlay(L"meltycat", &app);
    if (!app.overlay) return 1;

    app.tray.cbSize = sizeof(app.tray);
    app.tray.hWnd = app.overlay;
    app.tray.uID = 1;
    app.tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    app.tray.uCallbackMessage = trayMessage;
    app.tray.hIcon = MakeTrayIcon(*app.trayMenu.ActiveSkin());
    wcscpy_s(app.tray.szTip, L"meltycat");
    if (!app.tray.hIcon) {
        DestroyWindow(app.overlay);
        return 1;
    }

    app.RegisterTrayIcon();
    RegisterSkinAssociation();
    if (!skinFile.empty()) app.skinWindow.Show(skinFile);
    FrameTimer timer;
    const auto start = Clock::now();
    auto nextFrame = app.Frame(GetForegroundWindow());
    bool running = timer.Schedule(start, nextFrame);
    while (running) {
        const DWORD event = timer.Wait();
        if (event == WAIT_OBJECT_0) {
            const auto frameStart = Clock::now();
            nextFrame = app.Frame(GetForegroundWindow());
            running = timer.Schedule(frameStart, nextFrame);
        } else if (event != WAIT_OBJECT_0 + 1) {
            break;
        }

        MSG message{};
        for (int i = 0; i < 64 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++i) {
            if (message.message == WM_QUIT) return static_cast<int>(message.wParam);
            if (app.skinWindow.HandleKey(message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    DestroyWindow(app.overlay);
    return 1;
}

std::filesystem::path SkinFileArgument() {
    int count = 0;
    const auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return {};
    std::filesystem::path file;
    if (count == 2 && IsSkinFile(arguments[1])) {
        std::error_code ec;
        file = std::filesystem::absolute(arguments[1], ec);
        if (ec) file.clear();
    }
    LocalFree(arguments);
    return file;
}

void ForwardSkinFile(const std::filesystem::path& file) {
    if (file.empty()) return;
    HWND overlay = nullptr;
    for (int attempt = 0; attempt < 80 && !overlay; ++attempt) {
        overlay = FindWindowW(windowClass, L"meltycat");
        if (!overlay) Sleep(25);
    }
    if (!overlay) return;
    DWORD pid = 0;
    if (GetWindowThreadProcessId(overlay, &pid)) AllowSetForegroundWindow(pid);
    auto path = file.native();
    if (path.size() >= 32768) return;
    COPYDATASTRUCT data{importSkinMessage, static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)), path.data()};
    SendMessageTimeoutW(overlay, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 3000, nullptr);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const auto file = SkinFileArgument();
    const HANDLE singleInstance = CreateMutexW(nullptr, FALSE, L"Local\\MeltyCat.Prototype");
    const DWORD mutexError = GetLastError();
    if (!singleInstance) return 1;
    if (mutexError == ERROR_ALREADY_EXISTS) {
        ForwardSkinFile(file);
        CloseHandle(singleInstance);
        return 0;
    }

    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR gdiplus = 0;
    int result = 1;
    if (SUCCEEDED(RoInitialize(RO_INIT_SINGLETHREADED))) {
        if (Gdiplus::GdiplusStartup(&gdiplus, &input, nullptr) == Gdiplus::Ok) {
            try {
                result = Run(instance, file);
            } catch (const std::exception&) {
                result = 1;
            }
            Gdiplus::GdiplusShutdown(gdiplus);
        }
        RoUninitialize();
    }
    CloseHandle(singleInstance);
    if (result != 0) {
        MessageBoxW(nullptr, L"启动失败。请从 Windows 桌面重新运行 meltycat。",
                    L"meltycat", MB_OK | MB_ICONERROR);
    }
    return result;
}
