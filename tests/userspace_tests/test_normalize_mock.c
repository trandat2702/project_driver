#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "student.h"

int normalize_via_driver(const char *input, char *output, int buf_size) {
    (void)input; (void)output; (void)buf_size;
    return -1;
}

static int g_total = 0, g_failed = 0;

#define TEST(name, cond) do { \
    g_total++; \
    if (cond) { printf("[PASS] %s\n", name); } \
    else { printf("[FAIL] %s\n", name); g_failed++; } \
} while (0)

#define TEST_STR(name, actual, expected) do { \
    g_total++; \
    if (strcmp((actual), (expected)) == 0) { \
        printf("[PASS] %s -> \"%s\"\n", name, actual); \
    } else { \
        printf("[FAIL] %s\n  Expected: \"%s\"\n  Got:      \"%s\"\n", name, expected, actual); \
        g_failed++; \
    } \
} while (0)

int main(void) {
    char out[256];

    printf("=== NORMALIZE STRING TESTS ===\n\n");

    /* --- Basic Title Case --- */
    printf("--- Basic Title Case ---\n");
    
    normalize_name_best_effort("nguyen van an", out, sizeof(out));
    TEST_STR("Lowercase to title case", out, "Nguyen Van An");

    normalize_name_best_effort("TRAN THI BINH", out, sizeof(out));
    TEST_STR("All uppercase to title case", out, "Tran Thi Binh");

    normalize_name_best_effort("lE vAn C", out, sizeof(out));
    TEST_STR("Mixed case to title case", out, "Le Van C");

    normalize_name_best_effort("an", out, sizeof(out));
    TEST_STR("Single word", out, "An");

    normalize_name_best_effort("A", out, sizeof(out));
    TEST_STR("Single character", out, "A");

    normalize_name_best_effort("ab", out, sizeof(out));
    TEST_STR("Two characters", out, "Ab");

    /* --- Whitespace Handling --- */
    printf("\n--- Whitespace Handling ---\n");

    normalize_name_best_effort("  nguyen van an", out, sizeof(out));
    TEST_STR("Leading spaces", out, "Nguyen Van An");

    normalize_name_best_effort("nguyen van an  ", out, sizeof(out));
    TEST_STR("Trailing spaces", out, "Nguyen Van An");

    normalize_name_best_effort("  nguyen van an  ", out, sizeof(out));
    TEST_STR("Both leading and trailing spaces", out, "Nguyen Van An");

    normalize_name_best_effort("nguyen   van   an", out, sizeof(out));
    TEST_STR("Multiple spaces between words", out, "Nguyen Van An");

    normalize_name_best_effort("   nguyen    van    an   ", out, sizeof(out));
    TEST_STR("Complex whitespace pattern", out, "Nguyen Van An");

    normalize_name_best_effort("\tnguyen\tvan\tan\t", out, sizeof(out));
    TEST_STR("Tab characters", out, "Nguyen Van An");

    normalize_name_best_effort("   ", out, sizeof(out));
    TEST_STR("Only whitespace", out, "");

    normalize_name_best_effort("", out, sizeof(out));
    TEST_STR("Empty string", out, "");

    normalize_name_best_effort(" ", out, sizeof(out));
    TEST_STR("Single space", out, "");

    /* --- Special Characters (removed without preserving space) --- */
    printf("\n--- Special Characters Removal ---\n");

    normalize_name_best_effort("nguyen van an!", out, sizeof(out));
    TEST_STR("Trailing exclamation", out, "Nguyen Van An");

    normalize_name_best_effort("!nguyen van an", out, sizeof(out));
    TEST_STR("Leading exclamation", out, "Nguyen Van An");

    normalize_name_best_effort("nguyen $van an", out, sizeof(out));
    TEST_STR("Special char with space before", out, "Nguyen Van An");

    normalize_name_best_effort("tran van dat $", out, sizeof(out));
    TEST_STR("Trailing special after space", out, "Tran Van Dat");

    /* --- Digit Removal --- */
    printf("\n--- Digit Removal ---\n");

    normalize_name_best_effort("123nguyen", out, sizeof(out));
    TEST_STR("Remove leading digits", out, "Nguyen");

    normalize_name_best_effort("nguyen456", out, sizeof(out));
    TEST_STR("Remove trailing digits", out, "Nguyen");

    normalize_name_best_effort("toa 2  nguyen", out, sizeof(out));
    TEST_STR("Digit with spaces", out, "Toa Nguyen");

    normalize_name_best_effort("12345", out, sizeof(out));
    TEST_STR("Only digits", out, "");

    normalize_name_best_effort("a1b2c3", out, sizeof(out));
    TEST_STR("Alternating chars and digits", out, "Abc");

    /* --- Vietnamese Names --- */
    printf("\n--- Vietnamese Names ---\n");

    normalize_name_best_effort("nguyen thi kim anh", out, sizeof(out));
    TEST_STR("Four word name", out, "Nguyen Thi Kim Anh");

    normalize_name_best_effort("le hoang long", out, sizeof(out));
    TEST_STR("Three word name", out, "Le Hoang Long");

    normalize_name_best_effort("pham van", out, sizeof(out));
    TEST_STR("Two word name", out, "Pham Van");

    normalize_name_best_effort("vo", out, sizeof(out));
    TEST_STR("One word name", out, "Vo");

    /* --- Edge Cases --- */
    printf("\n--- Edge Cases ---\n");

    normalize_name_best_effort("a b c d e", out, sizeof(out));
    TEST_STR("Single letter words", out, "A B C D E");

    normalize_name_best_effort("ab cd ef", out, sizeof(out));
    TEST_STR("Two char words", out, "Ab Cd Ef");

    normalize_name_best_effort("Nguyen Van An", out, sizeof(out));
    TEST_STR("Already title case", out, "Nguyen Van An");

    normalize_name_best_effort("nGuYeN vAn An", out, sizeof(out));
    TEST_STR("Alternating case", out, "Nguyen Van An");

    /* --- Combined Patterns --- */
    printf("\n--- Combined Patterns ---\n");

    normalize_name_best_effort("  NGUYEN   VAN   AN  ", out, sizeof(out));
    TEST_STR("Uppercase with extra spaces", out, "Nguyen Van An");

    normalize_name_best_effort("  @#$NGUYEN123   VAN456   AN!@#  ", out, sizeof(out));
    TEST_STR("Complex: spaces + specials + digits", out, "Nguyen Van An");

    normalize_name_best_effort("!!!nguyen!!!", out, sizeof(out));
    TEST_STR("Name surrounded by specials", out, "Nguyen");

    /* --- Buffer Size Handling --- */
    printf("\n--- Buffer Size Handling ---\n");

    TEST("Null output returns -1", normalize_name_best_effort("test", NULL, 64) == -1);
    TEST("Zero buffer size returns -1", normalize_name_best_effort("test", out, 0) == -1);

    char small[5];
    int ret = normalize_name_best_effort("nguyen van an", small, sizeof(small));
    TEST("Small buffer handled safely", ret == 0 && strlen(small) < sizeof(small));

    /* --- Long Names --- */
    printf("\n--- Long Names ---\n");

    normalize_name_best_effort("nguyen thi thanh thuy hong", out, sizeof(out));
    TEST_STR("Five word name", out, "Nguyen Thi Thanh Thuy Hong");

    char long_input[256], long_expected[256];
    strcpy(long_input, "a b c d e f g h i j");
    strcpy(long_expected, "A B C D E F G H I J");
    normalize_name_best_effort(long_input, out, sizeof(out));
    TEST_STR("Ten single letters", out, long_expected);

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
