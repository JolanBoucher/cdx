#pragma once

/**
 * @file test_utils.h
 * @brief Minimal, dependency-free unit-testing harness shared by all cdx_lib test files.
 *
 * This header exists so that each module test file (test_cdx_types.cpp,
 * test_cdx_format.cpp, test_cdx_IO.cpp) can declare small, self-registering
 * test cases without depending on an external framework (GoogleTest,
 * Catch2, ...). Each test file is a standalone executable: it registers its
 * tests with CDX_TEST, then calls cdx_test::run_all() from its own main().
 *
 * Design notes:
 *  - Tests are registered at static-initialization time via the Registrar
 *    helper, so declaration order across translation units does not matter
 *    within a single test executable.
 *  - Assertions throw cdx_test::AssertionFailure instead of aborting, so a
 *    single failing assertion fails only its own test case and the runner
 *    continues with the remaining tests.
 *  - This harness intentionally has no notion of test fixtures, mocks, or
 *    parameterized tests: cdx_lib's test surface is small enough that plain
 *    functions plus a handful of assertion macros are sufficient.
 */

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cdx_test {
    /// One registered test case: a human-readable name and the function to run.
    struct TestCase {
        std::string name;
        std::function<void()> fn;
    };

    /**
     * @brief Returns the process-wide list of registered test cases.
     *
     * Uses function-local static storage so that registration works
     * correctly regardless of static-initialization order across
     * translation units.
     */
    inline std::vector<TestCase> &registry() {
        static std::vector<TestCase> tests;
        return tests;
    }

    /// RAII helper that appends a test case to the registry when constructed.
    struct Registrar {
        Registrar(const std::string &name, std::function<void()> fn) {
            registry().push_back({name, std::move(fn)});
        }
    };

    /// Exception type thrown by the CDX_ASSERT_* macros on assertion failure.
    struct AssertionFailure : std::runtime_error {
        explicit AssertionFailure(const std::string &msg) : std::runtime_error(msg) {
        }
    };

    /**
     * @brief Runs every registered test case and prints a pass/fail report.
     *
     * Each test case is run in isolation: an AssertionFailure or any other
     * exception it throws is caught, reported, and does not stop the
     * remaining tests from running.
     *
     * @param module_name Name of the module under test, used in the report header.
     * @return 0 if every test passed, 1 if at least one test failed.
     */
    inline int run_all(const std::string &module_name) {
        int failed = 0;
        std::cout << "=== Running tests for " << module_name << " ===\n";
        for (auto &test: registry()) {
            try {
                test.fn();
                std::cout << "[PASS] " << test.name << "\n";
            } catch (const std::exception &e) {
                ++failed;
                std::cout << "[FAIL] " << test.name << ": " << e.what() << "\n";
            } catch (...) {
                ++failed;
                std::cout << "[FAIL] " << test.name << ": unknown exception\n";
            }
        }
        const auto total = registry().size();
        std::cout << (total - static_cast<std::size_t>(failed)) << "/" << total << " tests passed.\n";
        return failed == 0 ? 0 : 1;
    }
} // namespace cdx_test

/// Declares and self-registers a test case named `name` (a plain identifier, not a string).
#define CDX_TEST(name)                                                        \
    static void name();                                                      \
    static cdx_test::Registrar registrar_##name(#name, name);                 \
    static void name()

/// Fails the current test unless `cond` is true.
#define CDX_ASSERT_TRUE(cond)                                                                 \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            throw cdx_test::AssertionFailure(std::string("CDX_ASSERT_TRUE failed: ") + #cond + \
                " at " + __FILE__ + ":" + std::to_string(__LINE__));                          \
        }                                                                                      \
    } while (0)

/// Fails the current test unless `a == b`.
#define CDX_ASSERT_EQ(a, b)                                                                    \
    do {                                                                                       \
        if (!((a) == (b))) {                                                                   \
            throw cdx_test::AssertionFailure(std::string("CDX_ASSERT_EQ failed: ") + #a + " == " + \
                #b + " at " + __FILE__ + ":" + std::to_string(__LINE__));                      \
        }                                                                                       \
    } while (0)

/// Fails the current test unless evaluating `expr` throws an exception of type `exc_type`.
#define CDX_ASSERT_THROWS(expr, exc_type)                                                      \
    do {                                                                                        \
        bool threw = false;                                                                     \
        try {                                                                                    \
            (void) (expr);                                                                       \
        } catch (const exc_type &) {                                                              \
            threw = true;                                                                          \
        }                                                                                            \
        if (!threw) {                                                                                \
            throw cdx_test::AssertionFailure(std::string("CDX_ASSERT_THROWS failed: expected ") +     \
                #exc_type + " from " + #expr + " at " + __FILE__ + ":" + std::to_string(__LINE__));    \
        }                                                                                                \
    } while (0)

/// Fails the current test if evaluating `expr` throws any std::exception.
#define CDX_ASSERT_NO_THROW(expr)                                                              \
    do {                                                                                        \
        try {                                                                                    \
            (void) (expr);                                                                       \
        } catch (const std::exception &e) {                                                        \
            throw cdx_test::AssertionFailure(std::string("CDX_ASSERT_NO_THROW failed: ") + #expr +  \
                " threw: " + e.what() + " at " + __FILE__ + ":" + std::to_string(__LINE__));        \
        }                                                                                              \
    } while (0)
