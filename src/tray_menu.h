#pragma once

#include "poof.h"
#include "skin_library.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct SkinText {
    const wchar_t* title;
    const wchar_t* dropHint;
    const wchar_t* chooseFile;
    const wchar_t* paste;
    const wchar_t* copyGuide;
    const wchar_t* guideCopied;
    const wchar_t* remove;
    const wchar_t* imported;
    const wchar_t* importFailed;
};

struct UpdateText {
    const wchar_t* checkNow;
    const wchar_t* automatically;
    const wchar_t* availableFormat;
    const wchar_t* upToDateFormat;
    const wchar_t* newerFormat;
    const wchar_t* failed;
    const wchar_t* download;
    const wchar_t* later;
    const wchar_t* ok;

    std::wstring Available(std::wstring_view version) const;
    std::wstring UpToDate(std::wstring_view current) const;
    std::wstring Newer(std::wstring_view version, std::wstring_view current) const;
};

class TrayMenu {
public:
    enum class Action { None, SkinChanged, ChangeSkin, CheckForUpdates, Exit };

    explicit TrayMenu(const SkinLibrary& library);

    const Skin& ActiveSkin() const { return skin_; }
    const std::string& SkinId() const { return skinId_; }
    PoofMode PoofCloudMode() const { return poofMode_; }
    const SkinText& Text() const;
    const UpdateText& UpdateLabels() const;
    bool AutoCheckUpdates() const { return autoCheckUpdates_; }
    std::optional<std::chrono::seconds> LastUpdateCheck() const { return lastUpdateCheck_; }
    void SetAutoCheckUpdates(bool enabled);
    void RecordUpdateCheck(std::chrono::seconds time);
    bool SetSkin(std::string_view id);
    HMENU Create(std::wstring_view availableUpdate = {}) const;
    Action Select(UINT command);

private:
    std::filesystem::path skinSettings_;
    std::filesystem::path languageSettings_;
    std::filesystem::path poofSettings_;
    std::filesystem::path autoUpdateSettings_;
    std::filesystem::path lastUpdateSettings_;
    const SkinLibrary& library_;
    std::string skinId_;
    Skin skin_;
    std::size_t languageIndex_;
    PoofMode poofMode_ = PoofMode::Quiet;
    bool autoCheckUpdates_ = true;
    std::optional<std::chrono::seconds> lastUpdateCheck_;
};
