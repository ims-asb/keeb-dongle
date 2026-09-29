/* Tiny assertion helper for the host tests (no external test framework). */
#ifndef N96_CHECK_H
#define N96_CHECK_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_failures;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (long long)(a), _b = (long long)(b); g_checks++; \
    if (_a != _b) { g_failures++; fprintf(stderr, "FAIL %s:%d: %s == %s (%lld vs %lld)\n", \
    __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#define CHECK_STR(a, b) do { const char *_a = (a), *_b = (b); g_checks++; \
    if (strcmp(_a, _b) != 0) { g_failures++; fprintf(stderr, "FAIL %s:%d:\n  got:      %s\n  expected: %s\n", \
    __FILE__, __LINE__, _a, _b); } } while (0)

#define TEST_MAIN_END(name) do { printf("%s: %d checks, %d failures\n", name, g_checks, g_failures); \
    return g_failures ? 1 : 0; } while (0)
#endif
