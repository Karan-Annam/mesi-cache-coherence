// Entry point. TEST(...) cases self-register across the linked TUs; this runs
// them all and exits with the failure count.
#include "../test_framework.hpp"
#include <cstdio>

int main() {
    std::printf("==== MESI C++ model / layers test suite ====\n");
    int failed = tf::run_all();
    if (failed == 0) std::printf("ALL TESTS PASSED\n");
    else std::printf("%d TEST(S) FAILED\n", failed);
    return failed == 0 ? 0 : 1;
}
