#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <openssl/sha.h>
#include "auth.h"

void hash_sha256(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    output_hex[64] = '\0';
}

static void read_password(const char *prompt, char *buf, size_t n) {
    struct termios old, noecho;
    printf("%s", prompt);
    fflush(stdout);
    tcgetattr(STDIN_FILENO, &old);
    noecho = old;
    noecho.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &noecho);
    if (fgets(buf, n, stdin))
        buf[strcspn(buf, "\n")] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    printf("\n");
}

/* ── Internal: parse a config line ───────────────────────────────────── */
/* Format: username:sha256hash:role   (role is optional, defaults to admin) */
static int parse_config_line(const char *line,
                             char *out_user, size_t user_sz,
                             char *out_hash, size_t hash_sz,
                             char *out_role, size_t role_sz) {
    char clean[256];
    strncpy(clean, line, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    clean[strcspn(clean, "\n")] = '\0';

    if (clean[0] == '#' || clean[0] == '\0')
        return -1;  /* comment or blank */

    /* Try 3-field parse first */
    char u[64], h[65], r[16];
    if (sscanf(clean, "%63[^:]:%64[^:]:%15s", u, h, r) == 3) {
        if (out_user) { strncpy(out_user, u, user_sz - 1); out_user[user_sz - 1] = '\0'; }
        if (out_hash) { strncpy(out_hash, h, hash_sz - 1); out_hash[hash_sz - 1] = '\0'; }
        if (out_role) { strncpy(out_role, r, role_sz - 1); out_role[role_sz - 1] = '\0'; }
        return 0;
    }

    /* Fallback: 2-field (backward compat) → default role = admin */
    if (sscanf(clean, "%63[^:]:%64s", u, h) == 2) {
        if (out_user) { strncpy(out_user, u, user_sz - 1); out_user[user_sz - 1] = '\0'; }
        if (out_hash) { strncpy(out_hash, h, hash_sz - 1); out_hash[hash_sz - 1] = '\0'; }
        if (out_role) { strncpy(out_role, ROLE_ADMIN, role_sz - 1); out_role[role_sz - 1] = '\0'; }
        return 0;
    }

    return -1;  /* unparseable */
}

/* ── Authenticate (basic, no role output) ──────────────────────────── */

int authenticate_credentials(const char *config_file,
                             const char *username,
                             const char *password) {
    return authenticate_with_role(config_file, username, password, NULL, 0);
}

/* ── Authenticate with role output ─────────────────────────────────── */

int authenticate_with_role(const char *config_file,
                           const char *username,
                           const char *password,
                           char *role_out, size_t role_size) {
    char input_hash[65];
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;

    if (!config_file || !username || !password)
        return 0;

    hash_sha256(password, input_hash);

    fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open %s\n", config_file);
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (parse_config_line(line, file_user, sizeof(file_user),
                              file_hash, sizeof(file_hash),
                              file_role, sizeof(file_role)) != 0)
            continue;

        if (strcmp(username, file_user) == 0 &&
            strcmp(input_hash, file_hash) == 0) {
            if (role_out && role_size > 0) {
                strncpy(role_out, file_role, role_size - 1);
                role_out[role_size - 1] = '\0';
            }
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

/* ── CLI authenticate (interactive) ────────────────────────────────── */

int authenticate(const char *config_file) {
    char username[64], password[64];
    int attempts = 0;
    const int max_attempts = 3;
    const int lockout_seconds = 30;

    while (1) {
        printf("Username: ");
        if (!fgets(username, sizeof(username), stdin))
            return 0;
        username[strcspn(username, "\n")] = '\0';

        read_password("Password: ", password, sizeof(password));
        if (authenticate_credentials(config_file, username, password)) {
            printf("Login successful. Welcome, %s!\n", username);
            return 1;
        }

        attempts++;
        if (attempts >= max_attempts) {
            printf("Too many failed attempts. System locked for %d seconds.\n",
                   lockout_seconds);
            for (int i = lockout_seconds; i > 0; i--) {
                printf("\rRetry in %2d seconds...", i);
                fflush(stdout);
                sleep(1);
            }
            printf("\rLock expired. Please try again.         \n");
            attempts = 0;  /* Reset counter after lockout */
        } else {
            printf("Invalid credentials. Attempts remaining: %d\n",
                   max_attempts - attempts);
        }
    }
}

/* ── Change password (preserves role field) ────────────────────────── */

int change_password(const char *config_file,
                    const char *username,
                    const char *old_password,
                    const char *new_password) {
    /* Verify old credentials first */
    if (!authenticate_credentials(config_file, username, old_password))
        return -1;  /* Wrong old password */

    if (!new_password || strlen(new_password) == 0)
        return -2;  /* New password empty */

    /* Read all lines from config */
    FILE *fp = fopen(config_file, "r");
    if (!fp) return -3;

    char lines[64][256];
    int line_count = 0;
    char new_hash[65];
    hash_sha256(new_password, new_hash);

    while (line_count < 64 && fgets(lines[line_count], sizeof(lines[0]), fp)) {
        line_count++;
    }
    fclose(fp);

    /* Rewrite file, replacing the matching user's hash but keeping role */
    fp = fopen(config_file, "w");
    if (!fp) return -3;

    for (int i = 0; i < line_count; i++) {
        char file_user[64], file_hash[65], file_role[16];

        if (parse_config_line(lines[i], file_user, sizeof(file_user),
                              file_hash, sizeof(file_hash),
                              file_role, sizeof(file_role)) != 0) {
            fputs(lines[i], fp);  /* Keep comments / blank lines as-is */
            continue;
        }

        if (strcmp(file_user, username) == 0) {
            fprintf(fp, "%s:%s:%s\n", username, new_hash, file_role);
        } else {
            fputs(lines[i], fp);
        }
    }
    fclose(fp);
    return 0;  /* Success */
}

/* ── Get user role ─────────────────────────────────────────────────── */

int get_user_role(const char *config_file,
                  const char *username,
                  char *role_out, size_t role_size) {
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;

    if (!config_file || !username || !role_out || role_size == 0)
        return -1;

    fp = fopen(config_file, "r");
    if (!fp) return -1;

    while (fgets(line, sizeof(line), fp)) {
        if (parse_config_line(line, file_user, sizeof(file_user),
                              file_hash, sizeof(file_hash),
                              file_role, sizeof(file_role)) != 0)
            continue;

        if (strcmp(username, file_user) == 0) {
            strncpy(role_out, file_role, role_size - 1);
            role_out[role_size - 1] = '\0';
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;  /* user not found */
}

/* ── Add user ──────────────────────────────────────────────────────── */

int add_user(const char *config_file,
             const char *username,
             const char *password,
             const char *role) {
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;

    if (!config_file || !username || !password || !role)
        return -1;

    if (strlen(username) == 0 || strlen(password) == 0)
        return -2;  /* empty username or password */

    /* Validate role */
    if (strcmp(role, ROLE_ADMIN) != 0 && strcmp(role, ROLE_VIEWER) != 0)
        return -3;  /* invalid role */

    /* Check if user already exists */
    fp = fopen(config_file, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (parse_config_line(line, file_user, sizeof(file_user),
                                  file_hash, sizeof(file_hash),
                                  file_role, sizeof(file_role)) != 0)
                continue;
            if (strcmp(username, file_user) == 0) {
                fclose(fp);
                return -4;  /* user already exists */
            }
        }
        fclose(fp);
    }

    /* Append new user */
    fp = fopen(config_file, "a");
    if (!fp) return -5;

    char hash[65];
    hash_sha256(password, hash);
    fprintf(fp, "%s:%s:%s\n", username, hash, role);
    fclose(fp);
    return 0;  /* Success */
}

/* ── Delete user ───────────────────────────────────────────────────── */

int delete_user(const char *config_file,
                const char *username) {
    FILE *fp;

    if (!config_file || !username || strlen(username) == 0)
        return -1;

    fp = fopen(config_file, "r");
    if (!fp) return -2;

    char lines[64][256];
    int line_count = 0;
    int found = 0;

    while (line_count < 64 && fgets(lines[line_count], sizeof(lines[0]), fp)) {
        line_count++;
    }
    fclose(fp);

    /* Check if user exists */
    for (int i = 0; i < line_count; i++) {
        char file_user[64], file_hash[65], file_role[16];
        if (parse_config_line(lines[i], file_user, sizeof(file_user),
                              file_hash, sizeof(file_hash),
                              file_role, sizeof(file_role)) != 0)
            continue;
        if (strcmp(username, file_user) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) return -3;  /* user not found */

    /* Rewrite file without the deleted user */
    fp = fopen(config_file, "w");
    if (!fp) return -2;

    for (int i = 0; i < line_count; i++) {
        char file_user[64], file_hash[65], file_role[16];
        if (parse_config_line(lines[i], file_user, sizeof(file_user),
                              file_hash, sizeof(file_hash),
                              file_role, sizeof(file_role)) == 0 &&
            strcmp(username, file_user) == 0) {
            continue;  /* skip this user's line */
        }
        fputs(lines[i], fp);
    }
    fclose(fp);
    return 0;  /* Success */
}

/* ── List users ────────────────────────────────────────────────────── */

int list_users(const char *config_file,
               char usernames[][MAX_USERNAME],
               char roles[][MAX_ROLE_LEN],
               int max_entries) {
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;
    int count = 0;

    if (!config_file || !usernames || !roles || max_entries <= 0)
        return 0;

    fp = fopen(config_file, "r");
    if (!fp) return 0;

    while (fgets(line, sizeof(line), fp) && count < max_entries) {
        if (parse_config_line(line, file_user, sizeof(file_user),
                              file_hash, sizeof(file_hash),
                              file_role, sizeof(file_role)) != 0)
            continue;

        strncpy(usernames[count], file_user, MAX_USERNAME - 1);
        usernames[count][MAX_USERNAME - 1] = '\0';
        strncpy(roles[count], file_role, MAX_ROLE_LEN - 1);
        roles[count][MAX_ROLE_LEN - 1] = '\0';
        count++;
    }

    fclose(fp);
    return count;
}
