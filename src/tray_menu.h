#pragma once

#include "cat.h"

#include <cstddef>
#include <filesystem>

class TrayMenu {
public:
    enum class Action { None, SkinChanged, Exit };

    TrayMenu();

    cat::Skin Skin() const { return skin_; }
    HMENU Create() const;
    Action Select(UINT command);

private:
    std::filesystem::path skinSettings_;
    std::filesystem::path languageSettings_;
    cat::Skin skin_ = cat::Skin::Black;
    std::size_t languageIndex_ = 0;
};
