import Foundation

// Strings copied verbatim from tray_menu.cpp; skin names follow Skin.menuOrder.
struct Language: Equatable {
    var setting: String
    var name: String
    var menuLabel: String
    var skinNames: [String]
    var quit: String

    init?(setting: String) {
        guard let language = Language.all.first(where: { $0.setting == setting }) else { return nil }
        self = language
    }

    private init(_ setting: String, _ name: String, _ menuLabel: String, _ skinNames: [String], _ quit: String) {
        self.setting = setting
        self.name = name
        self.menuLabel = menuLabel
        self.skinNames = skinNames
        self.quit = quit
    }

    // First supported entry of the system's preferred languages, e.g. "zh-Hant-HK" or "pt-BR".
    static func system(preferred: [String] = Locale.preferredLanguages) -> Language {
        for identifier in preferred {
            let parts = identifier.split(separator: "-").map(String.init)
            guard let code = parts.first else { continue }
            let setting: String
            if code == "zh" {
                let traditional = parts.contains("Hant") || parts.contains("TW") || parts.contains("HK") || parts.contains("MO")
                setting = traditional ? "zh-TW" : "zh-CN"
            } else {
                setting = code
            }
            if let language = Language(setting: setting) { return language }
        }
        return Language(setting: "en")!
    }

    static let all: [Language] = [
        Language("zh-CN", "简体中文", "语言",
                 ["布偶猫", "布偶混血", "缅因猫", "西伯利亚猫", "橘虎斑", "黑猫", "狸花猫", "奶牛猫", "海胆（Uni）"],
                 "退出 meltycat"),
        Language("zh-TW", "繁體中文", "語言",
                 ["布偶貓", "布偶混血", "緬因貓", "西伯利亞貓", "橘虎斑", "黑貓", "狸花貓", "奶牛貓", "海膽（Uni）"],
                 "結束 meltycat"),
        Language("en", "English", "Language",
                 ["Ragdoll", "Ragdoll Mix", "Maine Coon", "Siberian", "Orange Tabby", "Black Cat", "Li Hua", "Tuxedo Cat", "Uni"],
                 "Exit meltycat"),
        Language("ja", "日本語", "言語",
                 ["ラグドール", "ラグドールミックス", "メインクーン", "サイベリアン", "茶トラ", "黒猫", "キジトラ", "白黒猫", "ウニ（Uni）"],
                 "meltycat を終了"),
        Language("ko", "한국어", "언어",
                 ["랙돌", "랙돌 믹스", "메인쿤", "시베리안", "치즈태비", "검은 고양이", "고등어태비", "흑백 고양이", "우니 (Uni)"],
                 "meltycat 종료"),
        Language("es", "Español", "Idioma",
                 ["Ragdoll", "Ragdoll mestizo", "Maine Coon", "Siberiano", "Gato atigrado naranja", "Gato negro", "Gato atigrado marrón", "Gato blanco y negro", "Uni"],
                 "Salir de meltycat"),
        Language("fr", "Français", "Langue",
                 ["Ragdoll", "Ragdoll croisé", "Maine Coon", "Sibérien", "Chat roux tigré", "Chat noir", "Chat brun tigré", "Chat noir et blanc", "Uni"],
                 "Quitter meltycat"),
        Language("ru", "Русский язык", "Язык",
                 ["Рэгдолл", "Метис рэгдолла", "Мейн-кун", "Сибирская кошка", "Рыжий полосатый кот", "Чёрный кот", "Коричневый полосатый кот", "Чёрно-белый кот", "Уни (Uni)"],
                 "Выйти из meltycat"),
        Language("th", "ภาษาไทย", "ภาษา",
                 ["แร็กดอลล์", "แร็กดอลล์ผสม", "เมนคูน", "ไซบีเรียน", "แมวส้มลายเสือ", "แมวดำ", "แมวลายสลิด", "แมวขาวดำ", "Uni"],
                 "ออกจาก meltycat"),
        Language("vi", "Tiếng Việt", "Ngôn ngữ",
                 ["Mèo Ragdoll", "Mèo Ragdoll lai", "Mèo Maine Coon", "Mèo Siberia", "Mèo mướp cam", "Mèo đen", "Mèo mướp nâu", "Mèo đen trắng", "Uni"],
                 "Thoát meltycat"),
        Language("de", "Deutsch", "Sprache",
                 ["Ragdoll", "Ragdoll-Mix", "Maine Coon", "Sibirische Katze", "Orange getigerte Katze", "Schwarze Katze", "Braun getigerte Katze", "Schwarz-weiße Katze", "Uni"],
                 "meltycat beenden"),
        Language("id", "Bahasa Indonesia", "Bahasa",
                 ["Ragdoll", "Ragdoll campuran", "Maine Coon", "Kucing Siberia", "Kucing jingga belang", "Kucing hitam", "Kucing belang cokelat", "Kucing hitam putih", "Uni"],
                 "Keluar dari meltycat"),
        Language("pt", "Português", "Idioma",
                 ["Ragdoll", "Ragdoll mestiço", "Maine Coon", "Siberiano", "Gato laranja tigrado", "Gato preto", "Gato tigrado castanho", "Gato preto e branco", "Uni"],
                 "Sair do meltycat"),
        Language("tr", "Türkçe", "Dil",
                 ["Ragdoll", "Ragdoll melezi", "Maine Coon", "Sibirya kedisi", "Turuncu tekir", "Siyah kedi", "Kahverengi tekir", "Siyah beyaz kedi", "Uni"],
                 "meltycat’i kapat"),
        Language("it", "Italiano", "Lingua",
                 ["Ragdoll", "Ragdoll meticcio", "Maine Coon", "Siberiano", "Gatto arancione tigrato", "Gatto nero", "Gatto soriano marrone", "Gatto bianco e nero", "Uni"],
                 "Esci da meltycat"),
    ]
}

// Strings for the skin window and the custom skins in the menu.
struct SkinText {
    var changeSkin: String
    var dropHint: String
    var chooseFile: String
    var paste: String
    var copyGuide: String
    var guideCopied: String
    var delete: String
    var imported: String
    var importFailed: String
    var custom: String
}

extension Language {
    var skinText: SkinText { Self.skinTexts[setting] ?? Self.skinTexts["en"]! }
    var colon: String { ["zh-CN", "zh-TW", "ja"].contains(setting) ? "：" : ": " }

    private static let skinTexts: [String: SkinText] = [
        "zh-CN": SkinText(changeSkin: "换皮肤…", dropHint: "把皮肤文件拖到这里，或按 ⌘V 粘贴皮肤内容", chooseFile: "选择文件…",
                          paste: "粘贴", copyGuide: "复制 AI 指南",
                          guideCopied: "已复制。把指南和猫的照片一起发给 AI，再把它生成的内容导入这里。",
                          delete: "删除", imported: "已导入", importFailed: "无法导入", custom: "自定义"),
        "zh-TW": SkinText(changeSkin: "換皮膚…", dropHint: "把皮膚檔案拖到這裡，或按 ⌘V 貼上皮膚內容", chooseFile: "選擇檔案…",
                          paste: "貼上", copyGuide: "複製 AI 指南",
                          guideCopied: "已複製。把指南和貓的照片一起傳給 AI，再把它產生的內容匯入這裡。",
                          delete: "刪除", imported: "已匯入", importFailed: "無法匯入", custom: "自訂"),
        "en": SkinText(changeSkin: "Change Skin…", dropHint: "Drop a skin file here, or press ⌘V to paste skin text",
                       chooseFile: "Choose File…", paste: "Paste", copyGuide: "Copy AI Guide",
                       guideCopied: "Copied. Send the guide and a photo of your cat to an AI, then import what it gives you here.",
                       delete: "Delete", imported: "Imported", importFailed: "Couldn't import", custom: "Custom"),
        "ja": SkinText(changeSkin: "スキンを変更…", dropHint: "スキンファイルをここにドロップするか、⌘V でスキンの内容を貼り付けてください",
                       chooseFile: "ファイルを選択…", paste: "貼り付け", copyGuide: "AI ガイドをコピー",
                       guideCopied: "コピーしました。ガイドと猫の写真を AI に送り、生成された内容をここに読み込んでください。",
                       delete: "削除", imported: "読み込みました", importFailed: "読み込めません", custom: "カスタム"),
        "ko": SkinText(changeSkin: "스킨 바꾸기…", dropHint: "스킨 파일을 여기에 끌어 놓거나 ⌘V로 스킨 내용을 붙여 넣으세요",
                       chooseFile: "파일 선택…", paste: "붙여넣기", copyGuide: "AI 가이드 복사",
                       guideCopied: "복사했습니다. 가이드와 고양이 사진을 AI에 보내고, 만들어 준 내용을 여기로 가져오세요.",
                       delete: "삭제", imported: "가져왔습니다", importFailed: "가져올 수 없음", custom: "사용자 지정"),
        "es": SkinText(changeSkin: "Cambiar skin…", dropHint: "Suelta aquí un archivo de skin o pulsa ⌘V para pegar su contenido",
                       chooseFile: "Elegir archivo…", paste: "Pegar", copyGuide: "Copiar guía para IA",
                       guideCopied: "Copiado. Envía la guía y una foto de tu gato a una IA e importa aquí lo que te devuelva.",
                       delete: "Eliminar", imported: "Importado", importFailed: "No se pudo importar", custom: "Personalizados"),
        "fr": SkinText(changeSkin: "Changer de skin…", dropHint: "Déposez un fichier de skin ici, ou appuyez sur ⌘V pour coller son contenu",
                       chooseFile: "Choisir un fichier…", paste: "Coller", copyGuide: "Copier le guide pour l’IA",
                       guideCopied: "Copié. Envoyez le guide et une photo de votre chat à une IA, puis importez ici ce qu’elle vous renvoie.",
                       delete: "Supprimer", imported: "Importé", importFailed: "Impossible d’importer", custom: "Personnalisés"),
        "ru": SkinText(changeSkin: "Сменить скин…", dropHint: "Перетащите сюда файл скина или нажмите ⌘V, чтобы вставить его содержимое",
                       chooseFile: "Выбрать файл…", paste: "Вставить", copyGuide: "Скопировать гайд для ИИ",
                       guideCopied: "Скопировано. Отправьте гайд и фото кошки ИИ, а затем импортируйте сюда полученный результат.",
                       delete: "Удалить", imported: "Импортировано", importFailed: "Не удалось импортировать", custom: "Свои"),
        "th": SkinText(changeSkin: "เปลี่ยนสกิน…", dropHint: "ลากไฟล์สกินมาวางที่นี่ หรือกด ⌘V เพื่อวางเนื้อหาสกิน",
                       chooseFile: "เลือกไฟล์…", paste: "วาง", copyGuide: "คัดลอกคู่มือสำหรับ AI",
                       guideCopied: "คัดลอกแล้ว ส่งคู่มือพร้อมรูปแมวของคุณให้ AI แล้วนำสิ่งที่ได้มานำเข้าที่นี่",
                       delete: "ลบ", imported: "นำเข้าแล้ว", importFailed: "นำเข้าไม่ได้", custom: "กำหนดเอง"),
        "vi": SkinText(changeSkin: "Đổi skin…", dropHint: "Kéo tệp skin vào đây hoặc nhấn ⌘V để dán nội dung",
                       chooseFile: "Chọn tệp…", paste: "Dán", copyGuide: "Sao chép hướng dẫn cho AI",
                       guideCopied: "Đã sao chép. Gửi hướng dẫn và ảnh mèo của bạn cho AI, rồi nhập kết quả vào đây.",
                       delete: "Xóa", imported: "Đã nhập", importFailed: "Không thể nhập", custom: "Tùy chỉnh"),
        "de": SkinText(changeSkin: "Skin ändern…", dropHint: "Ziehe eine Skin-Datei hierher oder drücke ⌘V, um den Inhalt einzufügen",
                       chooseFile: "Datei auswählen…", paste: "Einfügen", copyGuide: "KI-Anleitung kopieren",
                       guideCopied: "Kopiert. Schicke die Anleitung und ein Foto deiner Katze an eine KI und importiere das Ergebnis hier.",
                       delete: "Löschen", imported: "Importiert", importFailed: "Import nicht möglich", custom: "Eigene"),
        "id": SkinText(changeSkin: "Ganti skin…", dropHint: "Seret file skin ke sini, atau tekan ⌘V untuk menempelkan isinya",
                       chooseFile: "Pilih file…", paste: "Tempel", copyGuide: "Salin panduan AI",
                       guideCopied: "Tersalin. Kirim panduan dan foto kucingmu ke AI, lalu impor hasilnya di sini.",
                       delete: "Hapus", imported: "Diimpor", importFailed: "Tidak bisa mengimpor", custom: "Kustom"),
        "pt": SkinText(changeSkin: "Trocar skin…", dropHint: "Arraste um arquivo de skin para cá ou pressione ⌘V para colar o conteúdo",
                       chooseFile: "Escolher arquivo…", paste: "Colar", copyGuide: "Copiar guia para IA",
                       guideCopied: "Copiado. Envie o guia e uma foto do seu gato para uma IA e importe aqui o que ela gerar.",
                       delete: "Apagar", imported: "Importado", importFailed: "Não foi possível importar", custom: "Personalizados"),
        "tr": SkinText(changeSkin: "Skin değiştir…", dropHint: "Bir skin dosyasını buraya sürükleyin veya içeriğini yapıştırmak için ⌘V’ye basın",
                       chooseFile: "Dosya seç…", paste: "Yapıştır", copyGuide: "Yapay zekâ rehberini kopyala",
                       guideCopied: "Kopyalandı. Rehberi ve kedinizin fotoğrafını bir yapay zekâya gönderin, sonra verdiği sonucu buraya aktarın.",
                       delete: "Sil", imported: "İçe aktarıldı", importFailed: "İçe aktarılamadı", custom: "Özel"),
        "it": SkinText(changeSkin: "Cambia skin…", dropHint: "Trascina qui un file skin oppure premi ⌘V per incollarne il contenuto",
                       chooseFile: "Scegli file…", paste: "Incolla", copyGuide: "Copia guida per l’IA",
                       guideCopied: "Copiato. Invia la guida e una foto del tuo gatto a un’IA, poi importa qui ciò che ti restituisce.",
                       delete: "Elimina", imported: "Importato", importFailed: "Impossibile importare", custom: "Personalizzati"),
    ]
}
