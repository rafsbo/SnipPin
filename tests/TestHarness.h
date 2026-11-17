// Minimal self-contained test harness (no third-party dependency).
//
//   TEST_CASE("name") { CHECK(a == b); REQUIRE(ptr != nullptr); }
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace snippin::test {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& FailureCount() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

struct RequireFailed {};

inline void ReportFailure(const char* expr, const char* file, int line) {
    ++FailureCount();
    std::cerr << file << ":" << line << ": check failed: " << expr << "\n";
}

}  // namespace snippin::test

#define SNIPPIN_CONCAT_INNER(a, b) a##b
#define SNIPPIN_CONCAT(a, b) SNIPPIN_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                       \
    static void SNIPPIN_CONCAT(snippin_test_, __LINE__)();                                    \
    static ::snippin::test::Registrar SNIPPIN_CONCAT(snippin_registrar_, __LINE__)(           \
        name, &SNIPPIN_CONCAT(snippin_test_, __LINE__));                                      \
    static void SNIPPIN_CONCAT(snippin_test_, __LINE__)()

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) ::snippin::test::ReportFailure(#expr, __FILE__, __LINE__); \
    } while (0)

#define REQUIRE(expr)                                                     \
    do {                                                                  \
        if (!(expr)) {                                                    \
            ::snippin::test::ReportFailure(#expr, __FILE__, __LINE__);    \
            throw ::snippin::test::RequireFailed{};                       \
        }                                                                 \
    } while (0)
