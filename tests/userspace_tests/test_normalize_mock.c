#include <stdio.h>
#include <string.h>
#include <ctype.h>

void normalize_string_userspace(const char *input, char *output,
                                 int buf_size) {
    int i = 0, j = 0, in_word = 0, new_word = 1;
    if (!input || !output || buf_size <= 0) return;
    memset(output, 0, buf_size);

    while (input[i] && isspace((unsigned char)input[i])) i++;

    while (input[i] && j < buf_size - 1) {
        if (isspace((unsigned char)input[i])) {
            if (in_word) {
                output[j++] = ' ';
                in_word = 0;
                new_word = 1;
            }
        } else {
            in_word = 1;
            if (new_word) {
                output[j++] = toupper((unsigned char)input[i]);
                new_word = 0;
            } else {
                output[j++] = tolower((unsigned char)input[i]);
            }
        }
        i++;
    }
    if (j > 0 && output[j-1] == ' ') output[--j] = '\0';
    else output[j] = '\0';
}

int g_total = 0, g_failed = 0;

#define TEST(name, input, expected) do { \
    char out[256] = {0}; \
    normalize_string_userspace(input, out, sizeof(out)); \
    if (strcmp(out, expected) == 0) { \
        printf("[PASS] %-45s -> \"%s\"\n", name, out); \
    } else { \
        printf("[FAIL] %-45s\n  Expected: \"%s\"\n  Got:      \"%s\"\n", \
               name, expected, out); \
        g_failed++; \
    } \
    g_total++; \
} while(0)

int main(void) {
    printf("=== String Normalization Unit Tests ===\n\n");

    TEST("Trim leading spaces",       "   nguyen van an",      "Nguyen Van An");
    TEST("Trim trailing spaces",      "nguyen van an   ",      "Nguyen Van An");
    TEST("Collapse middle spaces",    "nguyen   van   an",     "Nguyen Van An");
    TEST("Full combination",          "  nguyen   VAN   AN  ", "Nguyen Van An");
    TEST("All uppercase",             "TRAN THI BINH",         "Tran Thi Binh");
    TEST("All lowercase",             "tran thi binh",         "Tran Thi Binh");
    TEST("Mixed case",                "lE vAN c",              "Le Van C");
    TEST("Single char lowercase",     "a",                     "A");
    TEST("Single char uppercase",     "A",                     "A");
    TEST("Only spaces",               "     ",                 "");
    TEST("Empty string",              "",                       "");
    TEST("Single word",               "nguyen",                "Nguyen");
    TEST("Two spaces between",        "van  an",               "Van An");
    TEST("Ten leading spaces",        "          a",           "A");
    TEST("Tab between words",         "nguyen\tvan",           "Nguyen Van");
    TEST("Mixed spaces and tabs",     "  nguyen\t van  ",      "Nguyen Van");

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
