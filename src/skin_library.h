#pragma once

#include "skin_spec.h"

#include <filesystem>
#include <memory>

using Skin = std::shared_ptr<const cat::SkinSpec>;

struct SkinEntry {
    std::string id;
    Skin skin;
};

class SkinLibrary {
public:
    SkinLibrary();
    explicit SkinLibrary(std::filesystem::path directory);

    Skin Find(std::string_view id) const;
    const std::vector<SkinEntry>& CustomSkins() const { return custom_; }
    std::optional<SkinEntry> Import(std::string_view text, std::wstring& error);
    bool Delete(std::string_view id);
    static std::wstring Guide();

private:
    std::filesystem::path directory_;
    std::vector<SkinEntry> builtin_;
    std::vector<SkinEntry> custom_;
};

std::optional<std::string> ReadSkinFile(const std::filesystem::path& path, std::wstring& error);
bool IsSkinFile(const std::filesystem::path& path);
std::filesystem::path ExecutablePath();
void RegisterSkinAssociation();
