#pragma once

#include "skin_library.h"

#include <cstddef>
#include <filesystem>

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
    const wchar_t* custom;
};

class TrayMenu {
public:
    enum class Action { None, SkinChanged, ChangeSkin, Exit };

    explicit TrayMenu(const SkinLibrary& library);

    const Skin& ActiveSkin() const { return skin_; }
    const std::string& SkinId() const { return skinId_; }
    const SkinText& Text() const;
    bool SetSkin(std::string_view id);
    HMENU Create() const;
    Action Select(UINT command);

private:
    std::filesystem::path skinSettings_;
    std::filesystem::path languageSettings_;
    const SkinLibrary& library_;
    std::string skinId_;
    Skin skin_;
    std::size_t languageIndex_;
};
