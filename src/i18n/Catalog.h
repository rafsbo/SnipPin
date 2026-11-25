// Translation catalogues: parsing of res/lang/*.lang files, the active
// language, and safe placeholder formatting. Platform independent.
//
// File format (UTF-8, optional BOM):
//   # comment
//   language.code = it
//   language.name = Italiano
//   menu.exit = E&sci
// Escapes in values: \n (new line), \t (tab), \\ (backslash).
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "i18n/StringTable.h"

namespace snippin::i18n {

struct ParsedCatalog {
    std::string code;            // "it" (from language.code)
    std::wstring nativeName;     // "Italiano" (from language.name)
    std::wstring englishName;    // "Italian" (from language.name_en)
    std::map<std::string, std::wstring> entries;  // translation key -> text
    std::vector<std::string> errors;              // "line 12: ..." diagnostics
    std::vector<std::string> unknownKeys;         // keys not in the string table
};

// Parses a catalogue. Never throws; problems are reported in |errors|.
ParsedCatalog ParseCatalog(std::string_view utf8Text);

// UTF-8 -> wide string (UTF-16 on Windows, UTF-32 elsewhere). Invalid
// sequences become U+FFFD. Returns false if any were found.
bool DecodeUtf8(std::string_view utf8, std::wstring& out);
std::string EncodeUtf8(std::wstring_view text);

// Placeholder signature of a string: the sequence of %s / %d conversions,
// e.g. L"Snippet %d of %s" -> "ds". "%%" and a lone "%" are ignored.
std::string PlaceholderSignature(std::wstring_view text);

// Replaces %s and %d (in order) with |args|; "%%" becomes "%". Missing
// arguments leave the placeholder text untouched. Never reads past |args|,
// so a bad translation cannot crash the program (unlike printf).
std::wstring Format(std::wstring_view pattern, const std::vector<std::wstring>& args);

// Number of menu accelerator markers ('&' not followed by '&').
int CountAccelerators(std::wstring_view text);

// Installs |catalog| as the active language. Entries whose placeholders do
// not match English are ignored (English is used for them). Returns the
// number of strings that ended up translated.
int SetActiveCatalog(const ParsedCatalog& catalog);
void ResetToEnglish();
const std::string& ActiveLanguageCode();  // "en" when no catalogue is active

// Convenience: Format(Tr(id), args).
std::wstring TrFormat(StringId id, const std::vector<std::wstring>& args);

}  // namespace snippin::i18n
