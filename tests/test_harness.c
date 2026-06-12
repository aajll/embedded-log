/*
 * @file: test_harness.c
 * @brief Test runner implementation.
 */

#include "test_harness.h"

void
run_test(void (*test_func)(void), const char *name)
{
        test_func();
        TEST_PASS(name);
}
