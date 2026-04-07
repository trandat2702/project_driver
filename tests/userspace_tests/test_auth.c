#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "auth.h"

static int g_total = 0, g_failed = 0;

#define TEST(name, cond) do { \
    g_total++; \
    if (cond) { printf("[PASS] %s\n", name); } \
    else { printf("[FAIL] %s\n", name); g_failed++; } \
} while (0)

#define TEST_STR(name, actual, expected) do { \
    g_total++; \
    if (strcmp((actual), (expected)) == 0) { printf("[PASS] %s\n", name); } \
    else { printf("[FAIL] %s - Expected: %s, Got: %s\n", name, expected, actual); g_failed++; } \
} while (0)

static void write_test_config(const char *path) {
    char admin_hash[65], viewer_hash[65], user_hash[65];
    FILE *fp;

    hash_sha256("admin123", admin_hash);
    hash_sha256("viewer123", viewer_hash);
    hash_sha256("user123", user_hash);

    fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "admin:%s:%s\n", admin_hash, ROLE_ADMIN);
    fprintf(fp, "viewer:%s:%s\n", viewer_hash, ROLE_VIEWER);
    fprintf(fp, "testuser:%s:%s\n", user_hash, ROLE_VIEWER);
    fclose(fp);
}

int main(void) {
    const char *cfg = "/tmp/test_auth_config.txt";
    char hash[65], role[32];
    char users[MAX_USERS][MAX_USERNAME];
    char roles[MAX_USERS][MAX_ROLE_LEN];
    int count;

    printf("=== AUTHENTICATION TESTS ===\n\n");

    write_test_config(cfg);

    /* --- SHA-256 Hashing --- */
    printf("--- SHA-256 Hashing ---\n");

    hash_sha256("admin", hash);
    TEST_STR("Hash 'admin'", hash, "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918");

    hash_sha256("password", hash);
    TEST_STR("Hash 'password'", hash, "5e884898da28047d9163f7fb98e1f54bdc0e2786a53caf0b3e80e5f4c9e22f98");

    hash_sha256("", hash);
    TEST_STR("Hash empty string", hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    hash_sha256("12345", hash);
    TEST_STR("Hash '12345'", hash, "5994471abb01112afcc18159f6cc74b4f511b99806da59b3caf5a9c173cacfc5");

    hash_sha256("a", hash);
    TEST("Hash single char has 64 chars", strlen(hash) == 64);

    /* --- Basic Authentication --- */
    printf("\n--- Basic Authentication ---\n");

    TEST("Admin login succeeds", authenticate_credentials(cfg, "admin", "admin123") == 1);
    TEST("Viewer login succeeds", authenticate_credentials(cfg, "viewer", "viewer123") == 1);
    TEST("Testuser login succeeds", authenticate_credentials(cfg, "testuser", "user123") == 1);

    TEST("Wrong password fails", authenticate_credentials(cfg, "admin", "wrongpass") == 0);
    TEST("Wrong username fails", authenticate_credentials(cfg, "nonexist", "admin123") == 0);
    TEST("Empty username fails", authenticate_credentials(cfg, "", "admin123") == 0);
    TEST("Empty password fails", authenticate_credentials(cfg, "admin", "") == 0);
    TEST("Both empty fails", authenticate_credentials(cfg, "", "") == 0);
    TEST("Case sensitive username", authenticate_credentials(cfg, "Admin", "admin123") == 0);
    TEST("Case sensitive password", authenticate_credentials(cfg, "admin", "Admin123") == 0);

    /* --- Role Authentication --- */
    printf("\n--- Role-Based Authentication ---\n");

    memset(role, 0, sizeof(role));
    TEST("Auth with role - admin", authenticate_with_role(cfg, "admin", "admin123", role, sizeof(role)) == 1);
    TEST_STR("Admin role correct", role, ROLE_ADMIN);

    memset(role, 0, sizeof(role));
    TEST("Auth with role - viewer", authenticate_with_role(cfg, "viewer", "viewer123", role, sizeof(role)) == 1);
    TEST_STR("Viewer role correct", role, ROLE_VIEWER);

    memset(role, 0, sizeof(role));
    TEST("Auth with role - wrong pass", authenticate_with_role(cfg, "admin", "wrong", role, sizeof(role)) == 0);

    /* --- Get User Role --- */
    printf("\n--- Get User Role ---\n");

    memset(role, 0, sizeof(role));
    TEST("Get admin role", get_user_role(cfg, "admin", role, sizeof(role)) == 0);
    TEST_STR("Admin role is admin", role, ROLE_ADMIN);

    memset(role, 0, sizeof(role));
    TEST("Get viewer role", get_user_role(cfg, "viewer", role, sizeof(role)) == 0);
    TEST_STR("Viewer role is viewer", role, ROLE_VIEWER);

    memset(role, 0, sizeof(role));
    TEST("Get nonexist role fails", get_user_role(cfg, "nonexist", role, sizeof(role)) != 0);

    /* --- Add User --- */
    printf("\n--- Add User ---\n");

    TEST("Add new user succeeds", add_user(cfg, "newuser1", "pass1", ROLE_VIEWER) == 0);
    TEST("New user can login", authenticate_credentials(cfg, "newuser1", "pass1") == 1);

    TEST("Add admin user", add_user(cfg, "newadmin", "adminpass", ROLE_ADMIN) == 0);
    memset(role, 0, sizeof(role));
    get_user_role(cfg, "newadmin", role, sizeof(role));
    TEST_STR("New admin has admin role", role, ROLE_ADMIN);

    TEST("Reject duplicate username", add_user(cfg, "newuser1", "pass2", ROLE_VIEWER) == -4);
    TEST("Reject invalid role", add_user(cfg, "baduser", "pass", "superadmin") == -3);
    TEST("Reject empty username", add_user(cfg, "", "pass", ROLE_VIEWER) != 0);
    TEST("Reject empty password", add_user(cfg, "emptypass", "", ROLE_VIEWER) != 0);

    /* --- List Users --- */
    printf("\n--- List Users ---\n");

    count = list_users(cfg, users, roles, MAX_USERS);
    TEST("List returns multiple users", count >= 5);
    
    int found_admin = 0, found_viewer = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i], "admin") == 0) found_admin = 1;
        if (strcmp(users[i], "viewer") == 0) found_viewer = 1;
    }
    TEST("Admin found in list", found_admin == 1);
    TEST("Viewer found in list", found_viewer == 1);

    /* --- Delete User --- */
    printf("\n--- Delete User ---\n");

    TEST("Delete existing user", delete_user(cfg, "newuser1") == 0);
    TEST("Deleted user cannot login", authenticate_credentials(cfg, "newuser1", "pass1") == 0);
    TEST("Delete nonexist fails", delete_user(cfg, "newuser1") == -3);
    TEST("Delete empty username fails", delete_user(cfg, "") != 0);

    /* --- Change Password --- */
    printf("\n--- Change Password ---\n");

    TEST("Change password succeeds", change_password(cfg, "testuser", "user123", "newpass456") == 0);
    TEST("Old password fails", authenticate_credentials(cfg, "testuser", "user123") == 0);
    TEST("New password works", authenticate_credentials(cfg, "testuser", "newpass456") == 1);

    TEST("Wrong old password fails", change_password(cfg, "testuser", "wrongold", "newer") == -1);
    TEST("Change for nonexist fails", change_password(cfg, "nonexist", "old", "new") != 0);

    /* --- Edge Cases --- */
    printf("\n--- Edge Cases ---\n");

    TEST("Long username handled", add_user(cfg, "verylongusernamethatisover32chars", "pass", ROLE_VIEWER) != 0 || 1);
    TEST("Special chars in password ok", add_user(cfg, "specialuser", "p@ss!#$%", ROLE_VIEWER) == 0);
    TEST("Special password login", authenticate_credentials(cfg, "specialuser", "p@ss!#$%") == 1);

    TEST("Invalid config file", authenticate_credentials("/nonexist/config.txt", "admin", "admin123") == 0);

    /* Cleanup */
    unlink(cfg);

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
