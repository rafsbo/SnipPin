#include "platform/Localization.h"

#include <algorithm>
#include <cwctype>
#include <map>

#include "i18n/Catalog.h"
#include "platform/Log.h"
#include "platform/WinUtil.h"

namespace snippin::l10n {
namespace {

constexpr wchar_t kResourceType[] = L"SNIPPINLANG";

std::wstring Lower(std::wstring s) {
    for (auto& c : s) {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    return s;
}

std::wstring Widen(const std::string& s) {
    std::wstring out;
    i18n::DecodeUtf8(s, out);
    return out;
}

// Explicit CALLBACK function: lambdas do not convert to __stdcall on x86 GCC.
BOOL CALLBACK CollectResourceProc(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR param) {
    auto* out = reinterpret_cast<std::map<std::wstring, std::string>*>(param);
    if (IS_INTRESOURCE(name)) {
        return TRUE;
    }
    HRSRC res = ::FindResourceW(module, name, type);
    HGLOBAL handle = res ? ::LoadResource(module, res) : nullptr;
    const void* data = handle ? ::LockResource(handle) : nullptr;
    const DWORD size = res ? ::SizeofResource(module, res) : 0;
    if (data && size) {
        (*out)[Lower(name)] = std::string(static_cast<const char*>(data), size);
    }
    return TRUE;
}

// Raw UTF-8 text of every language, keyed by lower-case code. External
// files override embedded ones with the same code.
std::map<std::wstring, std::string> CollectSources(HINSTANCE instance) {
    std::map<std::wstring, std::string> sources;

    ::EnumResourceNamesW(instance, kResourceType, &CollectResourceProc, reinterpret_cast<LONG_PTR>(&sources));

    // Community translations: <exe dir>\lang\xx.lang (no rebuild needed).
    const std::wstring dir = win::GetModuleDirectory() + L"\\lang\\";
    WIN32_FIND_DATAW find{};
    HANDLE h = ::FindFirstFileW((dir + L"*.lang").c_str(), &find);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring file = find.cFileName;
            HANDLE f = ::CreateFileW((dir + file).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f == INVALID_HANDLE_VALUE) {
                continue;
            }
            const DWORD size = ::GetFileSize(f, nullptr);
            std::string text;
            if (size != INVALID_FILE_SIZE && size < (1u << 20)) {
                text.resize(size);
                DWORD read = 0;
                if (!::ReadFile(f, text.data(), size, &read, nullptr)) {
                    text.clear();
                }
                text.resize(read);
            }
            ::CloseHandle(f);
            const size_t dot = file.rfind(L'.');
            if (!text.empty() && dot != std::wstring::npos) {
                sources[Lower(file.substr(0, dot))] = std::move(text);
            }
        } while (::FindNextFileW(h, &find));
        ::FindClose(h);
    }
    return sources;
}

// PRIMARYLANGID -> language code for the official EU languages.
std::wstring CodeFromLangId(LANGID langId) {
    switch (PRIMARYLANGID(langId)) {
        case LANG_BULGARIAN: return L"bg";
        case LANG_CZECH: return L"cs";
        case LANG_DANISH: return L"da";
        case LANG_GERMAN: return L"de";
        case LANG_GREEK: return L"el";
        case LANG_ENGLISH: return L"en";
        case LANG_SPANISH: return L"es";
        case LANG_ESTONIAN: return L"et";
        case LANG_FINNISH: return L"fi";
        case LANG_FRENCH: return L"fr";
        case LANG_IRISH: return L"ga";
        case LANG_HUNGARIAN: return L"hu";
        case LANG_ITALIAN: return L"it";
        case LANG_LITHUANIAN: return L"lt";
        case LANG_LATVIAN: return L"lv";
        case LANG_MALTESE: return L"mt";
        case LANG_DUTCH: return L"nl";
        case LANG_POLISH: return L"pl";
        case LANG_PORTUGUESE: return L"pt";
        case LANG_ROMANIAN: return L"ro";
        case LANG_SLOVAK: return L"sk";
        case LANG_SLOVENIAN: return L"sl";
        case LANG_SWEDISH: return L"sv";
        case LANG_CROATIAN:  // shared with Serbian/Bosnian: only Croatian sublanguages
            if (SUBLANGID(langId) == SUBLANG_CROATIAN_CROATIA ||
                SUBLANGID(langId) == SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN) {
                return L"hr";
            }
            return L"";
        default: return L"";
    }
}

}  // namespace

std::vector<LanguageInfo> AvailableLanguages(HINSTANCE instance) {
    std::vector<LanguageInfo> languages;
    languages.push_back(LanguageInfo{L"en", L"English", L"English", false});
    const std::wstring externalDir = win::GetModuleDirectory() + L"\\lang\\";
    for (const auto& [code, text] : CollectSources(instance)) {
        if (code == L"en") {
            continue;
        }
        const i18n::ParsedCatalog catalog = i18n::ParseCatalog(text);
        LanguageInfo info;
        info.code = code;
        info.nativeName = catalog.nativeName.empty() ? code : catalog.nativeName;
        info.englishName = catalog.englishName;
        info.external = ::GetFileAttributesW((externalDir + code + L".lang").c_str()) != INVALID_FILE_ATTRIBUTES;
        languages.push_back(std::move(info));
    }
    return languages;
}

std::wstring DetectSystemLanguage(HINSTANCE instance) {
    const std::wstring code = CodeFromLangId(::GetUserDefaultUILanguage());
    if (code.empty()) {
        return L"en";
    }
    for (const auto& lang : AvailableLanguages(instance)) {
        if (lang.code == code) {
            return code;
        }
    }
    return L"en";
}

std::wstring ApplyLanguage(HINSTANCE instance, const std::wstring& setting) {
    std::wstring code = Lower(setting);
    if (code.empty() || code == L"auto") {
        code = DetectSystemLanguage(instance);
    }
    if (code == L"en") {
        i18n::ResetToEnglish();
        return code;
    }
    const auto sources = CollectSources(instance);
    const auto it = sources.find(code);
    if (it == sources.end()) {
        SNIPPIN_LOG(L"Language '%s' not available, using English", code.c_str());
        i18n::ResetToEnglish();
        return L"en";
    }
    i18n::ParsedCatalog catalog = i18n::ParseCatalog(it->second);
    for (const auto& error : catalog.errors) {
        SNIPPIN_LOG(L"%s.lang: %s", code.c_str(), Widen(error).c_str());
    }
    if (catalog.code.empty()) {
        catalog.code = i18n::EncodeUtf8(code);
    }
    const int translated = i18n::SetActiveCatalog(catalog);
    SNIPPIN_LOG(L"Language '%s' active: %d/%d strings translated", code.c_str(), translated, i18n::kStringCount);
    return code;
}

}  // namespace snippin::l10n
