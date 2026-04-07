#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE   "/dev/string_norm"
#define BUF_SIZE 512

static int g_total = 0, g_failed = 0, g_skipped = 0;

static int driver_available(void) {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static void test_driver(const char *name, const char *input, const char *expected) {
    char output[BUF_SIZE] = {0};

    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        printf("[SKIP] %-45s (driver not loaded)\n", name);
        g_skipped++;
        g_total++;
        return;
    }

    if (input && strlen(input) > 0)
        write(fd, input, strlen(input));

    ssize_t r = read(fd, output, BUF_SIZE - 1);
    if (r > 0) output[r] = '\0';

    close(fd);

    g_total++;
    if (strcmp(output, expected) == 0) {
        printf("[PASS] %-45s -> \"%s\"\n", name, output);
    } else {
        printf("[FAIL] %-45s\n  Expected: \"%s\"\n  Got:      \"%s\"\n", name, expected, output);
        g_failed++;
    }
}

int main(void) {
    printf("=== STRING_NORM DRIVER INTEGRATION TESTS ===\n");
    printf("Device: %s\n\n", DEVICE);

    if (!driver_available()) {
        printf("WARNING: Driver not loaded. Run: sudo insmod string_norm.ko\n\n");
    }

    /* --- Basic Normalization --- */
    printf("--- Basic Normalization ---\n");
    test_driver("Lowercase to title case",    "nguyen van an",       "Nguyen Van An");
    test_driver("All uppercase to title",     "TRAN THI BINH",       "Tran Thi Binh");
    test_driver("Mixed case normalize",       "lE vAn C",            "Le Van C");
    test_driver("Single word",                "nguyen",              "Nguyen");
    test_driver("Single character",           "a",                   "A");

    /* --- Whitespace Handling --- */
    printf("\n--- Whitespace Handling ---\n");
    test_driver("Leading spaces",             "  nguyen van an",     "Nguyen Van An");
    test_driver("Trailing spaces",            "nguyen van an  ",     "Nguyen Van An");
    test_driver("Both leading/trailing",      "  nguyen van an  ",   "Nguyen Van An");
    test_driver("Multiple spaces between",    "nguyen   van   an",   "Nguyen Van An");
    test_driver("Complex spacing",            "  nguyen   van  an ", "Nguyen Van An");
    test_driver("Only whitespace",            "     ",               "");
    test_driver("Tab characters",             "\tnguyen\tvan\t",     "Nguyen Van");

    /* --- Vietnamese Names --- */
    printf("\n--- Vietnamese Names ---\n");
    test_driver("Common name 1",              "tran van dat",        "Tran Van Dat");
    test_driver("Common name 2",              "le thi mai",          "Le Thi Mai");
    test_driver("Common name 3",              "pham hong son",       "Pham Hong Son");
    test_driver("Four word name",             "nguyen thi kim anh",  "Nguyen Thi Kim Anh");
    test_driver("Five word name",             "le hoang long an binh", "Le Hoang Long An Binh");

    /* --- Edge Cases --- */
    printf("\n--- Edge Cases ---\n");
    test_driver("Empty string",               "",                    "");
    test_driver("Single space",               " ",                   "");
    test_driver("Two spaces",                 "  ",                  "");
    test_driver("Already title case",         "Nguyen Van An",       "Nguyen Van An");
    test_driver("aLtErNaTiNg case",           "nGuYeN vAn An",       "Nguyen Van An");

    /* --- Long Names --- */
    printf("\n--- Long Names ---\n");
    test_driver("Long name",                  "nguyen thi thanh thuy hong ngoc", "Nguyen Thi Thanh Thuy Hong Ngoc");
    
    /* --- Special Input Patterns --- */
    printf("\n--- Special Input Patterns ---\n");
    test_driver("Multiple words same letter", "an an an an",         "An An An An");
    test_driver("Single letters",             "a b c d e",           "A B C D E");
    test_driver("Two char words",             "ab cd ef",            "Ab Cd Ef");

    /* --- Stress Tests --- */
    printf("\n--- Stress Tests ---\n");
    
    char long_input[256], long_expected[256];
    strcpy(long_input, "a b c d e f g h i j k l m n o p q r s t u v w x y z");
    strcpy(long_expected, "A B C D E F G H I J K L M N O P Q R S T U V W X Y Z");
    test_driver("26 single letters",          long_input,            long_expected);

    /* --- Consecutive Operations --- */
    printf("\n--- Consecutive Operations ---\n");
    
    if (driver_available()) {
        int fd = open(DEVICE, O_RDWR);
        if (fd >= 0) {
            char out1[BUF_SIZE] = {0}, out2[BUF_SIZE] = {0};
            
            write(fd, "first test", 10);
            read(fd, out1, BUF_SIZE - 1);
            
            write(fd, "second test", 11);
            read(fd, out2, BUF_SIZE - 1);
            
            close(fd);
            
            g_total += 2;
            if (strcmp(out1, "First Test") == 0) {
                printf("[PASS] First consecutive operation\n");
            } else {
                printf("[FAIL] First consecutive: expected 'First Test', got '%s'\n", out1);
                g_failed++;
            }
            
            if (strcmp(out2, "Second Test") == 0) {
                printf("[PASS] Second consecutive operation\n");
            } else {
                printf("[FAIL] Second consecutive: expected 'Second Test', got '%s'\n", out2);
                g_failed++;
            }
        }
    }

    /* --- Results --- */
    printf("\n=== Results: %d passed, %d failed, %d skipped ===\n",
           g_total - g_failed - g_skipped, g_failed, g_skipped);
    
    return g_failed > 0 ? 1 : 0;
}
