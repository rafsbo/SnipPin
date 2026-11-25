#include "TestHarness.h"
#include "i18n/Catalog.h"

using namespace snippin::i18n;

TEST_CASE("String table keys are unique and resolvable") {
    for (int i = 0; i < kStringCount; ++i) {
        const auto id = static_cast<StringId>(i);
        StringId found;
        REQUIRE(FindStringId(KeyOf(id), found));
        CHECK(found == id);  // first match is the same id -> keys are unique
        CHECK(EnglishOf(id)[0] != L'\0');
    }
    StringId dummy;
    CHECK(!FindStringId("no.such.key", dummy));
}

TEST_CASE("DecodeUtf8 handles multi-byte text and rejects bad input") {
    std::wstring out;
    CHECK(DecodeUtf8("Gr\xC3\xBC\xC3\x9F" "e \xE2\x80\xA6", out));  // "Grüße …"
    CHECK(out == L"Grüße …");
    CHECK(DecodeUtf8("\xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC", out));  // Greek
    CHECK(out == L"Ελληνικά");
    CHECK(!DecodeUtf8("bad \xC3", out));          // truncated sequence
    CHECK(!DecodeUtf8("bad \xC0\xAF", out));      // overlong encoding
    CHECK(!DecodeUtf8("bad \xED\xA0\x80", out));  // surrogate half
}

TEST_CASE("EncodeUtf8 round-trips through DecodeUtf8") {
    const std::wstring text = L"Malti: ħżġ · Lietuvių · Български";
    std::wstring back;
    REQUIRE(DecodeUtf8(EncodeUtf8(text), back));
    CHECK(back == text);
}

TEST_CASE("ParseCatalog reads keys, metadata, comments and escapes") {
    const char* text =
        "\xEF\xBB\xBF# comment\n"
        "language.code = it\r\n"
        "language.name = Italiano\n"
        "language.name_en = Italian\n"
        "; another comment\n"
        "\n"
        "menu.exit = E&sci\n"
        "about.format = SnipPin %s\\n\\n%s\\n\\nModo: %s\\nImpostazioni: %s\\tx \\\\ y\n";
    const ParsedCatalog c = ParseCatalog(text);
    CHECK(c.errors.empty());
    CHECK(c.unknownKeys.empty());
    CHECK(c.code == "it");
    CHECK(c.nativeName == L"Italiano");
    CHECK(c.englishName == L"Italian");
    REQUIRE(c.entries.count("menu.exit") == 1);
    CHECK(c.entries.at("menu.exit") == L"E&sci");
    CHECK(c.entries.at("about.format") == L"SnipPin %s\n\n%s\n\nModo: %s\nImpostazioni: %s\tx \\ y");
}

TEST_CASE("ParseCatalog reports malformed lines") {
    const ParsedCatalog c = ParseCatalog(
        "language.code = xx\n"
        "no equals sign\n"
        "Bad Key = value\n"
        "menu.exit =\n"
        "menu.exit = one\n"
        "menu.exit = two\n"
        "menu.unknown_entry = something\n"
        "menu.about = bad \\q escape\n");
    CHECK(c.errors.size() == 5);  // missing '=', invalid key, empty value, duplicate, bad escape
    REQUIRE(c.unknownKeys.size() == 1);
    CHECK(c.unknownKeys[0] == "menu.unknown_entry");
    CHECK(!ParseCatalog("menu.exit = x\n").errors.empty());  // missing language.code
}

TEST_CASE("PlaceholderSignature finds %s and %d in order") {
    CHECK(PlaceholderSignature(L"Snippet %d of %s") == "ds");
    CHECK(PlaceholderSignature(L"100% done, %% sign, (%)") == "");
    CHECK(PlaceholderSignature(L"%s%s") == "ss");
}

TEST_CASE("Format substitutes safely") {
    CHECK(Format(L"Opening %s…", {L"Notes"}) == L"Opening Notes…");
    CHECK(Format(L"%s / %d / %%", {L"a", L"7"}) == L"a / 7 / %");
    CHECK(Format(L"missing %s and %s", {L"one"}) == L"missing one and %s");  // no over-read
    CHECK(Format(L"no placeholders", {L"ignored"}) == L"no placeholders");
}

TEST_CASE("CountAccelerators ignores escaped ampersands") {
    CHECK(CountAccelerators(L"&Always on top") == 1);
    CHECK(CountAccelerators(L"Tom && Jerry") == 0);
    CHECK(CountAccelerators(L"&a && &b") == 2);
}

TEST_CASE("Active catalogue falls back to English and rejects unsafe entries") {
    ParsedCatalog c = ParseCatalog(
        "language.code = it\n"
        "menu.exit = E&sci\n"
        "notify.recall_launching = Apertura di %d\n");  // wrong placeholder type
    CHECK(SetActiveCatalog(c) == 1);
    CHECK(ActiveLanguageCode() == "it");
    CHECK(std::wstring(Tr(StringId::MenuExit)) == L"E&sci");
    CHECK(std::wstring(Tr(StringId::NotifyRecallLaunching)) == EnglishOf(StringId::NotifyRecallLaunching));
    CHECK(std::wstring(Tr(StringId::MenuAbout)) == EnglishOf(StringId::MenuAbout));
    ResetToEnglish();
    CHECK(ActiveLanguageCode() == "en");
    CHECK(std::wstring(Tr(StringId::MenuExit)) == EnglishOf(StringId::MenuExit));
}
