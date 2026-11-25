#include <cstring>

#include "TestHarness.h"

void DumpEnglishCatalog();  // TranslationFilesTests.cpp

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--dump-english") == 0) {
        DumpEnglishCatalog();
        return 0;
    }
    using namespace snippin::test;
    int failedTests = 0;
    for (const auto& test : Registry()) {
        const int before = FailureCount();
        try {
            test.body();
        } catch (const RequireFailed&) {
        } catch (const std::exception& e) {
            ++FailureCount();
            std::cerr << test.name << ": unexpected exception: " << e.what() << "\n";
        }
        const bool passed = FailureCount() == before;
        failedTests += passed ? 0 : 1;
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << "\n";
    }
    std::cout << "\n" << Registry().size() - static_cast<size_t>(failedTests) << "/" << Registry().size()
              << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
}
