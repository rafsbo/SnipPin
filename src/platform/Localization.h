// Windows side of localization: finds the available languages (embedded in
// the executable as "SNIPPINLANG" resources, plus optional community files in
// a "lang" folder next to the executable), detects the Windows display
// language and activates a catalogue.
#pragma once

#include <string>
#include <vector>

#include "platform/Win32.h"

namespace snippin::l10n {

struct LanguageInfo {
    std::wstring code;         // "it"
    std::wstring nativeName;   // "Italiano"
    std::wstring englishName;  // "Italian"
    bool external = false;     // loaded from <exe dir>\lang\<code>.lang
};

// Built-in English first, then the other languages sorted by code.
std::vector<LanguageInfo> AvailableLanguages(HINSTANCE instance);

// Two-letter code of the Windows display language ("de"), or "en" when it
// is not one of the supported languages.
std::wstring DetectSystemLanguage(HINSTANCE instance);

// Activates |setting| ("auto" or a language code) and returns the code that
// is now active. Unknown or broken languages fall back to English.
std::wstring ApplyLanguage(HINSTANCE instance, const std::wstring& setting);

}  // namespace snippin::l10n
