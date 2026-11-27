#include "tray_menu.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace {

constexpr UINT exitCommand = 1;

constexpr UINT changeSkinCommand = 25;
constexpr UINT firstCustomCommand = 1000;

constexpr struct { UINT command; const char* setting; } skinChoices[]{
    {22, "ragdoll"}, {26, "ragdoll-mix"}, {23, "maine-coon"}, {24, "siberian"}, {9, "orange-tabby"},
    {2, "black"}, {21, "li-hua"}, {10, "classic-tuxedo"}, {3, "tuxedo"}
};

constexpr struct { UINT command; const char* setting; PoofMode mode; } poofChoices[]{
    {27, "quiet", PoofMode::Quiet},
    {28, "always", PoofMode::Always},
    {29, "off", PoofMode::Off},
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
    {4, "zh-CN", L"简体中文", L"语言", {L"布偶猫", L"布偶混血", L"缅因猫", L"西伯利亚猫", L"橘虎斑", L"黑猫", L"狸花猫", L"奶牛猫", L"海胆（Uni）"}, L"退出 meltycat"},
    {5, "zh-TW", L"繁體中文", L"語言", {L"布偶貓", L"布偶混血", L"緬因貓", L"西伯利亞貓", L"橘虎斑", L"黑貓", L"狸花貓", L"奶牛貓", L"海膽（Uni）"}, L"結束 meltycat"},
    {6, "en", L"English", L"Language", {L"Ragdoll", L"Ragdoll Mix", L"Maine Coon", L"Siberian", L"Orange Tabby", L"Black", L"Li Hua", L"Tuxedo", L"Uni"}, L"Exit meltycat"},
    {7, "ja", L"日本語", L"言語", {L"ラグドール", L"ラグドールミックス", L"メインクーン", L"サイベリアン", L"茶トラ", L"黒猫", L"キジトラ", L"白黒猫", L"ウニ（Uni）"}, L"meltycat を終了"},
    {8, "ko", L"한국어", L"언어", {L"랙돌", L"랙돌 믹스", L"메인쿤", L"시베리안", L"치즈태비", L"검은 고양이", L"고등어태비", L"흑백 고양이", L"우니 (Uni)"}, L"meltycat 종료"},
    {11, "es", L"Español", L"Idioma",
     {L"Ragdoll", L"Ragdoll mestizo", L"Maine Coon", L"Siberiano", L"Gato atigrado naranja", L"Gato negro", L"Gato atigrado marrón", L"Gato blanco y negro", L"Uni"}, L"Salir de meltycat"},
    {12, "fr", L"Français", L"Langue",
     {L"Ragdoll", L"Ragdoll croisé", L"Maine Coon", L"Sibérien", L"Chat roux tigré", L"Chat noir", L"Chat brun tigré", L"Chat noir et blanc", L"Uni"}, L"Quitter meltycat"},
    {13, "ru", L"Русский язык", L"Язык",
     {L"Рэгдолл", L"Метис рэгдолла", L"Мейн-кун", L"Сибирская кошка", L"Рыжий полосатый кот", L"Чёрный кот", L"Коричневый полосатый кот", L"Чёрно-белый кот", L"Уни (Uni)"}, L"Выйти из meltycat"},
    {14, "th", L"ภาษาไทย", L"ภาษา", {L"แร็กดอลล์", L"แร็กดอลล์ผสม", L"เมนคูน", L"ไซบีเรียน", L"แมวส้มลายเสือ", L"แมวดำ", L"แมวลายสลิด", L"แมวขาวดำ", L"Uni"}, L"ออกจาก meltycat"},
    {15, "vi", L"Tiếng Việt", L"Ngôn ngữ", {L"Mèo Ragdoll", L"Mèo Ragdoll lai", L"Mèo Maine Coon", L"Mèo Siberia", L"Mèo mướp cam", L"Mèo đen", L"Mèo mướp nâu", L"Mèo đen trắng", L"Uni"}, L"Thoát meltycat"},
    {16, "de", L"Deutsch", L"Sprache",
     {L"Ragdoll", L"Ragdoll-Mix", L"Maine Coon", L"Sibirische Katze", L"Orange getigerte Katze", L"Schwarze Katze", L"Braun getigerte Katze", L"Schwarz-weiße Katze", L"Uni"}, L"meltycat beenden"},
    {17, "id", L"Bahasa Indonesia", L"Bahasa",
     {L"Ragdoll", L"Ragdoll campuran", L"Maine Coon", L"Kucing Siberia", L"Kucing jingga belang", L"Kucing hitam", L"Kucing belang cokelat", L"Kucing hitam putih", L"Uni"}, L"Keluar dari meltycat"},
    {18, "pt", L"Português", L"Idioma",
     {L"Ragdoll", L"Ragdoll mestiço", L"Maine Coon", L"Siberiano", L"Gato laranja tigrado", L"Gato preto", L"Gato tigrado castanho", L"Gato preto e branco", L"Uni"}, L"Sair do meltycat"},
    {19, "tr", L"Türkçe", L"Dil", {L"Ragdoll", L"Ragdoll melezi", L"Maine Coon", L"Sibirya kedisi", L"Turuncu tekir", L"Siyah kedi", L"Kahverengi tekir", L"Siyah beyaz kedi", L"Uni"}, L"meltycat’i kapat"},
    {20, "it", L"Italiano", L"Lingua",
     {L"Ragdoll", L"Ragdoll meticcio", L"Maine Coon", L"Siberiano", L"Gatto arancione tigrato", L"Gatto nero", L"Gatto soriano marrone", L"Gatto bianco e nero", L"Uni"}, L"Esci da meltycat"},
};

constexpr SkinText skinTexts[]{
    {L"换皮肤…", L"把皮肤文件拖到这里，或按 Ctrl+V 粘贴皮肤内容", L"选择文件…", L"粘贴",
     L"复制 AI 指南", L"已复制。把指南和猫的照片一起发给 AI，再把它生成的内容导入这里。", L"删除", L"已导入", L"无法导入"},
    {L"換皮膚…", L"把皮膚檔案拖到這裡，或按 Ctrl+V 貼上皮膚內容", L"選擇檔案…", L"貼上",
     L"複製 AI 指南", L"已複製。把指南和貓的照片一起傳給 AI，再把它產生的內容匯入這裡。", L"刪除", L"已匯入", L"無法匯入"},
    {L"Change Skin…", L"Drop a skin file here, or press Ctrl+V to paste skin text", L"Choose File…", L"Paste",
     L"Copy AI Guide", L"Copied. Send the guide and a photo of your cat to an AI, then import what it gives you here.", L"Delete", L"Imported", L"Couldn't import"},
    {L"スキンを変更…", L"スキンファイルをここにドロップするか、Ctrl+V でスキンの内容を貼り付けてください", L"ファイルを選択…", L"貼り付け",
     L"AI ガイドをコピー", L"コピーしました。ガイドと猫の写真を AI に送り、生成された内容をここに読み込んでください。", L"削除", L"読み込みました", L"読み込めません"},
    {L"스킨 바꾸기…", L"스킨 파일을 여기에 끌어 놓거나 Ctrl+V로 스킨 내용을 붙여 넣으세요", L"파일 선택…", L"붙여넣기",
     L"AI 가이드 복사", L"복사했습니다. 가이드와 고양이 사진을 AI에 보내고, 만들어 준 내용을 여기로 가져오세요.", L"삭제", L"가져왔습니다", L"가져올 수 없음"},
    {L"Cambiar skin…", L"Suelta aquí un archivo de skin o pulsa Ctrl+V para pegar su contenido", L"Elegir archivo…", L"Pegar",
     L"Copiar guía para IA", L"Copiado. Envía la guía y una foto de tu gato a una IA e importa aquí lo que te devuelva.", L"Eliminar", L"Importado", L"No se pudo importar"},
    {L"Changer de skin…", L"Déposez un fichier de skin ici, ou appuyez sur Ctrl+V pour coller son contenu", L"Choisir un fichier…", L"Coller",
     L"Copier le guide pour l’IA", L"Copié. Envoyez le guide et une photo de votre chat à une IA, puis importez ici ce qu’elle vous renvoie.", L"Supprimer", L"Importé", L"Impossible d’importer"},
    {L"Сменить скин…", L"Перетащите сюда файл скина или нажмите Ctrl+V, чтобы вставить его содержимое", L"Выбрать файл…", L"Вставить",
     L"Скопировать гайд для ИИ", L"Скопировано. Отправьте гайд и фото кошки ИИ, а затем импортируйте сюда полученный результат.", L"Удалить", L"Импортировано", L"Не удалось импортировать"},
    {L"เปลี่ยนสกิน…", L"ลากไฟล์สกินมาวางที่นี่ หรือกด Ctrl+V เพื่อวางเนื้อหาสกิน", L"เลือกไฟล์…", L"วาง",
     L"คัดลอกคู่มือสำหรับ AI", L"คัดลอกแล้ว ส่งคู่มือพร้อมรูปแมวของคุณให้ AI แล้วนำสิ่งที่ได้มานำเข้าที่นี่", L"ลบ", L"นำเข้าแล้ว", L"นำเข้าไม่ได้"},
    {L"Đổi skin…", L"Kéo tệp skin vào đây hoặc nhấn Ctrl+V để dán nội dung", L"Chọn tệp…", L"Dán",
     L"Sao chép hướng dẫn cho AI", L"Đã sao chép. Gửi hướng dẫn và ảnh mèo của bạn cho AI, rồi nhập kết quả vào đây.", L"Xóa", L"Đã nhập", L"Không thể nhập"},
    {L"Skin ändern…", L"Ziehe eine Skin-Datei hierher oder drücke Ctrl+V, um den Inhalt einzufügen", L"Datei auswählen…", L"Einfügen",
     L"KI-Anleitung kopieren", L"Kopiert. Schicke die Anleitung und ein Foto deiner Katze an eine KI und importiere das Ergebnis hier.", L"Löschen", L"Importiert", L"Import nicht möglich"},
    {L"Ganti skin…", L"Seret file skin ke sini, atau tekan Ctrl+V untuk menempelkan isinya", L"Pilih file…", L"Tempel",
     L"Salin panduan AI", L"Tersalin. Kirim panduan dan foto kucingmu ke AI, lalu impor hasilnya di sini.", L"Hapus", L"Diimpor", L"Tidak bisa mengimpor"},
    {L"Trocar skin…", L"Arraste um arquivo de skin para cá ou pressione Ctrl+V para colar o conteúdo", L"Escolher arquivo…", L"Colar",
     L"Copiar guia para IA", L"Copiado. Envie o guia e uma foto do seu gato para uma IA e importe aqui o que ela gerar.", L"Apagar", L"Importado", L"Não foi possível importar"},
    {L"Skin değiştir…", L"Bir skin dosyasını buraya sürükleyin veya içeriğini yapıştırmak için Ctrl+V’ye basın", L"Dosya seç…", L"Yapıştır",
     L"Yapay zekâ rehberini kopyala", L"Kopyalandı. Rehberi ve kedinizin fotoğrafını bir yapay zekâya gönderin, sonra verdiği sonucu buraya aktarın.", L"Sil", L"İçe aktarıldı", L"İçe aktarılamadı"},
    {L"Cambia skin…", L"Trascina qui un file skin oppure premi Ctrl+V per incollarne il contenuto", L"Scegli file…", L"Incolla",
     L"Copia guida per l’IA", L"Copiato. Invia la guida e una foto del tuo gatto a un’IA, poi importa qui ciò che ti restituisce.", L"Elimina", L"Importato", L"Impossibile importare"},
};

static_assert(std::size(skinTexts) == std::size(languageChoices));

struct PoofText {
    const wchar_t* title;
    const wchar_t* modes[std::size(poofChoices)];
};

constexpr PoofText poofTexts[]{
    {L"出场烟雾", {L"安静一分钟后才有", L"每次都有", L"关闭"}},
    {L"出場煙霧", {L"安靜一分鐘後才有", L"每次都有", L"關閉"}},
    {L"Poof Cloud", {L"After a Quiet Minute", L"Every Time", L"Off"}},
    {L"登場の煙", {L"1分間静かになってから", L"毎回", L"オフ"}},
    {L"등장 연기", {L"1분간 조용한 뒤에만", L"매번", L"끄기"}},
    {L"Nube de aparición", {L"Tras un minuto tranquilo", L"Siempre", L"Desactivada"}},
    {L"Nuage d’apparition", {L"Après une minute calme", L"Toujours", L"Désactivé"}},
    {L"Облачко появления", {L"После минуты затишья", L"Всегда", L"Выключено"}},
    {L"ควันตอนปรากฏตัว", {L"หลังเงียบไป 1 นาที", L"ทุกครั้ง", L"ปิด"}},
    {L"Làn khói xuất hiện", {L"Sau một phút yên tĩnh", L"Mỗi lần", L"Tắt"}},
    {L"Auftrittswolke", {L"Nach einer ruhigen Minute", L"Jedes Mal", L"Aus"}},
    {L"Kepulan asap", {L"Setelah semenit tenang", L"Setiap kali", L"Mati"}},
    {L"Nuvem de entrada", {L"Após um minuto calmo", L"Sempre", L"Desligada"}},
    {L"Belirme bulutu", {L"Sakin bir dakikadan sonra", L"Her seferinde", L"Kapalı"}},
    {L"Nuvoletta d’entrata", {L"Dopo un minuto di calma", L"Ogni volta", L"Disattivata"}},
};

static_assert(std::size(poofTexts) == std::size(languageChoices));

std::string ReadSetting(const std::filesystem::path& path) {
    std::ifstream config(path);
    std::string setting;
    config >> setting;
    return setting;
}

constexpr std::optional<std::size_t> LanguageIndex(std::string_view setting) {
    for (std::size_t i = 0; i < std::size(languageChoices); ++i) {
        if (setting == languageChoices[i].setting) return i;
    }
    return std::nullopt;
}

constexpr auto englishLanguage = *LanguageIndex("en");
constexpr auto simplifiedChinese = *LanguageIndex("zh-CN");
constexpr auto traditionalChinese = *LanguageIndex("zh-TW");

std::size_t PreferredLanguage(const wchar_t* languages) {
    for (const wchar_t* tag = languages; *tag; tag += std::char_traits<wchar_t>::length(tag) + 1) {
        std::wstring identifier(tag);
        std::transform(identifier.begin(), identifier.end(), identifier.begin(), [](wchar_t ch) -> wchar_t {
            return (ch >= L'A' && ch <= L'Z') ? static_cast<wchar_t>(ch + (L'a' - L'A')) : ch;
        });
        std::wstring_view parts(identifier);
        const auto code = parts.substr(0, parts.find(L'-'));
        if (code == L"zh") {
            auto separator = parts.find(L'-');
            while (separator != std::wstring_view::npos) {
                parts.remove_prefix(separator + 1);
                separator = parts.find(L'-');
                const auto part = parts.substr(0, separator);
                if (part == L"hant" || part == L"tw" || part == L"hk" || part == L"mo") {
                    return traditionalChinese;
                }
            }
            return simplifiedChinese;
        }
        std::string setting;
        for (const auto ch : code) {
            if (ch < L'a' || ch > L'z') break;
            setting.push_back(static_cast<char>(ch));
        }
        if (setting.size() == code.size()) {
            if (const auto index = LanguageIndex(setting)) return *index;
        }
    }
    return englishLanguage;
}

std::size_t LoadLanguage(const std::filesystem::path& path) {
    if (const auto index = LanguageIndex(ReadSetting(path))) return *index;
    ULONG count = 0, size = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &size) || size == 0) {
        return englishLanguage;
    }
    std::wstring languages(size, L'\0');
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, languages.data(), &size)) {
        return englishLanguage;
    }
    return PreferredLanguage(languages.c_str());
}

void SaveSetting(const std::filesystem::path& path, std::string_view setting) {
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

TrayMenu::TrayMenu(const SkinLibrary& library) : library_(library) {
    const auto executable = ExecutablePath();
    if (!executable.empty()) {
        const auto directory = executable.parent_path();
        skinSettings_ = directory / L"meltycat-skin.txt";
        languageSettings_ = directory / L"meltycat-language.txt";
        poofSettings_ = directory / L"meltycat-poof.txt";
    }
    skinId_ = ReadSetting(skinSettings_);
    skin_ = library_.Find(skinId_);
    if (!skin_) {
        skinId_ = "black";
        skin_ = library_.Find(skinId_);
    }
    languageIndex_ = LoadLanguage(languageSettings_);
    const auto poofSetting = ReadSetting(poofSettings_);
    for (const auto& choice : poofChoices) {
        if (poofSetting == choice.setting) {
            poofMode_ = choice.mode;
            break;
        }
    }
}

const SkinText& TrayMenu::Text() const { return skinTexts[languageIndex_]; }

bool TrayMenu::SetSkin(std::string_view id) {
    if (id == skinId_) return false;
    const auto selected = library_.Find(id);
    if (!selected) return false;
    skinId_ = id;
    skin_ = selected;
    SaveSetting(skinSettings_, skinId_);
    return true;
}

HMENU TrayMenu::Create() const {
    const HMENU menu = CreatePopupMenu();
    if (!menu) return nullptr;
    const auto& language = languageChoices[languageIndex_];
    bool wasLonghair = true;
    for (std::size_t i = 0; i < std::size(skinChoices); ++i) {
        const auto& choice = skinChoices[i];
        const bool longhair = library_.Find(choice.setting)->longhair;
        if (i > 0 && wasLonghair != longhair) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        wasLonghair = longhair;
        const UINT flags = MF_STRING | (skinId_ == choice.setting ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(menu, flags, choice.command, language.skinLabels[i]);
    }
    const auto& custom = library_.CustomSkins();
    if (!custom.empty()) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    for (std::size_t i = 0; i < custom.size(); ++i) {
        const auto& choice = custom[i];
        const UINT flags = MF_STRING | (skinId_ == choice.id ? MF_CHECKED : MF_UNCHECKED);
        std::wstring label = choice.skin->name;
        for (std::size_t pos = label.find(L'&'); pos != std::wstring::npos; pos = label.find(L'&', pos + 2)) {
            label.insert(pos, 1, L'&');
        }
        AppendMenuW(menu, flags, firstCustomCommand + static_cast<UINT>(i), label.c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, changeSkinCommand, Text().title);
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
    if (const HMENU poof = CreatePopupMenu()) {
        const auto& labels = poofTexts[languageIndex_];
        for (std::size_t i = 0; i < std::size(poofChoices); ++i) {
            const auto& choice = poofChoices[i];
            const UINT flags = MF_STRING | (poofMode_ == choice.mode ? MF_CHECKED : MF_UNCHECKED);
            AppendMenuW(poof, flags, choice.command, labels.modes[i]);
        }
        if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(poof), labels.title)) {
            DestroyMenu(poof);
        }
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, exitCommand, language.exitLabel);
    return menu;
}

TrayMenu::Action TrayMenu::Select(UINT command) {
    if (command == exitCommand) return Action::Exit;
    if (command == changeSkinCommand) return Action::ChangeSkin;

    for (std::size_t i = 0; i < std::size(languageChoices); ++i) {
        const auto& choice = languageChoices[i];
        if (choice.command != command) continue;
        languageIndex_ = i;
        SaveSetting(languageSettings_, choice.setting);
        return Action::None;
    }
    for (const auto& choice : poofChoices) {
        if (choice.command != command) continue;
        poofMode_ = choice.mode;
        SaveSetting(poofSettings_, choice.setting);
        return Action::None;
    }
    for (const auto& choice : skinChoices) {
        if (choice.command == command && SetSkin(choice.setting)) return Action::SkinChanged;
    }
    const auto& custom = library_.CustomSkins();
    if (command >= firstCustomCommand && command - firstCustomCommand < custom.size() &&
        SetSkin(custom[command - firstCustomCommand].id)) return Action::SkinChanged;
    return Action::None;
}
