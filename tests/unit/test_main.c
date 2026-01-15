/**
 * VNIDS Unit Tests - Main Entry Point
 *
 * Copyright (c) 2026 VNIDS Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Simple test framework macros */
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s == %s (line %d)\n", #a, #b, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: %s == %s (got \"%s\" vs \"%s\", line %d)\n", \
               #a, #b, (a), (b), __LINE__); \
        return 1; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("Running: %s\n", #test_func); \
    tests_run++; \
    int result = test_func(); \
    if (result == 0) { \
        tests_passed++; \
        printf("  PASS\n"); \
    } else { \
        tests_failed++; \
    } \
} while(0)

/* External test functions */
extern int test_event_queue_create(void);
extern int test_event_queue_push_pop(void);
extern int test_event_queue_full(void);
extern int test_config_parse(void);
extern int test_types_result_str(void);
extern int test_types_severity_str(void);
extern int test_types_protocol_str(void);

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("VNIDS Unit Tests\n");
    printf("================\n\n");

    /* Event Queue Tests */
    printf("\n[Event Queue Tests]\n");
    RUN_TEST(test_event_queue_create);
    RUN_TEST(test_event_queue_push_pop);
    RUN_TEST(test_event_queue_full);

    /* Config Tests */
    printf("\n[Config Tests]\n");
    RUN_TEST(test_config_parse);

    /* Types Tests */
    printf("\n[Types Tests]\n");
    RUN_TEST(test_types_result_str);
    RUN_TEST(test_types_severity_str);
    RUN_TEST(test_types_protocol_str);

    /* Summary */
    printf("\n================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("================\n");

    return tests_failed > 0 ? 1 : 0;
}
