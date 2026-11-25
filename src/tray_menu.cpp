#include "tray_menu.h"

#include <fstream>
#include <iterator>
#include <string>

namespace {

constexpr UINT exitCommand = 1;

struct SkinChoice {
    cat::Skin skin;
    UINT command;
    const char* setting;
};

constexpr SkinChoice skinChoices[]{
    {cat::Skin::Ragdoll, 22, "ragdoll"},
    {cat::Skin::MaineCoon, 23, "maine-coon"},
    {cat::Skin::Siberian, 24, "siberian"},
    {cat::Skin::OrangeTabby, 9, "orange-tabby"},
    {cat::Skin::Black, 2, "black"},
    {cat::Skin::LiHua, 21, "li-hua"},
    {cat::Skin::Tuxedo, 10, "classic-tuxedo"},
    {cat::Skin::Uni, 3, "tuxedo"}, // The original setting identifies Uni.
};

struct LanguageChoice {
    UINT command;
    const char* setting;
    const wchar_t* name;
    const wchar_t* menuLabel;
    const wchar_t* skinLabels[std::size(skinChoices)];
    const wchar_t* exitLabel;
};

constexpr LanguageChoice languageChoices[]{
    {4, "zh-CN", L"简体中文", L"语言", {L"布偶猫", L"缅因猫", L"西伯利亚猫", L"橘虎斑", L"黑猫", L"狸花猫", L"奶牛猫", L"海胆（Uni）"}, L"退出 meltycat"},
    {5, "zh-TW", L"繁體中文", L"語言", {L"布偶貓", L"緬因貓", L"西伯利亞貓", L"橘虎斑", L"黑貓", L"狸花貓", L"奶牛貓", L"海膽（Uni）"}, L"結束 meltycat"},
    {6, "en", L"English", L"Language", {L"Ragdoll", L"Maine Coon", L"Siberian", L"Orange Tabby", L"Black Cat", L"Li Hua", L"Tuxedo Cat", L"Uni"}, L"Exit meltycat"},
    {7, "ja", L"日本語", L"言語", {L"ラグドール", L"メインクーン", L"サイベリアン", L"茶トラ", L"黒猫", L"キジトラ", L"白黒猫", L"ウニ（Uni）"}, L"meltycat を終了"},
    {8, "ko", L"한국어", L"언어", {L"랙돌", L"메인쿤", L"시베리안", L"치즈태비", L"검은 고양이", L"고등어태비", L"흑백 고양이", L"우니 (Uni)"}, L"meltycat 종료"},
    {11, "es", L"Español", L"Idioma",
     {L"Ragdoll", L"Maine Coon", L"Siberiano", L"Gato atigrado naranja", L"Gato negro", L"Gato atigrado marrón", L"Gato blanco y negro", L"Uni"}, L"Salir de meltycat"},
    {12, "fr", L"Français", L"Langue",
     {L"Ragdoll", L"Maine Coon", L"Sibérien", L"Chat roux tigré", L"Chat noir", L"Chat brun tigré", L"Chat noir et blanc", L"Uni"}, L"Quitter meltycat"},
    {13, "ru", L"Русский язык", L"Язык",
     {L"Рэгдолл", L"Мейн-кун", L"Сибирская кошка", L"Рыжий полосатый кот", L"Чёрный кот", L"Коричневый полосатый кот", L"Чёрно-белый кот", L"Уни (Uni)"}, L"Выйти из meltycat"},
    {14, "th", L"ภาษาไทย", L"ภาษา", {L"แร็กดอลล์", L"เมนคูน", L"ไซบีเรียน", L"แมวส้มลายเสือ", L"แมวดำ", L"แมวลายสลิด", L"แมวขาวดำ", L"Uni"}, L"ออกจาก meltycat"},
    {15, "vi", L"Tiếng Việt", L"Ngôn ngữ", {L"Mèo Ragdoll", L"Mèo Maine Coon", L"Mèo Siberia", L"Mèo mướp cam", L"Mèo đen", L"Mèo mướp nâu", L"Mèo đen trắng", L"Uni"}, L"Thoát meltycat"},
    {16, "de", L"Deutsch", L"Sprache",
     {L"Ragdoll", L"Maine Coon", L"Sibirische Katze", L"Orange getigerte Katze", L"Schwarze Katze", L"Braun getigerte Katze", L"Schwarz-weiße Katze", L"Uni"}, L"meltycat beenden"},
    {17, "id", L"Bahasa Indonesia", L"Bahasa",
     {L"Ragdoll", L"Maine Coon", L"Kucing Siberia", L"Kucing jingga belang", L"Kucing hitam", L"Kucing belang cokelat", L"Kucing hitam putih", L"Uni"}, L"Keluar dari meltycat"},
    {18, "pt", L"Português", L"Idioma",
     {L"Ragdoll", L"Maine Coon", L"Siberiano", L"Gato laranja tigrado", L"Gato preto", L"Gato tigrado castanho", L"Gato preto e branco", L"Uni"}, L"Sair do meltycat"},
    {19, "tr", L"Türkçe", L"Dil", {L"Ragdoll", L"Maine Coon", L"Sibirya kedisi", L"Turuncu tekir", L"Siyah kedi", L"Kahverengi tekir", L"Siyah beyaz kedi", L"Uni"}, L"meltycat’i kapat"},
    {20, "it", L"Italiano", L"Lingua",
     {L"Ragdoll", L"Maine Coon", L"Siberiano", L"Gatto arancione tigrato", L"Gatto nero", L"Gatto soriano marrone", L"Gatto bianco e nero", L"Uni"}, L"Esci da meltycat"},
};

std::filesystem::path ExecutablePath() {
    std::wstring executable(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (size == 0 || size >= executable.size()) return {};
    executable.resize(size);
    return std::filesystem::path(executable);
}

std::string ReadSetting(const std::filesystem::path& path) {
    std::ifstream config(path);
    std::string setting;
    config >> setting;
    return setting;
}

cat::Skin LoadSkin(const std::filesystem::path& path) {
    const auto setting = ReadSetting(path);
    for (const auto& choice : skinChoices) {
        if (setting == choice.setting) return choice.skin;
    }
    return cat::Skin::Black;
}

std::size_t LoadLanguage(const std::filesystem::path& path) {
    const auto setting = ReadSetting(path);
    for (std::size_t i = 0; i < std::size(languageChoices); ++i) {
        if (setting == languageChoices[i].setting) return i;
    }
    return 0;
}

void SaveSetting(const std::filesystem::path& path, const char* setting) {
    if (path.empty()) return;
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream config(temporary, std::ios::binary | std::ios::trunc);
    if (!config) return;
    config << setting << '\n';
    config.close();
    if (!config || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temporary.c_str());
    }
}

} // namespace

TrayMenu::TrayMenu() {
    const auto executable = ExecutablePath();
    if (executable.empty()) return;
    const auto directory = executable.parent_path();
    skinSettings_ = directory / L"meltycat-skin.txt";
    languageSettings_ = directory / L"meltycat-language.txt";
    skin_ = LoadSkin(skinSettings_);
    languageIndex_ = LoadLanguage(languageSettings_);
}

HMENU TrayMenu::Create() const {
    const HMENU menu = CreatePopupMenu();
    if (!menu) return nullptr;
    const auto& language = languageChoices[languageIndex_];
    for (std::size_t i = 0; i < std::size(skinChoices); ++i) {
        const auto& choice = skinChoices[i];
        if (i > 0 && cat::IsLonghair(skinChoices[i - 1].skin) != cat::IsLonghair(choice.skin)) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        }
        const UINT flags = MF_STRING | (choice.skin == skin_ ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(menu, flags, choice.command, language.skinLabels[i]);
    }
    if (const HMENU languages = CreatePopupMenu()) {
        for (const auto& choice : languageChoices) {
            const UINT flags = MF_STRING | (&choice == &language ? MF_CHECKED : MF_UNCHECKED);
            AppendMenuW(languages, flags, choice.command, choice.name);
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(languages), language.menuLabel)) {
            DestroyMenu(languages);
        }
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, exitCommand, language.exitLabel);
    return menu;
}

TrayMenu::Action TrayMenu::Select(UINT command) {
    if (command == exitCommand) return Action::Exit;

    for (std::size_t i = 0; i < std::size(languageChoices); ++i) {
        const auto& choice = languageChoices[i];
        if (choice.command != command) continue;
        if (languageIndex_ != i) {
            languageIndex_ = i;
            SaveSetting(languageSettings_, choice.setting);
        }
        return Action::None;
    }
    for (const auto& choice : skinChoices) {
        if (choice.command != command || choice.skin == skin_) continue;
        skin_ = choice.skin;
        SaveSetting(skinSettings_, choice.setting);
        return Action::SkinChanged;
    }
    return Action::None;
}
