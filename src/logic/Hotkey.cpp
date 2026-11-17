#include "logic/Hotkey.h"

#include <cwctype>
#include <vector>

namespace snippin::logic {
namespace {

struct KeyName {
    unsigned vk;
    const wchar_t* name;
};

// Named keys. Letters, digits and function keys are handled separately.
constexpr KeyName kNamedKeys[] = {
    {0x08, L"Backspace"}, {0x09, L"Tab"},       {0x0D, L"Enter"},     {0x13, L"Pause"},
    {0x1B, L"Esc"},       {0x20, L"Space"},     {0x21, L"PageUp"},    {0x22, L"PageDown"},
    {0x23, L"End"},       {0x24, L"Home"},      {0x25, L"Left"},      {0x26, L"Up"},
    {0x27, L"Right"},     {0x28, L"Down"},      {0x2C, L"PrintScreen"}, {0x2D, L"Insert"},
    {0x2E, L"Delete"},    {0x6A, L"NumMultiply"}, {0x6B, L"NumAdd"},  {0x6D, L"NumSubtract"},
    {0x6E, L"NumDecimal"}, {0x6F, L"NumDivide"}, {0xBA, L";"},        {0xBB, L"="},
    {0xBC, L","},         {0xBD, L"-"},         {0xBE, L"."},         {0xBF, L"/"},
    {0xC0, L"`"},         {0xDB, L"["},         {0xDC, L"\\"},        {0xDD, L"]"},
    {0xDE, L"'"},
};

// Accepted aliases when parsing (lower case).
constexpr KeyName kAliases[] = {
    {0x1B, L"escape"}, {0x0D, L"return"}, {0x2E, L"del"},  {0x2D, L"ins"},
    {0x21, L"pgup"},   {0x22, L"pgdn"},   {0x08, L"back"}, {0xBB, L"plus"},
    {0xBD, L"minus"},
};

std::wstring ToLower(std::wstring_view s) {
    std::wstring out(s);
    for (auto& c : out) {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    return out;
}

std::wstring_view Trim(std::wstring_view s) {
    while (!s.empty() && std::iswspace(s.front())) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::iswspace(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}

// Splits on '+', but treats a trailing "+" key (e.g. "Ctrl++") as the key.
std::vector<std::wstring_view> SplitTokens(std::wstring_view text) {
    std::vector<std::wstring_view> tokens;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'+') {
            if (i == start) {  // empty token -> literal '+' key only at the end
                if (i == text.size() - 1) {
                    tokens.push_back(text.substr(i, 1));
                    return tokens;
                }
                continue;
            }
            tokens.push_back(Trim(text.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start < text.size()) {
        tokens.push_back(Trim(text.substr(start)));
    }
    return tokens;
}

bool ParseModifierToken(const std::wstring& lower, unsigned& mod) {
    if (lower == L"ctrl" || lower == L"control") {
        mod = kModControl;
    } else if (lower == L"alt") {
        mod = kModAlt;
    } else if (lower == L"shift") {
        mod = kModShift;
    } else if (lower == L"win" || lower == L"windows" || lower == L"super") {
        mod = kModWin;
    } else {
        return false;
    }
    return true;
}

bool ParseKeyToken(const std::wstring& lower, unsigned& vk) {
    if (lower.size() == 1) {
        const wchar_t c = lower[0];
        if (c >= L'a' && c <= L'z') {
            vk = 0x41 + (c - L'a');
            return true;
        }
        if (c >= L'0' && c <= L'9') {
            vk = 0x30 + (c - L'0');
            return true;
        }
        if (c == L'+') {
            vk = 0xBB;
            return true;
        }
    }
    if (lower.size() >= 2 && lower[0] == L'f') {
        int n = 0;
        bool digits = true;
        for (size_t i = 1; i < lower.size(); ++i) {
            if (!std::iswdigit(lower[i])) {
                digits = false;
                break;
            }
            n = n * 10 + (lower[i] - L'0');
        }
        if (digits && n >= 1 && n <= 24) {
            vk = 0x70 + (n - 1);
            return true;
        }
    }
    if (lower.size() == 4 && lower.compare(0, 3, L"num") == 0 && std::iswdigit(lower[3])) {
        vk = 0x60 + (lower[3] - L'0');
        return true;
    }
    for (const auto& k : kNamedKeys) {
        if (ToLower(k.name) == lower) {
            vk = k.vk;
            return true;
        }
    }
    for (const auto& k : kAliases) {
        if (lower == k.name) {
            vk = k.vk;
            return true;
        }
    }
    return false;
}

}  // namespace

std::wstring VirtualKeyName(unsigned vk) {
    if (vk >= 0x41 && vk <= 0x5A) {
        return std::wstring(1, static_cast<wchar_t>(L'A' + (vk - 0x41)));
    }
    if (vk >= 0x30 && vk <= 0x39) {
        return std::wstring(1, static_cast<wchar_t>(L'0' + (vk - 0x30)));
    }
    if (vk >= 0x70 && vk <= 0x87) {
        return L"F" + std::to_wstring(vk - 0x70 + 1);
    }
    if (vk >= 0x60 && vk <= 0x69) {
        return L"Num" + std::to_wstring(vk - 0x60);
    }
    for (const auto& k : kNamedKeys) {
        if (k.vk == vk) {
            return k.name;
        }
    }
    return {};
}

bool ParseModifiers(std::wstring_view text, unsigned& out) {
    text = Trim(text);
    if (text.empty() || ToLower(text) == L"none") {
        out = 0;
        return true;
    }
    unsigned mods = 0;
    for (auto token : SplitTokens(text)) {
        unsigned m = 0;
        if (!ParseModifierToken(ToLower(token), m)) {
            return false;
        }
        mods |= m;
    }
    out = mods;
    return true;
}

std::wstring FormatModifiers(unsigned modifiers) {
    std::wstring s;
    auto add = [&s](const wchar_t* name) {
        if (!s.empty()) {
            s += L'+';
        }
        s += name;
    };
    if (modifiers & kModControl) add(L"Ctrl");
    if (modifiers & kModAlt) add(L"Alt");
    if (modifiers & kModShift) add(L"Shift");
    if (modifiers & kModWin) add(L"Win");
    return s.empty() ? std::wstring(L"None") : s;
}

bool ParseHotkey(std::wstring_view text, Hotkey& out) {
    text = Trim(text);
    if (text.empty() || ToLower(text) == L"none") {
        out = Hotkey{};
        return true;
    }
    const auto tokens = SplitTokens(text);
    if (tokens.empty()) {
        return false;
    }
    Hotkey result;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::wstring lower = ToLower(tokens[i]);
        if (lower.empty()) {
            return false;
        }
        unsigned mod = 0;
        if (i + 1 < tokens.size()) {
            if (!ParseModifierToken(lower, mod)) {
                return false;
            }
            result.modifiers |= mod;
        } else {
            if (ParseModifierToken(lower, mod)) {
                return false;  // modifiers only, no key
            }
            if (!ParseKeyToken(lower, result.vk)) {
                return false;
            }
        }
    }
    out = result;
    return true;
}

std::wstring FormatHotkey(const Hotkey& hotkey) {
    if (!hotkey.IsEnabled()) {
        return L"None";
    }
    std::wstring key = VirtualKeyName(hotkey.vk);
    if (key.empty()) {
        return L"None";
    }
    if (hotkey.modifiers == 0) {
        return key;
    }
    return FormatModifiers(hotkey.modifiers) + L"+" + key;
}

}  // namespace snippin::logic
