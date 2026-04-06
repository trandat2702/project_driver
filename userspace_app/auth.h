#ifndef AUTH_H
#define AUTH_H

#define MAX_USERS      64
#define MAX_USERNAME   64
#define MAX_ROLE_LEN   16

#define ROLE_ADMIN     "admin"
#define ROLE_VIEWER    "viewer"

/* Băm mật khẩu sang SHA-256 dạng chuỗi hex để lưu/so khớp trong config.txt. */
void hash_sha256(const char *input, char *output_hex);

/*
 * Xác thực username/password theo file config.
 * Đây là wrapper gọn khi caller chỉ cần biết đúng hay sai.
 */
int  authenticate_credentials(const char *config_file,
                              const char *username,
                              const char *password);

/*
 * Xác thực và đồng thời lấy ra role của user nếu đăng nhập thành công.
 * GUI dùng hàm này để quyết định bật/tắt các chức năng theo RBAC.
 */
int  authenticate_with_role(const char *config_file,
                            const char *username,
                            const char *password,
                            char *role_out, size_t role_size);

/* Luồng đăng nhập tương tác cho bản CLI, có lockout sau nhiều lần sai. */
int  authenticate(const char *config_file);

/* Đổi mật khẩu cho user hiện tại nhưng giữ nguyên role. */
int  change_password(const char *config_file,
                     const char *username,
                     const char *old_password,
                     const char *new_password);

/* Cấp lại mật khẩu mới cho user (bỏ qua kiểm tra pass cũ, chỉ dành cho Admin). */
int  admin_reset_password(const char *config_file,
                          const char *username,
                          const char *new_password);

/* Lấy role của một user bất kỳ từ config.txt. */
int  get_user_role(const char *config_file,
                   const char *username,
                   char *role_out, size_t role_size);

/* Thêm user mới vào hệ thống; caller nên tự bảo đảm đây là thao tác của admin. */
int  add_user(const char *config_file,
              const char *username,
              const char *password,
              const char *role);

/* Xóa user khỏi config.txt bằng cách ghi lại file. */
int  delete_user(const char *config_file,
                 const char *username);

/*
 * Liệt kê danh sách user và role.
 * Mảng usernames và roles phải được cấp phát sẵn bởi caller.
 */
int  list_users(const char *config_file,
                char usernames[][MAX_USERNAME],
                char roles[][MAX_ROLE_LEN],
                int max_entries);

#endif
