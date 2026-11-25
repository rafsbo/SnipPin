// Checks every shipped catalogue in res/lang against the string table.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "TestHarness.h"
#include "i18n/Catalog.h"

using namespace snippin::i18n;
namespace fs = std::filesystem;

namespace {

// The 24 official languages of the European Union.
const std::set<std::string> kEuLanguages = {"bg", "cs", "da", "de", "el", "en", "es", "et", "fi", "fr", "ga", "hr",
                                            "hu", "it", "lt", "lv", "mt", "nl", "pl", "pt", "ro", "sk", "sl", "sv"};

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<fs::path> LanguageFiles() {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(fs::path(SNIPPIN_RES_DIR) / "lang")) {
        if (entry.path().extension() == ".lang") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string EscapeValue(const std::wstring& value) {
    std::string out;
    for (char c : EncodeUtf8(value)) {
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += c;
        }
    }
    return out;
}

}  // namespace

// Writes the reference catalogue (res/lang/en.lang) to stdout.
// Usage: snippin_tests --dump-english > res/lang/en.lang
void DumpEnglishCatalog() {
    std::cout << "# SnipPin - English reference catalogue.\n"
                 "# GENERATED from src/i18n/StringTable.h: run `snippin_tests --dump-english > res/lang/en.lang`.\n"
                 "# Translators: copy this file to <code>.lang and translate the text after '='.\n"
                 "# See docs/TRANSLATING.md.\n\n"
                 "language.code = en\n"
                 "language.name = English\n"
                 "language.name_en = English\n\n";
    for (int i = 0; i < kStringCount; ++i) {
        const auto id = static_cast<StringId>(i);
        std::cout << KeyOf(id) << " = " << EscapeValue(EnglishOf(id)) << "\n";
    }
}

TEST_CASE("All EU languages ship a catalogue") {
    std::set<std::string> found;
    for (const auto& file : LanguageFiles()) {
        found.insert(file.stem().string());
    }
    for (const auto& code : kEuLanguages) {
        if (!found.count(code)) {
            std::cerr << "missing catalogue: res/lang/" << code << ".lang\n";
        }
        CHECK(found.count(code) == 1);
    }
}

TEST_CASE("Every catalogue is complete, valid and safe") {
    const auto files = LanguageFiles();
    REQUIRE(!files.empty());
    for (const auto& file : files) {
        const std::string name = file.filename().string();
        const ParsedCatalog c = ParseCatalog(ReadFile(file));
        for (const auto& error : c.errors) {
            std::cerr << name << ": " << error << "\n";
        }
        CHECK(c.errors.empty());
        for (const auto& key : c.unknownKeys) {
            std::cerr << name << ": unknown key '" << key << "'\n";
        }
        CHECK(c.unknownKeys.empty());
        CHECK(c.code == file.stem().string());
        CHECK(!c.nativeName.empty());
        CHECK(!c.englishName.empty());

        for (int i = 0; i < kStringCount; ++i) {
            const auto id = static_cast<StringId>(i);
            const auto it = c.entries.find(KeyOf(id));
            if (it == c.entries.end()) {
                std::cerr << name << ": missing '" << KeyOf(id) << "'\n";
                CHECK(it != c.entries.end());
                continue;
            }
            const std::wstring english = EnglishOf(id);
            if (PlaceholderSignature(it->second) != PlaceholderSignature(english)) {
                std::cerr << name << ": placeholders differ in '" << KeyOf(id) << "'\n";
                CHECK(false);
            }
            // Menu items keep exactly one keyboard accelerator (&), others none.
            if (CountAccelerators(it->second) != CountAccelerators(english)) {
                std::cerr << name << ": '&' accelerator count differs in '" << KeyOf(id) << "'\n";
                CHECK(false);
            }
        }
    }
}

TEST_CASE("en.lang matches the built-in English strings") {
    const ParsedCatalog c = ParseCatalog(ReadFile(fs::path(SNIPPIN_RES_DIR) / "lang" / "en.lang"));
    CHECK(c.errors.empty());
    for (int i = 0; i < kStringCount; ++i) {
        const auto id = static_cast<StringId>(i);
        const auto it = c.entries.find(KeyOf(id));
        const bool same = it != c.entries.end() && it->second == EnglishOf(id);
        if (!same) {
            std::cerr << "en.lang is stale for '" << KeyOf(id)
                      << "': run `snippin_tests --dump-english > res/lang/en.lang`\n";
        }
        CHECK(same);
    }
}

TEST_CASE("Every catalogue is embedded by res/Languages.rc") {
    const std::string rc = ReadFile(fs::path(SNIPPIN_RES_DIR) / "Languages.rc");
    for (const auto& file : LanguageFiles()) {
        const std::string entry = "\"lang/" + file.filename().string() + "\"";
        if (rc.find(entry) == std::string::npos) {
            std::cerr << "res/Languages.rc does not embed " << entry << "\n";
        }
        CHECK(rc.find(entry) != std::string::npos);
    }
}
