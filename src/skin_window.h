#pragma once

#include "tray_menu.h"

#include <functional>

class SkinWindow {
public:
    SkinWindow(SkinLibrary& library, TrayMenu& menu, std::function<void()> changed);
    ~SkinWindow();

    HWND Handle() const { return window_; }
    void Show(const std::filesystem::path& file = {});
    void Refresh();
    bool HandleKey(MSG& message);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void Layout();
    void Paint();
    void ChooseFile();
    void ImportFile(const std::filesystem::path& path);
    void ImportText(std::string_view text);
    void Paste();
    void CopyGuide();
    void DeleteSelected();
    void Status(std::wstring_view text, bool error = false);
    const SkinEntry* Selected() const;

    SkinLibrary& library_;
    TrayMenu& menu_;
    std::function<void()> changed_;
    HWND window_ = nullptr;
    HWND hint_ = nullptr, choose_ = nullptr, paste_ = nullptr, guide_ = nullptr;
    HWND list_ = nullptr, remove_ = nullptr, status_ = nullptr;
    HFONT font_ = nullptr;
    bool error_ = false;
};
