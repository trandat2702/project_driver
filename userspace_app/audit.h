#ifndef AUDIT_LOG_H
#define AUDIT_LOG_H

#define AUDIT_LOG_FILE "audit.log"

/*
 * Ghi lại thao tác của hệ thống.
 * - username: Tên tài khoản đang thực hiện (truyền "SYSTEM" nếu là hệ thống)
 * - action_fmt: Mô tả thao tác dạng chuỗi format của printf (VD: "Added user %s")
 */
void log_audit(const char *username, const char *action_fmt, ...);

#endif // AUDIT_LOG_H
