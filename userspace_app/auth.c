#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <openssl/sha.h>
#include "auth.h"

/*
 * Module xác thực / phân quyền của ứng dụng.
 *
 * Dữ liệu tài khoản được lưu trong config.txt theo dạng:
 *   username:sha256hash:role
 *
 * Chủ đích thiết kế:
 * - băm mật khẩu ở user space bằng OpenSSL
 * - so khớp với file cấu hình
 * - tách vai trò admin/viewer để GUI bật/tắt chức năng tương ứng
 */

/* Chuyển chuỗi mật khẩu thành SHA-256 hex string dài 64 ký tự. */
void hash_sha256(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    output_hex[64] = '\0';
}

/* Ẩn echo trên terminal để mật khẩu CLI không hiện plaintext khi gõ. */
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
    /*
     * Hàm này xử lý một dòng bất kỳ trong config.txt.
     *
     * Kết quả mong muốn:
     * - bỏ ký tự xuống dòng ở cuối
     * - bỏ qua dòng comment / dòng rỗng
     * - hỗ trợ cả format mới 3 trường và format cũ 2 trường
     *
     * Việc gom logic parse vào một chỗ giúp các hàm authenticate/add/delete/list
     * không phải lặp lại cùng một đoạn xử lý chuỗi.
     */
    char clean[256];
    strncpy(clean, line, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    clean[strcspn(clean, "\n")] = '\0';

    if (clean[0] == '#' || clean[0] == '\0')
        return -1;  /* comment or blank */

    /* Ưu tiên định dạng đầy đủ: username:hash:role */
    char u[64], h[65], r[16];
    if (sscanf(clean, "%63[^:]:%64[^:]:%15s", u, h, r) == 3) {
        if (out_user) { strncpy(out_user, u, user_sz - 1); out_user[user_sz - 1] = '\0'; }
        if (out_hash) { strncpy(out_hash, h, hash_sz - 1); out_hash[hash_sz - 1] = '\0'; }
        if (out_role) { strncpy(out_role, r, role_sz - 1); out_role[role_sz - 1] = '\0'; }
        return 0;
    }

    /* Tương thích ngược với file cũ chưa có cột role. */
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
    /* Wrapper đơn giản khi caller chỉ cần biết đúng/sai, không cần role. */
    return authenticate_with_role(config_file, username, password, NULL, 0);
}

/* ── Authenticate with role output ─────────────────────────────────── */

int authenticate_with_role(const char *config_file,
                           const char *username,
                           const char *password,
                           char *role_out, size_t role_size) {
    /*
     * Hàm xác thực lõi:
     * - băm mật khẩu người dùng vừa nhập
     * - đọc từng dòng config.txt
     * - so khớp username + hash
     * - nếu cần, trả ra role để tầng GUI áp RBAC
     */
    char input_hash[65];
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;

    /* Không đủ tham số thì coi như xác thực thất bại ngay. */
    if (!config_file || !username || !password)
        return 0;

    /* Chỉ so sánh hash, không bao giờ so sánh plaintext password trong file. */
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

        /*
         * Điều kiện đăng nhập thành công:
         * - username trùng
         * - hash của password vừa nhập trùng với hash lưu trong file
         */
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
    /*
     * Luồng đăng nhập dành cho CLI.
     * GUI không dùng hàm này; GUI tự quản lý lockout và hiển thị lỗi riêng.
     */
    char username[64], password[64];
    int attempts = 0;
    const int max_attempts = 3;
    const int lockout_seconds = 30;

    while (1) {
        /* Đọc username theo từng dòng để tránh tràn bộ đệm khi nhập từ terminal. */
        printf("Username: ");
        if (!fgets(username, sizeof(username), stdin))
            return 0;
        username[strcspn(username, "\n")] = '\0';

        /* Password được nhập ở chế độ ẩn ký tự. */
        read_password("Password: ", password, sizeof(password));
        if (authenticate_credentials(config_file, username, password)) {
            printf("Login successful. Welcome, %s!\n", username);
            return 1;
        }

        attempts++;
        if (attempts >= max_attempts) {
            /* Lockout cứng trên giao diện dòng lệnh để giảm brute-force cơ bản. */
            printf("Too many failed attempts. System locked for %d seconds.\n",
                   lockout_seconds);
            for (int i = lockout_seconds; i > 0; i--) {
                /* Đếm ngược trực tiếp trên cùng một dòng terminal. */
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
    /*
     * Đổi mật khẩu nhưng giữ nguyên role.
     * Cách làm: nạp toàn bộ file vào RAM, sau đó rewrite file với hash mới.
     */
    /* Verify old credentials first */
    if (!authenticate_credentials(config_file, username, old_password))
        return -1;  /* Wrong old password */

    /* Không cho đổi sang mật khẩu rỗng để tránh tạo tài khoản "không mật khẩu". */
    if (!new_password || strlen(new_password) == 0)
        return -2;  /* New password empty */

    /* Read all lines from config */
    FILE *fp = fopen(config_file, "r");
    if (!fp) return -3;

    char lines[64][256];
    int line_count = 0;
    char new_hash[65];
    hash_sha256(new_password, new_hash);

    /*
     * Với thiết kế file-based đơn giản, cách dễ nhất là đọc toàn bộ file vào RAM,
     * sửa bản ghi cần thiết, rồi ghi đè lại file.
     */
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
            /* Chỉ thay hash; username và role phải được giữ nguyên. */
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
    /* Hàm tiện ích để tầng trên hỏi riêng role mà không cần xác thực lại mật khẩu. */
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

        /* Hàm này chỉ cần user và role, nên không quan tâm password/hash có gì. */
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
    /*
     * Thêm user mới vào cuối file config.
     * Mật khẩu luôn được băm trước khi ghi ra đĩa; file không lưu plaintext password.
     */
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;

    if (!config_file || !username || !password || !role)
        return -1;

    if (strlen(username) == 0 || strlen(password) == 0)
        return -2;  /* empty username or password */

    /* Chỉ cho phép 2 role hợp lệ để tránh ghi linh tinh vào file cấu hình. */
    if (strcmp(role, ROLE_ADMIN) != 0 && strcmp(role, ROLE_VIEWER) != 0)
        return -3;  /* invalid role */

    /* Kiểm tra trùng username trước khi append để giữ tính duy nhất của tài khoản. */
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

    /* Nếu qua được các bước trên, user mới sẽ được thêm vào cuối file. */
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
    /*
     * Xóa user bằng cách rewrite lại file và bỏ qua dòng tương ứng.
     * Đây là mô hình đơn giản nhưng đủ cho project file-based cỡ nhỏ.
     */
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

    /* Bước 1: xác nhận user có tồn tại thật trước khi ghi đè file. */
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

    /* Bước 2: ghi lại file và bỏ qua đúng dòng của user cần xóa. */
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
    /* Trả danh sách user + role cho hộp thoại quản lý tài khoản trong GUI. */
    char line[256], file_user[64], file_hash[65], file_role[16];
    FILE *fp;
    int count = 0;

    if (!config_file || !usernames || !roles || max_entries <= 0)
        return 0;

    fp = fopen(config_file, "r");
    if (!fp) return 0;

    while (fgets(line, sizeof(line), fp) && count < max_entries) {
        /* Chỉ các dòng parse được mới được coi là user hợp lệ. */
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
