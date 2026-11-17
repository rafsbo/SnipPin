#include "TestHarness.h"
#include "logic/Hotkey.h"

using namespace snippin::logic;

TEST_CASE("ParseHotkey reads modifiers and letters") {
    Hotkey hk;
    REQUIRE(ParseHotkey(L"Ctrl+Alt+T", hk));
    CHECK(hk.modifiers == (kModControl | kModAlt));
    CHECK(hk.vk == 'T');
}

TEST_CASE("ParseHotkey is case and whitespace tolerant") {
    Hotkey hk;
    REQUIRE(ParseHotkey(L"  control + shift +f5 ", hk));
    CHECK(hk.modifiers == (kModControl | kModShift));
    CHECK(hk.vk == 0x74);  // VK_F5
}

TEST_CASE("ParseHotkey understands named keys and aliases") {
    Hotkey hk;
    REQUIRE(ParseHotkey(L"Ctrl+Alt+Backspace", hk));
    CHECK(hk.vk == 0x08);
    REQUIRE(ParseHotkey(L"Win+Up", hk));
    CHECK(hk.modifiers == kModWin);
    CHECK(hk.vk == 0x26);
    REQUIRE(ParseHotkey(L"Alt+Escape", hk));
    CHECK(hk.vk == 0x1B);
    REQUIRE(ParseHotkey(L"Ctrl+Num7", hk));
    CHECK(hk.vk == 0x67);
}

TEST_CASE("ParseHotkey handles the plus key") {
    Hotkey hk;
    REQUIRE(ParseHotkey(L"Ctrl++", hk));
    CHECK(hk.modifiers == kModControl);
    CHECK(hk.vk == 0xBB);
}

TEST_CASE("ParseHotkey treats None and empty as disabled") {
    Hotkey hk{kModControl, 'X'};
    REQUIRE(ParseHotkey(L"None", hk));
    CHECK(!hk.IsEnabled());
    hk = Hotkey{kModControl, 'X'};
    REQUIRE(ParseHotkey(L"", hk));
    CHECK(!hk.IsEnabled());
}

TEST_CASE("ParseHotkey rejects malformed input") {
    Hotkey hk{kModControl, 'X'};
    CHECK(!ParseHotkey(L"Ctrl+Alt", hk));        // no key
    CHECK(!ParseHotkey(L"Hyper+T", hk));         // unknown modifier
    CHECK(!ParseHotkey(L"Ctrl+NotAKey", hk));    // unknown key
    CHECK(!ParseHotkey(L"Ctrl+F25", hk));        // out of range
    CHECK((hk == Hotkey{kModControl, 'X'}));     // untouched on failure
}

TEST_CASE("FormatHotkey produces canonical text that round-trips") {
    const Hotkey samples[] = {
        {kModControl | kModAlt, 'T'}, {kModControl | kModAlt, 0x28}, {kModShift | kModWin, 0x70},
        {kModAlt, 0xBE},              {kModControl, 0x08},          {0, 0x7B},
    };
    for (const auto& hk : samples) {
        Hotkey parsed;
        REQUIRE(ParseHotkey(FormatHotkey(hk), parsed));
        CHECK(parsed == hk);
    }
    CHECK(FormatHotkey(Hotkey{kModControl | kModAlt, 'T'}) == L"Ctrl+Alt+T");
    CHECK(FormatHotkey(Hotkey{}) == L"None");
}

TEST_CASE("Modifier lists parse and format") {
    unsigned mods = 0;
    REQUIRE(ParseModifiers(L"Ctrl+Shift", mods));
    CHECK(mods == (kModControl | kModShift));
    CHECK(FormatModifiers(kModAlt | kModControl) == L"Ctrl+Alt");
    CHECK(!ParseModifiers(L"Ctrl+T", mods));
}
