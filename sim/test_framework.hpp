// Minimal self-registering TEST()/CHECK harness. Non-zero exit on any failure.
#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace tf {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct AssertFail {
    std::string msg;
};

inline void do_check(bool cond, const char* expr, const char* file, int line,
                     const std::string& extra = "") {
    if (!cond) {
        std::ostringstream os;
        os << file << ":" << line << ": CHECK failed: " << expr;
        if (!extra.empty()) os << "  [" << extra << "]";
        throw AssertFail{os.str()};
    }
}

// Returns number of failed tests.
inline int run_all() {
    int failed = 0;
    int passed = 0;
    for (auto& tc : registry()) {
        try {
            tc.fn();
            std::printf("  [PASS] %s\n", tc.name.c_str());
            ++passed;
        } catch (const AssertFail& f) {
            std::printf("  [FAIL] %s\n         %s\n", tc.name.c_str(), f.msg.c_str());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("  [FAIL] %s\n         exception: %s\n", tc.name.c_str(), e.what());
            ++failed;
        }
    }
    std::printf("---- %d passed, %d failed (%zu total) ----\n",
                passed, failed, registry().size());
    return failed;
}

} // namespace tf

#define TF_CONCAT2(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT2(a, b)
#define TEST(name)                                                       \
    static void TF_CONCAT(tf_test_, __LINE__)();                         \
    static ::tf::Registrar TF_CONCAT(tf_reg_, __LINE__)(                 \
        name, &TF_CONCAT(tf_test_, __LINE__));                           \
    static void TF_CONCAT(tf_test_, __LINE__)()

#define CHECK(cond) ::tf::do_check((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        long long _va = (long long)(a);                                   \
        long long _vb = (long long)(b);                                   \
        std::ostringstream _os;                                          \
        _os << #a " == " #b ": " << _va << " vs " << _vb;                \
        ::tf::do_check(_va == _vb, #a " == " #b, __FILE__, __LINE__, _os.str()); \
    } while (0)
