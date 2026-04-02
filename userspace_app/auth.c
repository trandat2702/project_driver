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

int authenticate_credentials(const char *config_file,
                             const char *username,
                             const char *password) {
    char input_hash[65];
    char line[256], file_user[64], file_hash[65];
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
        if (line[0] == '#' || line[0] == '\n')
            continue;

        line[strcspn(line, "\n")] = '\0';
        if (sscanf(line, "%63[^:]:%64s", file_user, file_hash) != 2)
            continue;

        if (strcmp(username, file_user) == 0 &&
            strcmp(input_hash, file_hash) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

int authenticate(const char *config_file) {
    char username[64], password[64];
    int attempts = 0;
    const int max_attempts = 3;

    while (attempts < max_attempts) {
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
        printf("Invalid credentials. Attempts remaining: %d\n",
               max_attempts - attempts);
    }

    printf("Too many failed attempts.\n");
    return 0;
}
