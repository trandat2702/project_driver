#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "audit.h"

/*
 * Audit log là lớp ghi vết tối giản của hệ thống.
 *
 * Mỗi dòng log có dạng:
 *   [YYYY-MM-DD HH:MM:SS] [username] action...
 *
 * Thiết kế này đủ nhẹ để gọi trực tiếp từ GUI callback và các thao tác nghiệp vụ
 * mà không cần thêm subsystem logging phức tạp.
 */
void log_audit(const char *username, const char *action_fmt, ...) {
    FILE *fp = fopen(AUDIT_LOG_FILE, "a");
    if (!fp) return;

    /* Lấy thời gian hiện tại */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    /* Ghi prefix [Thời gian] [Tài khoản] */
    fprintf(fp, "[%s] [%s] ", time_str, username ? username : "SYSTEM");

    /* Ghi action_fmt kèm theo biến */
    va_list args;
    va_start(args, action_fmt);
    vfprintf(fp, action_fmt, args);
    va_end(args);

    fprintf(fp, "\n");
    fclose(fp);
}
