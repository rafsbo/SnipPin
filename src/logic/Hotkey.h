// Hotkey text parsing/formatting ("Ctrl+Alt+T" <-> modifiers + virtual key).
// Platform independent: modifier bits and key codes mirror the Win32 values.
#pragma once

#include <string>
#include <string_view>

namespace snippin::logic {

// Same values as MOD_ALT / MOD_CONTROL / MOD_SHIFT / MOD_WIN.
enum HotkeyModifier : unsigned {
    kModAlt = 0x0001,
    kModControl = 0x0002,
    kModShift = 0x0004,
    kModWin = 0x0008,
};

struct Hotkey {
    unsigned modifiers = 0;  // combination of HotkeyModifier
    unsigned vk = 0;         // Win32 virtual-key code, 0 = disabled

    bool IsEnabled() const { return vk != 0; }
    bool operator==(const Hotkey& o) const { return modifiers == o.modifiers && vk == o.vk; }
    bool operator!=(const Hotkey& o) const { return !(*this == o); }
};

// Parses text such as "Ctrl+Alt+T", "ctrl + shift + F5" or "None".
// Returns false for malformed input (output left untouched).
bool ParseHotkey(std::wstring_view text, Hotkey& out);

// Formats a hotkey in canonical form ("Ctrl+Alt+T"). Disabled -> "None".
std::wstring FormatHotkey(const Hotkey& hotkey);

// Name of a single virtual key ("T", "F5", "Up"), empty when unknown.
std::wstring VirtualKeyName(unsigned vk);

// Parses a modifier list such as "Ctrl+Shift" (no key). Empty -> 0.
bool ParseModifiers(std::wstring_view text, unsigned& out);
std::wstring FormatModifiers(unsigned modifiers);

}  // namespace snippin::logic
