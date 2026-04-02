#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE   "/dev/string_norm"
#define BUF_SIZE 256

int g_total = 0, g_failed = 0;

void test_driver(const char *name, const char *input,
                 const char *expected) {
    char output[BUF_SIZE] = {0};

    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        printf("[SKIP] %-40s (driver not loaded)\n", name);
        return;
    }

    if (strlen(input) > 0)
        write(fd, input, strlen(input));

    ssize_t r = read(fd, output, BUF_SIZE - 1);
    if (r > 0) output[r] = '\0';

    close(fd);

    if (strcmp(output, expected) == 0)
        printf("[PASS] %-40s -> \"%s\"\n", name, output);
    else {
        printf("[FAIL] %-40s\n  Expected: \"%s\"\n  Got:      \"%s\"\n",
               name, expected, output);
        g_failed++;
    }
    g_total++;
}

int main(void) {
    printf("=== Driver Integration Tests ===\n");
    printf("Device: %s\n\n", DEVICE);

    test_driver("Basic normalize",        "nguyen van an",        "Nguyen Van An");
    test_driver("Leading spaces",         "  tran thi binh",      "Tran Thi Binh");
    test_driver("Trailing spaces",        "le van c  ",           "Le Van C");
    test_driver("Multiple spaces",        "nguyen   van   an",    "Nguyen Van An");
    test_driver("Full combination",       "  NGUYEN  VAN  AN  ",  "Nguyen Van An");
    test_driver("All uppercase",          "TRAN THI BINH",        "Tran Thi Binh");
    test_driver("Mixed case",             "lE vAN c",             "Le Van C");
    test_driver("Single word",            "  nguyen  ",           "Nguyen");
    test_driver("Whitespace only",        "   ",                   "");

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
