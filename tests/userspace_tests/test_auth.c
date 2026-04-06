#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "auth.h"

static int g_total = 0;
static int g_failed = 0;

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
        printf("[PASS] %s\n", name); \
    } else { \
        printf("[FAIL] %s\n  Expected: %s\n  Actual:   %s\n", \
               name, expected, actual); \
        g_failed++; \
    } \
} while (0)

static void write_seed_config(const char *path) {
    char admin_hash[65];
    char viewer_hash[65];
    FILE *fp;

    hash_sha256("admin123", admin_hash);
    hash_sha256("viewer123", viewer_hash);

    fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "admin:%s:%s\n", admin_hash, ROLE_ADMIN);
    fprintf(fp, "viewer:%s:%s\n", viewer_hash, ROLE_VIEWER);
    fclose(fp);
}

static void test_hash_sha256(void) {
    char out[65];
    hash_sha256("admin", out);
    ASSERT_STREQ("hash_sha256(admin) matches known digest",
                 out,
                 "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918");
}

static void test_authenticate_and_role(const char *cfg) {
    char role[16] = {0};

    ASSERT_TRUE("authenticate_with_role succeeds for admin/admin123",
                authenticate_with_role(cfg, "admin", "admin123", role, sizeof(role)) == 1);
    ASSERT_STREQ("admin role resolved correctly", role, ROLE_ADMIN);

    memset(role, 0, sizeof(role));
    ASSERT_TRUE("authenticate_with_role succeeds for viewer/viewer123",
                authenticate_with_role(cfg, "viewer", "viewer123", role, sizeof(role)) == 1);
    ASSERT_STREQ("viewer role resolved correctly", role, ROLE_VIEWER);

    ASSERT_TRUE("authenticate_credentials fails on wrong password",
                authenticate_credentials(cfg, "admin", "wrong") == 0);
}

static void test_get_role_list_add_delete(const char *cfg) {
    char role[16] = {0};
    char users[MAX_USERS][MAX_USERNAME];
    char roles[MAX_USERS][MAX_ROLE_LEN];
    int count;

    ASSERT_TRUE("get_user_role returns admin role",
                get_user_role(cfg, "admin", role, sizeof(role)) == 0);
    ASSERT_STREQ("get_user_role(admin) == admin", role, ROLE_ADMIN);

    ASSERT_TRUE("add_user adds a new viewer account",
                add_user(cfg, "newuser", "newpass", ROLE_VIEWER) == 0);
    ASSERT_TRUE("add_user rejects duplicate username",
                add_user(cfg, "newuser", "newpass", ROLE_VIEWER) == -4);
    ASSERT_TRUE("add_user rejects invalid role",
                add_user(cfg, "badrole", "pw", "owner") == -3);

    count = list_users(cfg, users, roles, MAX_USERS);
    ASSERT_TRUE("list_users returns at least 3 users after insertion", count >= 3);

    ASSERT_TRUE("delete_user removes existing account",
                delete_user(cfg, "newuser") == 0);
    ASSERT_TRUE("delete_user fails for non-existent account",
                delete_user(cfg, "newuser") == -3);
}

static void test_change_password(const char *cfg) {
    ASSERT_TRUE("change_password succeeds with correct old password",
                change_password(cfg, "viewer", "viewer123", "viewer456") == 0);

    ASSERT_TRUE("old password no longer authenticates",
                authenticate_credentials(cfg, "viewer", "viewer123") == 0);

    ASSERT_TRUE("new password authenticates",
                authenticate_credentials(cfg, "viewer", "viewer456") == 1);

    ASSERT_TRUE("change_password fails with wrong old password",
                change_password(cfg, "viewer", "wrong-old", "newer") == -1);
}

int main(void) {
    const char *cfg = "/tmp/test_auth_config.txt";

    printf("=== Auth Unit Tests (Current Project Behavior) ===\n\n");

    write_seed_config(cfg);
    test_hash_sha256();
    test_authenticate_and_role(cfg);
    test_get_role_list_add_delete(cfg);
    test_change_password(cfg);

    unlink(cfg);

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
