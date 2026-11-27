#include "skin_window.h"

#include "cat.h"
#include "skin_resources.h"

#include <commdlg.h>
#include <shellapi.h>
#include <winrt/base.h>

#include <algorithm>
#include <cwchar>
#include <cstring>

namespace {

constexpr wchar_t skinWindowClass[] = L"MeltyCat.SkinWindow";
constexpr DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
constexpr int windowWidth = 520;
constexpr int windowHeight = 400;
enum Control { Choose = 101, Paste, Guide, SkinList, Remove };

std::filesystem::path DroppedPath(HDROP drop) {
    if (DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) != 1) return {};
    const auto length = DragQueryFileW(drop, 0, nullptr, 0);
    std::wstring path(length + 1, L'\0');
    if (!length || DragQueryFileW(drop, 0, path.data(), length + 1) != length) return {};
    path.resize(length);
    return path;
}

} // namespace

SkinWindow::SkinWindow(SkinLibrary& library, TrayMenu& menu, std::function<void()> changed)
    : library_(library), menu_(menu), changed_(std::move(changed)) {}

SkinWindow::~SkinWindow() {
    if (window_) DestroyWindow(window_);
    if (font_) DeleteObject(font_);
}

void SkinWindow::Show(const std::filesystem::path& file) {
    if (!window_) {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        const auto loadIcon = [instance](int widthMetric, int heightMetric) {
            return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(ID_APP_ICON), IMAGE_ICON,
                                                GetSystemMetrics(widthMetric), GetSystemMetrics(heightMetric),
                                                LR_SHARED));
        };
        const WNDCLASSEXW type{sizeof(WNDCLASSEXW), 0, WindowProc, 0, 0, instance,
                               loadIcon(SM_CXICON, SM_CYICON), LoadCursorW(nullptr, IDC_ARROW),
                               GetSysColorBrush(COLOR_WINDOW), nullptr, skinWindowClass,
                               loadIcon(SM_CXSMICON, SM_CYSMICON)};
        if (!RegisterClassExW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

        POINT cursor{};
        GetCursorPos(&cursor);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor);
        window_ = CreateWindowExW(WS_EX_CONTROLPARENT, skinWindowClass, menu_.Text().title, windowStyle,
                                  cursor.x, cursor.y, windowWidth, windowHeight,
                                  nullptr, nullptr, instance, this);
        if (!window_) return;
        const UINT dpi = GetDpiForWindow(window_);
        RECT bounds{0, 0, MulDiv(windowWidth, dpi, 96), MulDiv(windowHeight, dpi, 96)};
        AdjustWindowRectExForDpi(&bounds, windowStyle, FALSE, WS_EX_CONTROLPARENT, dpi);
        const LONG w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
        const RECT& work = monitor.rcWork;
        SetWindowPos(window_, nullptr, work.left + std::max(0L, (work.right - work.left - w) / 2),
                     work.top + std::max(0L, (work.bottom - work.top - h) / 2), w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    Refresh();
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    if (!file.empty()) ImportFile(file);
}

void SkinWindow::Refresh() {
    if (!window_) return;
    const auto& text = menu_.Text();
    SetWindowTextW(window_, text.title);
    SetWindowTextW(hint_, text.dropHint);
    SetWindowTextW(choose_, text.chooseFile);
    SetWindowTextW(paste_, text.paste);
    SetWindowTextW(guide_, text.copyGuide);
    SetWindowTextW(remove_, text.remove);
    SendMessageW(list_, CB_RESETCONTENT, 0, 0);
    const auto& skins = library_.CustomSkins();
    LRESULT selected = CB_ERR;
    for (const auto& entry : skins) {
        const auto index = SendMessageW(list_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.skin->name.c_str()));
        if (entry.id == menu_.SkinId()) selected = index;
    }
    if (selected != CB_ERR) SendMessageW(list_, CB_SETCURSEL, selected, 0);
    ShowWindow(list_, skins.empty() ? SW_HIDE : SW_SHOW);
    ShowWindow(remove_, skins.empty() ? SW_HIDE : SW_SHOW);
    EnableWindow(remove_, selected != CB_ERR);
    InvalidateRect(window_, nullptr, TRUE);
}

bool SkinWindow::HandleKey(MSG& message) {
    if (!window_ || !IsWindowVisible(window_) ||
        (message.hwnd != window_ && !IsChild(window_, message.hwnd))) return false;
    if (message.message == WM_KEYDOWN && message.wParam == 'V' && GetKeyState(VK_CONTROL) < 0) {
        Paste();
        return true;
    }
    return IsDialogMessageW(window_, &message) != FALSE;
}

void SkinWindow::Layout() {
    const UINT dpi = GetDpiForWindow(window_);
    const auto px = [dpi](int value) { return MulDiv(value, dpi, 96); };
    const auto place = [&](HWND control, int x, int y, int w, int h) {
        MoveWindow(control, px(x), px(y), px(w), px(h), TRUE);
    };
    place(hint_, 20, 226, 480, 42);
    place(choose_, 33, 272, 146, 30);
    place(paste_, 187, 272, 146, 30);
    place(guide_, 341, 272, 146, 30);
    place(list_, 104, 316, 208, 180);
    place(remove_, 320, 315, 96, 29);
    place(status_, 20, 355, 480, 42);

    const HFONT old = font_;
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    font_ = SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)
        ? CreateFontIndirectW(&metrics.lfMessageFont) : nullptr;
    const HFONT use = font_ ? font_ : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    for (const HWND control : {hint_, choose_, paste_, guide_, list_, remove_, status_}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(use), TRUE);
    }
    if (old) DeleteObject(old);
}

void SkinWindow::Paint() {
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(window_, &paint);
    {
        Gdiplus::Graphics g(dc);
        const float scale = GetDpiForWindow(window_) / 96.0f;
        g.ScaleTransform(scale, scale);
        const auto system = [](int role) {
            const auto color = GetSysColor(role);
            return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
        };
        Gdiplus::SolidBrush backdrop(system(COLOR_BTNFACE)), panel(system(COLOR_WINDOW));
        Gdiplus::Pen edge(system(COLOR_3DSHADOW), 1.0f);
        g.FillRectangle(&backdrop, 20, 16, 480, 198);
        g.FillRectangle(&panel, 116, 94, 288, 120);
        g.DrawLine(&edge, 116, 94, 404, 94);
        g.DrawLine(&edge, 116, 94, 116, 214);
        g.DrawLine(&edge, 404, 94, 404, 214);
        // The body rests on the top-right of the preview ledge.
        g.TranslateTransform(404.0f - cat::cornerX, 94.0f - cat::cornerY);
        const cat::Pose pose{{52, 40}, 0, 0, 1, -1, 1, 1, 0, 0};
        cat::Draw(g, pose, *menu_.ActiveSkin(), 6);
    }
    EndPaint(window_, &paint);
}

void SkinWindow::Status(std::wstring_view text, bool error) {
    error_ = error;
    SetWindowTextW(status_, std::wstring(text).c_str());
}

void SkinWindow::ChooseFile() {
    std::wstring path(32768, L'\0');
    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"meltycat (*.meltycat)\0*.meltycat\0\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"meltycat";
    dialog.lpstrTitle = menu_.Text().chooseFile;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (GetOpenFileNameW(&dialog)) ImportFile(path.c_str());
}

void SkinWindow::ImportFile(const std::filesystem::path& path) {
    std::wstring error;
    if (!IsSkinFile(path)) {
        error = L"expected a .meltycat file";
    } else if (const auto data = ReadSkinFile(path, error)) {
        ImportText(*data);
        return;
    }
    Status(std::wstring(menu_.Text().importFailed) + L": " + error, true);
}

void SkinWindow::ImportText(std::string_view text) {
    std::wstring error;
    const auto skin = library_.Import(text, error);
    if (!skin) {
        Status(std::wstring(menu_.Text().importFailed) + L": " + error, true);
        return;
    }
    menu_.SetSkin(skin->id);
    changed_();
    Refresh();
    Status(std::wstring(menu_.Text().imported) + L": " + skin->skin->name);
}

void SkinWindow::Paste() {
    if (!OpenClipboard(window_)) return;
    std::wstring text;
    bool tooLarge = false;
    std::filesystem::path file;
    if (const auto data = GetClipboardData(CF_HDROP)) {
        file = DroppedPath(static_cast<HDROP>(data));
    } else if (const auto unicode = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto value = static_cast<const wchar_t*>(GlobalLock(unicode))) {
            const auto size = std::min(GlobalSize(unicode) / sizeof(wchar_t), cat::maxSkinBytes + 1);
            if (const auto end = std::wmemchr(value, L'\0', size)) text.assign(value, end);
            else tooLarge = true;
            GlobalUnlock(unicode);
        }
    }
    CloseClipboard();
    if (!file.empty()) {
        ImportFile(file);
    } else if (!text.empty()) {
        try {
            ImportText(winrt::to_string(text));
        } catch (const winrt::hresult_error&) {
            Status(menu_.Text().importFailed, true);
        }
    } else if (tooLarge) {
        Status(std::wstring(menu_.Text().importFailed) + L": file is larger than 512 KB", true);
    }
}

void SkinWindow::CopyGuide() {
    const auto text = SkinLibrary::Guide();
    const auto size = (text.size() + 1) * sizeof(wchar_t);
    const auto data = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!data) return;
    const auto memory = GlobalLock(data);
    if (!memory) {
        GlobalFree(data);
        return;
    }
    std::memcpy(memory, text.c_str(), size);
    GlobalUnlock(data);
    bool copied = false;
    if (OpenClipboard(window_)) {
        copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
        CloseClipboard();
    }
    if (copied) Status(menu_.Text().guideCopied);
    else GlobalFree(data);
}

const SkinEntry* SkinWindow::Selected() const {
    const auto index = SendMessageW(list_, CB_GETCURSEL, 0, 0);
    const auto& skins = library_.CustomSkins();
    return index >= 0 && static_cast<std::size_t>(index) < skins.size() ? &skins[index] : nullptr;
}

void SkinWindow::DeleteSelected() {
    const auto* selected = Selected();
    if (!selected) return;
    const bool active = selected->id == menu_.SkinId();
    if (!library_.Delete(selected->id)) return;
    if (active && menu_.SetSkin("black")) changed_();
    Refresh();
    Status(L"");
}

LRESULT CALLBACK SkinWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        const auto self = static_cast<SkinWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    const auto self = reinterpret_cast<SkinWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!self) return DefWindowProcW(window, message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        self->window_ = nullptr;
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return self->OnMessage(message, wParam, lParam);
}

LRESULT SkinWindow::OnMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        const auto control = [&](const wchar_t* type, DWORD style, int id = 0) {
            return CreateWindowExW(0, type, L"", WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                                   window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
        };
        hint_ = control(L"STATIC", SS_CENTER);
        choose_ = control(L"BUTTON", WS_TABSTOP, Choose);
        paste_ = control(L"BUTTON", WS_TABSTOP, Control::Paste);
        guide_ = control(L"BUTTON", WS_TABSTOP, Guide);
        list_ = control(L"COMBOBOX", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, SkinList);
        remove_ = control(L"BUTTON", WS_TABSTOP, Remove);
        status_ = control(L"STATIC", SS_CENTER | SS_NOPREFIX);
        if (!hint_ || !choose_ || !paste_ || !guide_ || !list_ || !remove_ || !status_) return -1;
        Layout();
        DragAcceptFiles(window_, TRUE);
        return 0;
    }
    case WM_DPICHANGED: {
        const auto rect = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window_, nullptr, rect->left, rect->top, rect->right - rect->left,
                     rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }
    case WM_PAINT:
        Paint();
        return 0;
    case WM_CTLCOLORSTATIC: {
        const auto dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, error_ && reinterpret_cast<HWND>(lParam) == status_ ? RGB(158, 32, 32)
                                                                           : GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_DROPFILES: {
        const auto drop = reinterpret_cast<HDROP>(wParam);
        const auto path = DroppedPath(drop);
        DragFinish(drop);
        ImportFile(path);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case Choose: ChooseFile(); return 0;
        case Control::Paste: Paste(); return 0;
        case Guide: CopyGuide(); return 0;
        case Remove: DeleteSelected(); return 0;
        case SkinList:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                const auto* selected = Selected();
                EnableWindow(remove_, selected != nullptr);
                if (selected && menu_.SetSkin(selected->id)) {
                    changed_();
                    InvalidateRect(window_, nullptr, TRUE);
                }
            }
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(window_, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

