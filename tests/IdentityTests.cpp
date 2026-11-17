#include "TestHarness.h"
#include "logic/Identity.h"

using namespace snippin::logic;

namespace {

WindowIdentity Make(const wchar_t* exe, const wchar_t* cls, const wchar_t* title, const wchar_t* folder = L"") {
    return WindowIdentity{exe, cls, title, folder};
}

}  // namespace

TEST_CASE("MatchScore requires the same application and class") {
    const auto entry = Make(L"C:\\App\\calc.exe", L"CalcFrame", L"Calculator");
    CHECK(MatchScore(entry, Make(L"C:\\Other\\calc.exe", L"CalcFrame", L"Calculator")) == 0);
    CHECK(MatchScore(entry, Make(L"C:\\App\\calc.exe", L"OtherClass", L"Calculator")) == 0);
}

TEST_CASE("MatchScore is case-insensitive for paths and classes") {
    const auto entry = Make(L"C:\\App\\Calc.EXE", L"CalcFrame", L"Calculator");
    CHECK(MatchScore(entry, Make(L"c:\\app\\calc.exe", L"calcframe", L"Calculator")) == 100);
}

TEST_CASE("MatchScore ranks titles") {
    const auto entry = Make(L"excel.exe", L"XLMAIN", L"Budget.xlsx - Excel");
    CHECK(MatchScore(entry, Make(L"excel.exe", L"XLMAIN", L"Budget.xlsx - Excel")) == 100);
    CHECK(MatchScore(entry, Make(L"excel.exe", L"XLMAIN", L"Budget.xlsx - Excel [Read-Only]")) == 70);
    CHECK(MatchScore(entry, Make(L"excel.exe", L"XLMAIN", L"Other.xlsx - Excel")) == 40);
}

TEST_CASE("Explorer windows match by folder only") {
    const auto entry = Make(L"explorer.exe", L"CabinetWClass", L"Reports", L"D:\\Data\\Reports");
    CHECK(MatchScore(entry, Make(L"explorer.exe", L"CabinetWClass", L"Reports", L"d:\\data\\reports")) == 100);
    CHECK(MatchScore(entry, Make(L"explorer.exe", L"CabinetWClass", L"Reports", L"D:\\Other\\Reports")) == 0);
}

TEST_CASE("SameStashKey deduplicates history entries") {
    CHECK(SameStashKey(Make(L"a.exe", L"C", L"Doc 1"), Make(L"A.EXE", L"c", L"Doc 1")));
    CHECK(!SameStashKey(Make(L"a.exe", L"C", L"Doc 1"), Make(L"a.exe", L"C", L"Doc 2")));
    CHECK(SameStashKey(Make(L"explorer.exe", L"CabinetWClass", L"X", L"C:\\X"),
                       Make(L"explorer.exe", L"CabinetWClass", L"Renamed", L"c:\\x")));
}

TEST_CASE("PushMru moves duplicates to the front and trims") {
    std::vector<int> list{1, 2, 3};
    PushMru(list, 2, 3, [](int a, int b) { return a == b; });
    CHECK((list == std::vector<int>{2, 1, 3}));
    PushMru(list, 4, 3, [](int a, int b) { return a == b; });
    CHECK((list == std::vector<int>{4, 2, 1}));
    PushMru(list, 5, 0, [](int a, int b) { return a == b; });
    CHECK(list.empty());
}
