#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "student.h"

static int g_driver_should_fail = 0;
static int g_total = 0;
static int g_failed = 0;

static void mock_driver_transform(const char *input, char *output, int buf_size) {
    int i = 0;
    int j = 0;
    int new_word = 1;

    if (!input || !output || buf_size <= 0) return;
    output[0] = '\0';

    while (input[i] && j < buf_size - 1) {
        unsigned char c = (unsigned char)input[i];

        if (isspace(c)) {
            if (j > 0 && output[j - 1] != ' ') {
                output[j++] = ' ';
            }
            new_word = 1;
        } else if (isalpha(c)) {
            output[j++] = (char)(new_word ? toupper(c) : tolower(c));
            new_word = 0;
        } else {
            output[j++] = (char)c;
            new_word = 0;
        }
        i++;
    }

    while (j > 0 && output[j - 1] == ' ') {
        j--;
    }
    output[j] = '\0';
}

int normalize_via_driver(const char *input, char *output, int buf_size) {
    if (g_driver_should_fail) {
        return -1;
    }
    mock_driver_transform(input, output, buf_size);
    return 0;
}

#define ASSERT_TRUE(name, cond) do { \
    g_total++; \
    if (cond) { \
        printf("[PASS] %s\n", name); \
    } else { \
        printf("[FAIL] %s\n", name); \
        g_failed++; \
    } \
} while (0)

#define ASSERT_STREQ(name, actual, expected) do { \
    g_total++; \
    if (strcmp((actual), (expected)) == 0) { \
        printf("[PASS] %s -> \"%s\"\n", name, actual); \
    } else { \
        printf("[FAIL] %s\n  Expected: \"%s\"\n  Actual:   \"%s\"\n", \
               name, expected, actual); \
        g_failed++; \
    } \
} while (0)

static void test_sanitizes_special_characters(void) {
    char out[256];

    g_driver_should_fail = 0;
    ASSERT_TRUE("normalize_name_best_effort succeeds when driver is available",
                normalize_name_best_effort("tran van dat $", out, sizeof(out)) == 0);
    ASSERT_STREQ("special symbols are removed from normalized display name",
                 out, "Tran Van Dat");
}

static void test_sanitizes_digits_and_punctuation(void) {
    char out[256];

    g_driver_should_fail = 0;
    ASSERT_TRUE("normalize_name_best_effort succeeds for mixed input",
                normalize_name_best_effort("toa 2@@  nguyen", out, sizeof(out)) == 0);
    ASSERT_STREQ("digits and punctuation are removed, spacing normalized",
                 out, "Toa Nguyen");
}

static void test_driver_unavailable_returns_error(void) {
    char out[256];

    g_driver_should_fail = 1;
    ASSERT_TRUE("normalize_name_best_effort fails when driver is unavailable",
                normalize_name_best_effort("nguyen van a", out, sizeof(out)) == -1);
    g_driver_should_fail = 0;
}

int main(void) {
    printf("=== Normalize Unit Tests (Current Project Behavior) ===\n\n");

    test_sanitizes_special_characters();
    test_sanitizes_digits_and_punctuation();
    test_driver_unavailable_returns_error();

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
