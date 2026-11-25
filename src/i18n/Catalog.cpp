#include "i18n/Catalog.h"

#include <array>
#include <cstdint>

namespace snippin::i18n {
namespace {

struct Entry {
    const char* key;
    const wchar_t* english;
};

constexpr Entry kEntries[] = {
#define SNIPPIN_STRING_ENTRY(id, key, english) {key, english},
    SNIPPIN_STRING_TABLE(SNIPPIN_STRING_ENTRY)
#undef SNIPPIN_STRING_ENTRY
};
static_assert(sizeof(kEntries) / sizeof(kEntries[0]) == static_cast<size_t>(kStringCount),
              "string table and enum are out of sync");

struct ActiveState {
    std::array<std::wstring, kStringCount> text;
    std::array<bool, kStringCount> translated{};
    std::string code = "en";
};

ActiveState& State() {
    static ActiveState state;
    return state;
}

void AppendCodePoint(std::wstring& out, std::uint32_t cp) {
    if constexpr (sizeof(wchar_t) == 2) {
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out += static_cast<wchar_t>(0xD800 + (cp >> 10));
            out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
            return;
        }
    }
    out += static_cast<wchar_t>(cp);
}

std::string_view TrimView(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

bool IsValidKey(std::string_view key) {
    if (key.empty()) {
        return false;
    }
    for (char c : key) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string Unescape(std::string_view value, bool& badEscape) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 == value.size()) {
            out += value[i];
            continue;
        }
        const char next = value[++i];
        switch (next) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case '\\': out += '\\'; break;
            default:
                badEscape = true;
                out += '\\';
                out += next;
                break;
        }
    }
    return out;
}

}  // namespace

// ------------------------------------------------------------------ table

const char* KeyOf(StringId id) {
    const int i = static_cast<int>(id);
    return (i >= 0 && i < kStringCount) ? kEntries[i].key : "";
}

const wchar_t* EnglishOf(StringId id) {
    const int i = static_cast<int>(id);
    return (i >= 0 && i < kStringCount) ? kEntries[i].english : L"";
}

bool FindStringId(std::string_view key, StringId& out) {
    for (int i = 0; i < kStringCount; ++i) {
        if (key == kEntries[i].key) {
            out = static_cast<StringId>(i);
            return true;
        }
    }
    return false;
}

const wchar_t* Tr(StringId id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= kStringCount) {
        return L"";
    }
    const ActiveState& s = State();
    return s.translated[static_cast<size_t>(i)] ? s.text[static_cast<size_t>(i)].c_str() : kEntries[i].english;
}

// ------------------------------------------------------------------ UTF-8

bool DecodeUtf8(std::string_view in, std::wstring& out) {
    out.clear();
    out.reserve(in.size());
    bool valid = true;
    size_t i = 0;
    while (i < in.size()) {
        const auto b0 = static_cast<unsigned char>(in[i]);
        std::uint32_t cp = 0;
        int extra = 0;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            cp = b0 & 0x1F;
            extra = 1;
        } else if ((b0 & 0xF0) == 0xE0) {
            cp = b0 & 0x0F;
            extra = 2;
        } else if ((b0 & 0xF8) == 0xF0) {
            cp = b0 & 0x07;
            extra = 3;
        } else {
            out += static_cast<wchar_t>(0xFFFD);
            valid = false;
            ++i;
            continue;
        }
        if (i + static_cast<size_t>(extra) >= in.size()) {  // truncated sequence at the end
            out += static_cast<wchar_t>(0xFFFD);
            valid = false;
            break;
        }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            const auto b = static_cast<unsigned char>(in[i + static_cast<size_t>(k)]);
            if ((b & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (b & 0x3F);
        }
        // Reject overlong forms, surrogates and out-of-range values.
        static const std::uint32_t kMin[] = {0, 0x80, 0x800, 0x10000};
        if (!ok || cp < kMin[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            out += static_cast<wchar_t>(0xFFFD);
            valid = false;
            ++i;
            continue;
        }
        AppendCodePoint(out, cp);
        i += static_cast<size_t>(extra) + 1;
    }
    return valid;
}

std::string EncodeUtf8(std::wstring_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(text[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size()) {
                const auto lo = static_cast<std::uint32_t>(text[i + 1]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    ++i;
                }
            }
        }
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// ------------------------------------------------------------------ parsing

ParsedCatalog ParseCatalog(std::string_view text) {
    ParsedCatalog result;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    int lineNumber = 0;
    while (!text.empty()) {
        ++lineNumber;
        const size_t eol = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text = eol == std::string_view::npos ? std::string_view() : text.substr(eol + 1);

        line = TrimView(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        const std::string where = "line " + std::to_string(lineNumber) + ": ";
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            result.errors.push_back(where + "missing '='");
            continue;
        }
        const std::string key(TrimView(line.substr(0, eq)));
        if (!IsValidKey(key)) {
            result.errors.push_back(where + "invalid key '" + key + "'");
            continue;
        }
        bool badEscape = false;
        const std::string raw = Unescape(TrimView(line.substr(eq + 1)), badEscape);
        if (badEscape) {
            result.errors.push_back(where + "unknown escape sequence in '" + key + "'");
        }
        std::wstring value;
        if (!DecodeUtf8(raw, value)) {
            result.errors.push_back(where + "invalid UTF-8 in '" + key + "'");
        }
        if (result.entries.count(key) || (key == "language.code" && !result.code.empty())) {
            result.errors.push_back(where + "duplicate key '" + key + "'");
        }

        if (key == "language.code") {
            result.code = raw;
        } else if (key == "language.name") {
            result.nativeName = value;
        } else if (key == "language.name_en") {
            result.englishName = value;
        } else {
            StringId id;
            if (!FindStringId(key, id)) {
                result.unknownKeys.push_back(key);
            }
            if (value.empty()) {
                result.errors.push_back(where + "empty value for '" + key + "'");
                continue;
            }
            result.entries[key] = std::move(value);
        }
    }
    if (result.code.empty()) {
        result.errors.push_back("missing language.code");
    }
    return result;
}

// ------------------------------------------------------------------ formatting

std::string PlaceholderSignature(std::wstring_view text) {
    std::string sig;
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        if (text[i] != L'%') {
            continue;
        }
        const wchar_t next = text[i + 1];
        if (next == L'%') {
            ++i;
        } else if (next == L's' || next == L'd') {
            sig += static_cast<char>(next);
            ++i;
        }
    }
    return sig;
}

std::wstring Format(std::wstring_view pattern, const std::vector<std::wstring>& args) {
    std::wstring out;
    out.reserve(pattern.size() + 32);
    size_t next = 0;
    for (size_t i = 0; i < pattern.size(); ++i) {
        const wchar_t c = pattern[i];
        if (c == L'%' && i + 1 < pattern.size()) {
            const wchar_t spec = pattern[i + 1];
            if (spec == L'%') {
                out += L'%';
                ++i;
                continue;
            }
            if ((spec == L's' || spec == L'd') && next < args.size()) {
                out += args[next++];
                ++i;
                continue;
            }
        }
        out += c;
    }
    return out;
}

int CountAccelerators(std::wstring_view text) {
    int count = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'&') {
            if (i + 1 < text.size() && text[i + 1] == L'&') {
                ++i;
            } else {
                ++count;
            }
        }
    }
    return count;
}

// ------------------------------------------------------------------ active language

int SetActiveCatalog(const ParsedCatalog& catalog) {
    ResetToEnglish();
    ActiveState& s = State();
    s.code = catalog.code.empty() ? std::string("en") : catalog.code;
    int translated = 0;
    for (const auto& [key, value] : catalog.entries) {
        StringId id;
        if (!FindStringId(key, id)) {
            continue;
        }
        if (PlaceholderSignature(value) != PlaceholderSignature(EnglishOf(id))) {
            continue;  // unsafe translation: keep English for this string
        }
        const auto index = static_cast<size_t>(id);
        s.text[index] = value;
        s.translated[index] = true;
        ++translated;
    }
    return translated;
}

void ResetToEnglish() {
    ActiveState& s = State();
    for (auto& t : s.text) {
        t.clear();
    }
    s.translated.fill(false);
    s.code = "en";
}

const std::string& ActiveLanguageCode() { return State().code; }

std::wstring TrFormat(StringId id, const std::vector<std::wstring>& args) { return Format(Tr(id), args); }

}  // namespace snippin::i18n
