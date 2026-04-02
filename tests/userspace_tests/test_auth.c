#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

void hash_sha256(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    output_hex[64] = '\0';
}

int g_total = 0, g_failed = 0;

#define SHA_TEST(password, expected) do { \
    char result[65]; \
    hash_sha256(password, result); \
    if (strcmp(result, expected) == 0) \
        printf("[PASS] SHA256(\"%s\")\n", password); \
    else { \
        printf("[FAIL] SHA256(\"%s\")\n  Expected: %s\n  Got:      %s\n", \
               password, expected, result); \
        g_failed++; \
    } \
    g_total++; \
} while(0)

int main(void) {
    printf("=== SHA-256 Authentication Tests ===\n\n");

    SHA_TEST("admin",
        "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918");
    SHA_TEST("123",
        "a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3");
    SHA_TEST("",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    SHA_TEST("password123",
        "ef92b778bafe771e89245b89ecbc08a44a4e166c06659911881f383d4473e94f");
    SHA_TEST("student",
        "264c8c381bf16c982a4e59b0dd4c6f7808c51a05f64c35db42cc78a2a72875bb");

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
