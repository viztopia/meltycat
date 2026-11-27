#include "skin_library.h"
#include "skin_resources.h"

#include <shlobj.h>
#include <winrt/base.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace {

constexpr char customPrefix[] = "custom:";
constexpr wchar_t skinExtension[] = L".meltycat";

constexpr struct { const char* id; WORD resource; } builtins[]{
    {"ragdoll", ID_SKIN_RAGDOLL}, {"ragdoll-mix", ID_SKIN_RAGDOLL_MIX}, {"maine-coon", ID_SKIN_MAINE_COON},
    {"siberian", ID_SKIN_SIBERIAN}, {"orange-tabby", ID_SKIN_ORANGE_TABBY}, {"black", ID_SKIN_BLACK},
    {"li-hua", ID_SKIN_LI_HUA}, {"classic-tuxedo", ID_SKIN_CLASSIC_TUXEDO}, {"tuxedo", ID_SKIN_TUXEDO}
};

std::string_view Resource(WORD id) {
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!resource) return {};
    const auto data = LoadResource(nullptr, resource);
    if (!data) return {};
    const auto bytes = static_cast<const char*>(LockResource(data));
    return bytes ? std::string_view(bytes, SizeofResource(nullptr, resource)) : std::string_view();
}

std::filesystem::path CustomDirectory() {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) return {};
    const std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result / L"meltycat" / L"skins";
}

std::optional<std::string> CustomId(const std::filesystem::path& path) {
    if (!IsSkinFile(path)) return std::nullopt;
    const auto name = path.stem().native();
    if (name.size() <= 5 || name.compare(0, 5, L"skin-") != 0 ||
        name.find_first_not_of(L"0123456789", 5) != std::wstring::npos) return std::nullopt;
    return customPrefix + winrt::to_string(name);
}

} // namespace

bool IsSkinFile(const std::filesystem::path& path) {
    return CompareStringOrdinal(path.extension().c_str(), -1, skinExtension, -1, TRUE) == CSTR_EQUAL;
}

std::filesystem::path ExecutablePath() {
    std::wstring executable(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (size == 0 || size >= executable.size()) return {};
    executable.resize(size);
    return executable;
}

void RegisterSkinAssociation() {
    const auto executable = ExecutablePath();
    if (executable.empty()) return;
    constexpr wchar_t fileType[] = L"meltycat.skin";
    constexpr wchar_t typeKey[] = L"Software\\Classes\\meltycat.skin";
    constexpr wchar_t commandKey[] = L"Software\\Classes\\meltycat.skin\\shell\\open\\command";
    constexpr wchar_t extensionKey[] = L"Software\\Classes\\.meltycat";
    constexpr wchar_t openWithKey[] = L"Software\\Classes\\.meltycat\\OpenWithProgids";
    const std::wstring command = L"\"" + executable.native() + L"\" \"%1\"";
    const auto setText = [](const wchar_t* key, const std::wstring& text) {
        return RegSetKeyValueW(HKEY_CURRENT_USER, key, nullptr, REG_SZ,
                              text.c_str(), static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    };
    if (!setText(typeKey, L"meltycat skin") || !setText(commandKey, command)) return;
    RegSetKeyValueW(HKEY_CURRENT_USER, openWithKey, fileType, REG_NONE, nullptr, 0);

    // Register an initial handler only when the extension has no existing default.
    DWORD existingBytes = 0;
    const auto existing = RegGetValueW(HKEY_CLASSES_ROOT, L".meltycat", nullptr, RRF_RT_ANY,
                                       nullptr, nullptr, &existingBytes);
    if (existing == ERROR_FILE_NOT_FOUND || (existing == ERROR_SUCCESS && existingBytes <= sizeof(wchar_t))) {
        setText(extensionKey, fileType);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

std::optional<std::string> ReadSkinFile(const std::filesystem::path& path, std::wstring& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = L"couldn't open the file";
        return std::nullopt;
    }
    const auto size = static_cast<std::streamoff>(file.tellg());
    if (size < 0 || size > cat::maxSkinBytes) {
        error = size < 0 ? L"couldn't read the file" : L"file is larger than 512 KB";
        return std::nullopt;
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!file.seekg(0).read(bytes.data(), size) || file.peek() != EOF) {
        error = L"couldn't read the file";
        return std::nullopt;
    }
    return bytes;
}

SkinLibrary::SkinLibrary() : SkinLibrary(CustomDirectory()) {}

SkinLibrary::SkinLibrary(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::wstring error;
    for (const auto& builtin : builtins) {
        auto skin = cat::ParseSkin(Resource(builtin.resource), error);
        if (!skin) throw std::runtime_error("Invalid built-in skin: " + std::string(builtin.id));
        builtin_.push_back({builtin.id, std::make_shared<cat::SkinSpec>(std::move(*skin))});
    }

    if (directory_.empty()) return;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory_, ec), end; !ec && it != end; it.increment(ec)) {
        const auto id = CustomId(it->path());
        if (!id) continue;
        const auto text = ReadSkinFile(it->path(), error);
        if (!text) continue;
        auto skin = cat::ParseSkin(*text, error);
        if (skin) custom_.push_back({*id, std::make_shared<cat::SkinSpec>(std::move(*skin))});
    }
    std::sort(custom_.begin(), custom_.end(), [](const SkinEntry& a, const SkinEntry& b) { return a.id < b.id; });
}

Skin SkinLibrary::Find(std::string_view id) const {
    const auto& entries = id.substr(0, sizeof(customPrefix) - 1) == customPrefix ? custom_ : builtin_;
    const auto entry = std::find_if(entries.begin(), entries.end(), [id](const SkinEntry& item) {
        return item.id == id;
    });
    return entry == entries.end() ? nullptr : entry->skin;
}

std::optional<SkinEntry> SkinLibrary::Import(std::string_view text, std::wstring& error) {
    auto skin = cat::ParseSkin(text, error);
    if (!skin) return std::nullopt;
    std::error_code ec;
    if (!directory_.empty()) std::filesystem::create_directories(directory_, ec);
    if (directory_.empty() || ec) {
        error = L"couldn't create the skins folder";
        return std::nullopt;
    }

    using namespace std::chrono;
    auto millis = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    for (;;) {
        const auto name = L"skin-" + std::to_wstring(millis++);
        const auto path = directory_ / (name + skinExtension);
        const auto temporary = directory_ / (name + L".tmp");
        HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_EXISTS) continue;
            break;
        }
        DWORD written = 0;
        const bool saved = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                           written == text.size();
        CloseHandle(file);
        if (saved && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
            SkinEntry entry{customPrefix + winrt::to_string(name), std::make_shared<cat::SkinSpec>(std::move(*skin))};
            custom_.push_back(entry);
            return entry;
        }
        const DWORD failure = GetLastError();
        DeleteFileW(temporary.c_str());
        if (!saved || (failure != ERROR_FILE_EXISTS && failure != ERROR_ALREADY_EXISTS)) break;
    }
    error = L"couldn't save the skin";
    return std::nullopt;
}

bool SkinLibrary::Delete(std::string_view id) {
    const auto entry = std::find_if(custom_.begin(), custom_.end(), [id](const SkinEntry& item) {
        return item.id == id;
    });
    if (entry == custom_.end()) return false;
    const auto name = winrt::to_hstring(id.substr(sizeof(customPrefix) - 1));
    const auto path = directory_ / (std::wstring(name) + skinExtension);
    if (!DeleteFileW(path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) return false;
    custom_.erase(entry);
    return true;
}

std::wstring SkinLibrary::Guide() {
    std::string text(Resource(ID_SKIN_GUIDE));
    const auto append = [&](const char* title, WORD id) {
        text += "\n\n## Appendix: ";
        text += title;
        text += "\n\n```json\n";
        text += Resource(id);
        if (text.back() != '\n') text += '\n';
        text += "```\n";
    };
    append("shorthair template (li-hua)", ID_SKIN_LI_HUA);
    append("longhair template (ragdoll)", ID_SKIN_RAGDOLL);
    return std::wstring(winrt::to_hstring(text));
}

