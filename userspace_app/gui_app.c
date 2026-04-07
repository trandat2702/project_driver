/* gui_app.c – Redesigned Student Manager GUI
 * Improvements:
 *   - GPA stored / displayed as formatted string ("3.50")
 *   - Login error shown inline (login_status_label), never goes to hidden widget
 *   - Enter key submits login and search
 *   - Search bar sits inside the table header for quick access
 *   - Form + action buttons grouped in a tidy card at the bottom
 *   - USB section lives in its own nested expander
 *   - Semantic button colours (green add, yellow edit, red delete)
 *   - Sidebar has clear section labels and a dedicated "Clear form" button
 *   - Logout moved exclusively to sidebar – no duplication in main area
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "auth.h"
#include "usb_driver_client.h"
#include "student.h"
#include "usb_file.h"
#include "audit.h"

#define COL_CODE  0
#define COL_NAME  1
#define COL_CLASS 2
#define COL_DOB   3
#define COL_GPA   4          /* now G_TYPE_STRING – "%.2f" formatted */
#define USB_TEXT_BUF_SIZE 2048

/*
 * AppState gom toàn bộ "trạng thái sống" của GUI vào một struct duy nhất.
 *
 * Nhờ đó các callback GTK có thể nhận cùng một con trỏ AppState và:
 * - truy cập dữ liệu sinh viên trong RAM
 * - cập nhật widget đang hiển thị
 * - gọi sang các module nghiệp vụ như auth/student/usb_file/audit
 */

/* ── AppState ──────────────────────────────────────────────────────────── */

typedef struct {
    Student list[MAX_STUDENTS];
    Student found[MAX_STUDENTS];
    int count;

    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *tree;
    GtkListStore *store;
    GtkWidget *status_label;        /* in dashboard footer */
    GtkWidget *login_status_label;  /* inline error on login card */
    GtkWidget *list_title_label;

    GtkWidget *login_user_entry;
    GtkWidget *login_pass_entry;
    GtkWidget *login_btn;           /* login button – disabled during lockout */

    int login_attempts;             /* failed login attempt counter */
    int lockout_remaining;          /* seconds remaining in lockout */

    GtkWidget *code_entry;
    GtkWidget *name_entry;
    GtkWidget *class_entry;
    GtkWidget *dob_entry;
    GtkWidget *gpa_entry;

    GtkWidget *search_entry;
    GtkWidget *file_entry;

    GtkWidget *btn_export_txt;
    GtkWidget *btn_export_csv;

    GtkWidget *sort_combo;

    char logged_in_user[64];         /* username of current session */
    char logged_in_role[MAX_ROLE_LEN]; /* role: admin or viewer */

    /* Role-sensitive widgets (disabled for viewer) */
    GtkWidget *btn_add;
    GtkWidget *btn_edit;
    GtkWidget *btn_delete;
    GtkWidget *btn_save;
    GtkWidget *btn_manage_users;     /* sidebar button, hidden for viewer */
    GtkWidget *form_card;            /* hidden for viewer */

    GtkWidget *topbar_user_label;    /* shows "user (role)" on topbar */
} AppState;

/* ── Validation helpers ────────────────────────────────────────────────── */

static int is_blank(const char *s) {
    while (s && *s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_code(const char *s) {
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isalnum((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_class(const char *s) {
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isspace((unsigned char)*s) && !isalnum((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_dob(const char *s) {
    int d, m, y;
    if (!s || strlen(s) != 10) return 0;
    if (s[2] != '/' || s[5] != '/') return 0;
    if (sscanf(s, "%d/%d/%d", &d, &m, &y) != 3) return 0;
    if (d < 1 || d > 31) return 0;
    if (m < 1 || m > 12) return 0;
    if (y < 1900 || y > 2100) return 0;
    return 1;
}

static void normalize_code_key(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!input) return;
    for (size_t i = 0; input[i] != '\0' && j < output_size - 1; i++) {
        if (!isspace((unsigned char)input[i]))
            output[j++] = (char)toupper((unsigned char)input[i]);
    }
    output[j] = '\0';
}

/* ── UI helpers ────────────────────────────────────────────────────────── */

static gboolean enable_confirm_buttons(gpointer user_data) {
    GtkDialog *dialog = GTK_DIALOG(user_data);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_NO,  TRUE);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_YES, TRUE);
    return G_SOURCE_REMOVE;
}

static int confirm_action(AppState *app, const char *title, const char *message) {
    GtkWidget *dialog;
    int response;

    dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                    GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_NONE,
                                    "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Không", GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Có",    GTK_RESPONSE_YES);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NONE);

    /* Prevent accidental click-through */
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_NO,  FALSE);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_YES, FALSE);
    g_timeout_add(450, enable_confirm_buttons, dialog);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return response == GTK_RESPONSE_YES;
}

static int choose_path(AppState *app, GtkFileChooserAction action,
                       const char *title, GtkWidget *target_entry) {
    GtkWidget *dialog;
    int accepted = 0;

    dialog = gtk_file_chooser_dialog_new(title, GTK_WINDOW(app->window), action,
                                         "_Hủy",   GTK_RESPONSE_CANCEL,
                                         "_Chọn",  GTK_RESPONSE_ACCEPT,
                                         NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(target_entry), path);
        g_free(path);
        accepted = 1;
    }
    gtk_widget_destroy(dialog);
    return accepted;
}

static void clear_form(AppState *app) {
    gtk_entry_set_text(GTK_ENTRY(app->code_entry),  "");
    gtk_entry_set_text(GTK_ENTRY(app->name_entry),  "");
    gtk_entry_set_text(GTK_ENTRY(app->class_entry), "");
    gtk_entry_set_text(GTK_ENTRY(app->dob_entry),   "");
    gtk_entry_set_text(GTK_ENTRY(app->gpa_entry),   "");
}

/* Update the dashboard footer status bar */
static void set_status(AppState *app, const char *fmt, ...) {
    char buf[512];
    va_list args;
    if (!app->status_label) return;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    gtk_label_set_text(GTK_LABEL(app->status_label), buf);
}

static void set_list_title(AppState *app, const char *prefix, int count) {
    char buf[160];
    if (!app->list_title_label) return;
    snprintf(buf, sizeof(buf), "%s  (%d)", prefix, count);
    gtk_label_set_text(GTK_LABEL(app->list_title_label), buf);
}

/* Refresh the TreeView from an arbitrary source array */
static void refresh_table(AppState *app, Student *source, int source_count) {
    char gpa_buf[16];
    gtk_list_store_clear(app->store);
    for (int i = 0; i < source_count; i++) {
        GtkTreeIter iter;
        snprintf(gpa_buf, sizeof(gpa_buf), "%.2f", source[i].gpa);
        gtk_list_store_append(app->store, &iter);
        gtk_list_store_set(app->store, &iter,
                           COL_CODE,  source[i].student_code,
                           COL_NAME,  source[i].normalized_name,
                           COL_CLASS, source[i].student_class,
                           COL_DOB,   source[i].dob,
                           COL_GPA,   gpa_buf,
                           -1);
    }
}

static void refresh_main_table(AppState *app) {
    refresh_table(app, app->list, app->count);
    set_list_title(app, "Danh sách sinh viên", app->count);
}

/* Return the student_code of the selected row (0 = nothing selected) */
static int get_selected_code(AppState *app, char *out, size_t out_size) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *code = NULL;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return 0;
    gtk_tree_model_get(model, &iter, COL_CODE, &code, -1);
    if (!code) return 0;
    strncpy(out, code, out_size - 1);
    out[out_size - 1] = '\0';
    g_free(code);
    return 1;
}

/* ── Callbacks ─────────────────────────────────────────────────────────── */

/* ── Login lockout timer callback ──────────────────────────────────────── */

static gboolean on_lockout_tick(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    app->lockout_remaining--;

    if (app->lockout_remaining <= 0) {
        /* Unlock */
        gtk_widget_set_sensitive(app->login_btn, TRUE);
        gtk_widget_set_sensitive(app->login_user_entry, TRUE);
        gtk_widget_set_sensitive(app->login_pass_entry, TRUE);
        app->login_attempts = 0;
        if (app->login_status_label) {
            gtk_label_set_text(GTK_LABEL(app->login_status_label),
                               "Đã mở khóa. Vui lòng thử lại.");
            gtk_widget_show(app->login_status_label);
        }
        return G_SOURCE_REMOVE;  /* stop timer */
    }

    /* Update countdown */
    char msg[128];
    snprintf(msg, sizeof(msg),
             "Đăng nhập bị khóa. Thử lại sau %d giây...",
             app->lockout_remaining);
    if (app->login_status_label) {
        gtk_label_set_text(GTK_LABEL(app->login_status_label), msg);
        gtk_widget_show(app->login_status_label);
    }
    return G_SOURCE_CONTINUE;  /* keep ticking */
}

static void on_login_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(app->login_user_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(app->login_pass_entry));

    /*
     * Callback này là cửa vào chính của GUI:
     * UI -> auth.c -> role -> bật/tắt các nút theo RBAC -> chuyển sang dashboard.
     */

    /* Clear any previous inline error */
    if (app->login_status_label) {
        gtk_label_set_text(GTK_LABEL(app->login_status_label), "");
        gtk_widget_hide(app->login_status_label);
    }

    if (is_blank(username) || is_blank(password)) {
        if (app->login_status_label) {
            gtk_label_set_text(GTK_LABEL(app->login_status_label),
                               "Tài khoản hoặc mật khẩu không được để trống");
            gtk_widget_show(app->login_status_label);
        }
        return;
    }

    char role[MAX_ROLE_LEN] = {0};
    if (!authenticate_with_role("config.txt", username, password, role, sizeof(role))) {
        log_audit(username, "Failed login attempt");
        app->login_attempts++;
        char err_msg[160];

        if (app->login_attempts >= 3) {
            /* Lock out for 30 seconds */
            app->lockout_remaining = 30;
            gtk_widget_set_sensitive(app->login_btn, FALSE);
            gtk_widget_set_sensitive(app->login_user_entry, FALSE);
            gtk_widget_set_sensitive(app->login_pass_entry, FALSE);

            snprintf(err_msg, sizeof(err_msg),
                     "Sai 3 lần liên tiếp. Khóa đăng nhập %d giây...",
                     app->lockout_remaining);
            g_timeout_add_seconds(1, on_lockout_tick, app);
        } else {
            snprintf(err_msg, sizeof(err_msg),
                     "Sai tài khoản hoặc mật khẩu (còn %d lần thử)",
                     3 - app->login_attempts);
        }

        if (app->login_status_label) {
            gtk_label_set_text(GTK_LABEL(app->login_status_label), err_msg);
            gtk_widget_show(app->login_status_label);
        }
        return;
    }

    /* Success – reset counter, save username & role */
    app->login_attempts = 0;
    strncpy(app->logged_in_user, username, sizeof(app->logged_in_user) - 1);
    app->logged_in_user[sizeof(app->logged_in_user) - 1] = '\0';
    strncpy(app->logged_in_role, role, sizeof(app->logged_in_role) - 1);
    app->logged_in_role[sizeof(app->logged_in_role) - 1] = '\0';

    /* Update topbar user info */
    if (app->topbar_user_label) {
        char user_info[128];
        const char *role_vi = (strcmp(role, ROLE_ADMIN) == 0) ?
                              "Quản trị viên" : "Chỉ xem";
        snprintf(user_info, sizeof(user_info), "👤 %s (%s)", username, role_vi);
        gtk_label_set_text(GTK_LABEL(app->topbar_user_label), user_info);
    }

    /*
     * Quyền được áp ngay trên widget:
     * viewer vẫn xem danh sách được, nhưng các nút thay đổi dữ liệu bị khóa.
     */
    int is_admin = (strcmp(role, ROLE_ADMIN) == 0);
    if (app->btn_add) {
        gtk_widget_set_visible(app->btn_add, is_admin);
        gtk_widget_set_no_show_all(app->btn_add, !is_admin);
    }
    if (app->btn_edit) {
        gtk_widget_set_visible(app->btn_edit, is_admin);
        gtk_widget_set_no_show_all(app->btn_edit, !is_admin);
    }
    if (app->btn_delete) {
        gtk_widget_set_visible(app->btn_delete, is_admin);
        gtk_widget_set_no_show_all(app->btn_delete, !is_admin);
    }
    if (app->btn_save) {
        gtk_widget_set_visible(app->btn_save, is_admin);
        gtk_widget_set_no_show_all(app->btn_save, !is_admin);
    }
    if (app->form_card) {
        gtk_widget_set_visible(app->form_card, is_admin);
        gtk_widget_set_no_show_all(app->form_card, !is_admin);
    }
    if (app->btn_export_txt) {
        gtk_widget_set_visible(app->btn_export_txt, TRUE);
        gtk_widget_set_no_show_all(app->btn_export_txt, FALSE);
    }
    if (app->btn_export_csv) {
        gtk_widget_set_visible(app->btn_export_csv, TRUE);
        gtk_widget_set_no_show_all(app->btn_export_csv, FALSE);
    }
    if (app->btn_manage_users) {
        gtk_widget_set_visible(app->btn_manage_users, is_admin);
        gtk_widget_set_no_show_all(app->btn_manage_users, !is_admin);
    }

    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "dashboard");
    set_status(app, "Đăng nhập thành công. Xin chào, %s! [%s]",
               username, is_admin ? "Admin" : "Viewer");
}

static void on_add_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    const char *code         = gtk_entry_get_text(GTK_ENTRY(app->code_entry));
    const char *name         = gtk_entry_get_text(GTK_ENTRY(app->name_entry));
    const char *student_class = gtk_entry_get_text(GTK_ENTRY(app->class_entry));
    const char *dob          = gtk_entry_get_text(GTK_ENTRY(app->dob_entry));
    const char *gpa_str      = gtk_entry_get_text(GTK_ENTRY(app->gpa_entry));
    float gpa = 0.0f;

    if (!is_valid_code(code)) {
        set_status(app, "Mã sinh viên không hợp lệ (chỉ dùng chữ và số)");
        return;
    }
    if (is_blank(name) || is_blank(student_class)) {
        set_status(app, "Họ tên và lớp không được để trống");
        return;
    }
    if (!is_valid_dob(dob)) {
        set_status(app, "Ngày sinh phải đúng định dạng dd/mm/yyyy");
        return;
    }
    if (!is_valid_name(name)) {
        set_status(app, "Họ tên không được chứa số hoặc ký tự đặc biệt");
        return;
    }
    char *endptr;
    gpa = strtof(gpa_str, &endptr);
    while (endptr && *endptr && isspace((unsigned char)*endptr)) endptr++;
    if (!endptr || endptr == gpa_str || *endptr != '\0' || gpa < 0.0f || gpa > 4.0f) {
        set_status(app, "GPA phải là số thập phân hợp lệ nằm trong khoảng [0.0 – 4.0]");
        return;
    }

    /*
     * add_student() sẽ gọi tiếp sang student.c, và ở đó tên sinh viên có thể
     * đi qua /dev/string_norm trước khi được đưa vào mảng app->list.
     */
    if (add_student(app->list, &app->count, code, name, student_class, dob, gpa) == 0) {
        refresh_main_table(app);
        set_status(app, "Thêm sinh viên %s thành công!", code);
        log_audit(app->logged_in_user, "Added student: %s", code);
        clear_form(app);
    } else {
        set_status(app, "Them that bai: ma %s da ton tai hoac driver string_norm chua san sang", code);
    }
}

static void on_delete_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    char code[MAX_CODE_LEN];
    char normalized_code[MAX_CODE_LEN];

    if (!get_selected_code(app, code, sizeof(code))) {
        const char *entry_code = gtk_entry_get_text(GTK_ENTRY(app->code_entry));
        if (!is_blank(entry_code)) {
            strncpy(code, entry_code, sizeof(code) - 1);
            code[sizeof(code) - 1] = '\0';
        } else {
            set_status(app, "Chon mot dong trong bang hoac nhap Ma SV de xoa");
            return;
        }
    }

    if (!confirm_action(app, "Xac nhan xoa",
                        "Ban co chac muon xoa sinh vien nay khong?"))
        return;

    normalize_code_key(code, normalized_code, sizeof(normalized_code));
    if (delete_student(app->list, &app->count, normalized_code) == 0) {
        refresh_main_table(app);
        set_status(app, "Xóa sinh viên (Mã SV: %s) thành công!", normalized_code);
        log_audit(app->logged_in_user, "Deleted student: %s", normalized_code);
        clear_form(app);
    } else {
        set_status(app, "Xoa that bai – khong tim thay ma: %s", normalized_code);
    }
}

static void on_search_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    const char *key = gtk_entry_get_text(GTK_ENTRY(app->search_entry));
    int found_count;

    if (is_blank(key)) {
        refresh_main_table(app);
        set_status(app, "Hien thi toan bo danh sach");
        return;
    }

    found_count = search_student(app->list, app->count, key, app->found);
    refresh_table(app, app->found, found_count);
    set_list_title(app, "Ket qua tim kiem", found_count);
    set_status(app, "Tim thay %d sinh vien voi tu khoa \"%s\"", found_count, key);
}

static void on_list_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    gtk_entry_set_text(GTK_ENTRY(app->search_entry), "");
    refresh_main_table(app);
    set_status(app, "Hien thi toan bo %d sinh vien", app->count);
}

static void on_save_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    const char *file   = gtk_entry_get_text(GTK_ENTRY(app->file_entry));
    const char *target = is_blank(file) ? "students.txt" : file;

    /* Lưu chụp trạng thái mảng sinh viên hiện tại xuống file đích. */
    if (save_to_file(target, app->list, app->count) == 0) {
        set_status(app, "Đã lưu danh sách vào %s", target);
        log_audit(app->logged_in_user, "Saved data to file: %s", target);
    } else
        set_status(app, "Luu that bai: %s", target);
}

static void on_load_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    const char *file   = gtk_entry_get_text(GTK_ENTRY(app->file_entry));
    const char *target = is_blank(file) ? "students.txt" : file;

    if (app->count > 0 &&
        !confirm_action(app, "Xac nhan tai",
                        "Tai du lieu se thay the danh sach hien tai. Tiep tuc?")) {
        set_status(app, "Da huy tai du lieu");
        return;
    }

    /*
     * load_from_file() thay thế toàn bộ danh sách trong RAM,
     * nên sau khi load phải refresh lại TreeView.
     */
    int rc = load_from_file(target, app->list, &app->count);
    if (rc == 0) {
        refresh_main_table(app);
        set_status(app, "Đã tải %d sinh viên từ: %s", app->count, target);
        log_audit(app->logged_in_user, "Loaded data from file: %s", target);
    } else if (rc == -2) {
        set_status(app, "Từ chối tải file: sai định dạng (text app hoặc CSV Excel) hoặc vi phạm nghiệp vụ");
        log_audit(app->logged_in_user,
                  "Rejected import file (invalid format or business rules): %s",
                  target);
    } else {
        set_status(app, "Tai that bai: %s", target);
    }
}

static void on_export_csv_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    /* Export báo cáo CSV phục vụ mở bằng Excel; dữ liệu lấy từ app->list hiện tại. */
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Lưu File CSV",
                                      GTK_WINDOW(app->window),
                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      "Hủy", GTK_RESPONSE_CANCEL,
                                      "Xuất", GTK_RESPONSE_ACCEPT,
                                      NULL);
                                      
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "danh_sach.csv");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (export_to_csv(filename, app->list, app->count) == 0) {
            set_status(app, "Đã xuất báo cáo CSV: %s", filename);
            log_audit(app->logged_in_user, "Exported CSV: %s", filename);
        } else {
            set_status(app, "Lỗi khi xuất CSV!");
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_export_usb_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    GtkWidget *dialog;

    if (!confirm_action(app, "Xac nhan xuat",
                        "Xuat toan bo danh sach hien tai ra USB?"))
        return;

    dialog = gtk_file_chooser_dialog_new("Lưu File Text",
                                         GTK_WINDOW(app->window),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "Hủy", GTK_RESPONSE_CANCEL,
                                         "Xuất", GTK_RESPONSE_ACCEPT,
                                         NULL);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "danh_sach.txt");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (save_to_file(filename, app->list, app->count) == 0) {
            set_status(app, "Đã xuất báo cáo Text: %s", filename);
            log_audit(app->logged_in_user, "Exported TXT: %s", filename);
        } else {
            set_status(app, "Lỗi khi xuất Text!");
        }
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

static void on_browse_file_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    if (choose_path(app, GTK_FILE_CHOOSER_ACTION_SAVE,
                    "Chon file du lieu sinh vien", app->file_entry))
        set_status(app, "Da chon file du lieu");
}

static void on_change_password_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Đổi mật khẩu", GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Xác nhận", GTK_RESPONSE_ACCEPT,
        "Hủy",     GTK_RESPONSE_CANCEL,
        NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, -1);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 8);

    GtkWidget *lbl_info = gtk_label_new(NULL);
    char info_buf[128];
    snprintf(info_buf, sizeof(info_buf),
             "Đổi mật khẩu cho tài khoản: <b>%s</b>", app->logged_in_user);
    gtk_label_set_markup(GTK_LABEL(lbl_info), info_buf);
    gtk_widget_set_halign(lbl_info, GTK_ALIGN_START);

    GtkWidget *old_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(old_entry), "Mật khẩu cũ");
    gtk_entry_set_visibility(GTK_ENTRY(old_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(old_entry), 0x25CF);

    GtkWidget *new_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(new_entry), "Mật khẩu mới");
    gtk_entry_set_visibility(GTK_ENTRY(new_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(new_entry), 0x25CF);

    GtkWidget *confirm_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(confirm_entry), "Xác nhận mật khẩu mới");
    gtk_entry_set_visibility(GTK_ENTRY(confirm_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(confirm_entry), 0x25CF);

    GtkWidget *err_label = gtk_label_new("");
    gtk_widget_set_halign(err_label, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(content), lbl_info,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), old_entry,      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), new_entry,      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), confirm_entry,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), err_label,      FALSE, FALSE, 0);
    gtk_widget_show_all(content);

    while (1) {
        int response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response != GTK_RESPONSE_ACCEPT) break;

        const char *old_pw  = gtk_entry_get_text(GTK_ENTRY(old_entry));
        const char *new_pw  = gtk_entry_get_text(GTK_ENTRY(new_entry));
        const char *conf_pw = gtk_entry_get_text(GTK_ENTRY(confirm_entry));

        if (is_blank(old_pw) || is_blank(new_pw) || is_blank(conf_pw)) {
            gtk_label_set_markup(GTK_LABEL(err_label),
                "<span color='#ff6b6b'>Vui lòng điền đầy đủ các trường</span>");
            continue;
        }
        if (strcmp(new_pw, conf_pw) != 0) {
            gtk_label_set_markup(GTK_LABEL(err_label),
                "<span color='#ff6b6b'>Mật khẩu mới không khớp</span>");
            continue;
        }
        if (strcmp(old_pw, new_pw) == 0) {
            gtk_label_set_markup(GTK_LABEL(err_label),
                "<span color='#ff6b6b'>Mật khẩu mới phải khác mật khẩu cũ</span>");
            continue;
        }

        int rc = change_password("config.txt", app->logged_in_user, old_pw, new_pw);
        if (rc == -1) {
            gtk_label_set_markup(GTK_LABEL(err_label),
                "<span color='#ff6b6b'>Mật khẩu cũ không đúng</span>");
            continue;
        } else if (rc < 0) {
            gtk_label_set_markup(GTK_LABEL(err_label),
                "<span color='#ff6b6b'>Lỗi hệ thống, không thể đổi mật khẩu</span>");
            continue;
        }

        /* Success */
        set_status(app, "Đổi mật khẩu thành công cho tài khoản %s", app->logged_in_user);
        log_audit(app->logged_in_user, "Changed password");
        break;
    }
    gtk_widget_destroy(dialog);
}

static void on_logout_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    /* Reset trạng thái phiên GUI để quay về màn đăng nhập sạch. */
    app->logged_in_user[0] = '\0';
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "login");
    gtk_entry_set_text(GTK_ENTRY(app->login_user_entry), "");
    gtk_entry_set_text(GTK_ENTRY(app->login_pass_entry), "");
    if (app->login_status_label)
        gtk_label_set_text(GTK_LABEL(app->login_status_label), "");
    clear_form(app);
}

static void on_edit_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    char code[MAX_CODE_LEN] = {0};
    char selected_code[MAX_CODE_LEN] = {0};
    char new_code[MAX_CODE_LEN] = {0};
    const char *name         = gtk_entry_get_text(GTK_ENTRY(app->name_entry));
    const char *code_input   = gtk_entry_get_text(GTK_ENTRY(app->code_entry));
    const char *student_class = gtk_entry_get_text(GTK_ENTRY(app->class_entry));
    const char *dob          = gtk_entry_get_text(GTK_ENTRY(app->dob_entry));
    const char *gpa_str      = gtk_entry_get_text(GTK_ENTRY(app->gpa_entry));
    float gpa = -1.0f;
    int has_selected = get_selected_code(app, selected_code, sizeof(selected_code));

    if (!has_selected) {
        if (is_blank(code_input)) {
            set_status(app, "Hay chon mot dong hoac nhap Ma SV de sua");
            return;
        }
        if (!is_valid_code(code_input)) {
            set_status(app, "Mã sinh viên không hợp lệ (chỉ dùng chữ và số)");
            return;
        }
        normalize_code_key(code_input, code, sizeof(code));
    } else {
        strncpy(code, selected_code, sizeof(code) - 1);
        code[sizeof(code) - 1] = '\0';
    }

    if (is_blank(code_input)) {
        strncpy(new_code, code, sizeof(new_code) - 1);
        new_code[sizeof(new_code) - 1] = '\0';
    } else {
        if (!is_valid_code(code_input)) {
            set_status(app, "Mã sinh viên mới không hợp lệ (chỉ dùng chữ và số)");
            return;
        }
        normalize_code_key(code_input, new_code, sizeof(new_code));
    }

    if (!is_blank(student_class) && !is_valid_class(student_class)) {
        set_status(app, "Lớp không hợp lệ (chỉ cho phép chữ, số và khoảng trắng)");
        return;
    }

    if (!is_blank(dob) && !is_valid_dob(dob)) {
        set_status(app, "Ngày sinh phải đúng định dạng dd/mm/yyyy");
        return;
    }

    if (!is_blank(gpa_str)) {
        char *endptr;
        gpa = strtof(gpa_str, &endptr);
        while (endptr && *endptr && isspace((unsigned char)*endptr)) endptr++;
        if (!endptr || endptr == gpa_str || *endptr != '\0' || gpa < 0.0f || gpa > 4.0f) {
            set_status(app, "GPA phải là số thập phân hợp lệ nằm trong khoảng [0.0 - 4.0]");
            return;
        }
    }

    /*
     * GUI edit thao tác trực tiếp trên bản ghi trong RAM.
     * Sau khi cập nhật xong phải vẽ lại bảng vì TreeView không tự đồng bộ.
     */
    for (int i = 0; i < app->count; i++) {
        Student *s = &app->list[i];
        if (strcmp(s->student_code, code) == 0) {
            if (strcmp(new_code, code) != 0) {
                for (int j = 0; j < app->count; j++) {
                    if (j != i && strcmp(app->list[j].student_code, new_code) == 0) {
                        set_status(app, "Mã sinh viên mới bị trùng: %s", new_code);
                        return;
                    }
                }
                strncpy(s->student_code, new_code, MAX_CODE_LEN - 1);
                s->student_code[MAX_CODE_LEN - 1] = '\0';
            }

            if (!is_blank(name)) {
                if (!is_valid_name(name)) {
                    set_status(app, "Họ tên không được chứa số hoặc ký tự đặc biệt");
                    return;
                }
                strncpy(s->raw_name, name, MAX_NAME_LEN - 1);
                s->raw_name[MAX_NAME_LEN - 1] = '\0';
                /* Giữ cùng quy ước chuẩn hóa tên như lúc thêm mới. */
                if (normalize_name_best_effort(name, s->normalized_name, MAX_NAME_LEN) < 0) {
                    set_status(app, "Sua that bai: driver string_norm chua san sang");
                    return;
                }
                if (is_blank(s->normalized_name)) {
                    set_status(app, "Họ tên không hợp lệ sau chuẩn hóa");
                    return;
                }
            }
            if (!is_blank(student_class)) {
                int j = 0;
                for (int k = 0; student_class[k] != '\0' && j < MAX_CLASS_LEN - 1; k++) {
                    if (!isspace((unsigned char)student_class[k]))
                        s->student_class[j++] = (char)toupper((unsigned char)student_class[k]);
                }
                s->student_class[j] = '\0';
            }
            if (!is_blank(dob)) {
                strncpy(s->dob, dob, MAX_DOB_LEN - 1);
                s->dob[MAX_DOB_LEN - 1] = '\0';
            }
            if (!is_blank(gpa_str))
                s->gpa = gpa;

            refresh_main_table(app);
            set_status(app, "Đã cập nhật sinh viên: %s", s->student_code);
            log_audit(app->logged_in_user, "Updated student: %s -> %s", code, s->student_code);
            return;
        }
    }
    set_status(app, "Sua that bai – khong tim thay ma: %s", code);
}

static void on_sort_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    int active = gtk_combo_box_get_active(GTK_COMBO_BOX(app->sort_combo));

    switch (active) {
    case 0: sort_by_name(app->list, app->count);       set_status(app, "Sap xep theo Ten A-Z");        break;
    case 1: sort_by_gpa(app->list, app->count, 1);     set_status(app, "Sap xep GPA tang dan");        break;
    case 2: sort_by_gpa(app->list, app->count, 0);     set_status(app, "Sap xep GPA giam dan");       break;
    default: set_status(app, "Lua chon sap xep khong hop le"); return;
    }
    refresh_main_table(app);
}

static void on_clear_form_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    clear_form(app);
    set_status(app, "Da xoa trang form nhap lieu");
}

/* Fill form fields when a table row is selected */
static void on_row_selected(GtkTreeSelection *sel, gpointer user_data) {
    AppState *app = (AppState *)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *code = NULL, *name = NULL, *cls = NULL, *dob = NULL, *gpa_str = NULL;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    gtk_tree_model_get(model, &iter,
                       COL_CODE,  &code,
                       COL_NAME,  &name,
                       COL_CLASS, &cls,
                       COL_DOB,   &dob,
                       COL_GPA,   &gpa_str,
                       -1);

    gtk_entry_set_text(GTK_ENTRY(app->code_entry),  code    ? code    : "");
    gtk_entry_set_text(GTK_ENTRY(app->name_entry),  name    ? name    : "");
    gtk_entry_set_text(GTK_ENTRY(app->class_entry), cls     ? cls     : "");
    gtk_entry_set_text(GTK_ENTRY(app->dob_entry),   dob     ? dob     : "");
    gtk_entry_set_text(GTK_ENTRY(app->gpa_entry),   gpa_str ? gpa_str : "");

    g_free(code); g_free(name); g_free(cls); g_free(dob); g_free(gpa_str);
}

/* ── Widget factory ────────────────────────────────────────────────────── */

/* Returns a VBox[ label, entry ] and stores the entry in *out_entry. */
static GtkWidget *make_labeled_entry(const char *label_text, GtkWidget **out_entry) {
    GtkWidget *box   = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *label = gtk_label_new(label_text);
    GtkWidget *entry = gtk_entry_new();

    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    *out_entry = entry;
    return box;
}

/* ── Login view ────────────────────────────────────────────────────────── */

static GtkWidget *build_login_view(AppState *app) {
    GtkWidget *root     = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *card     = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    GtkWidget *title    = gtk_label_new("Đăng nhập vào hệ thống");
    GtkWidget *sep      = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *user_row = make_labeled_entry("Tài khoản", &app->login_user_entry);
    GtkWidget *pass_row = make_labeled_entry("Mật khẩu",  &app->login_pass_entry);

    app->login_status_label = gtk_label_new("");
    gtk_widget_set_name(app->login_status_label, "login-status");
    gtk_widget_set_no_show_all(app->login_status_label, TRUE);
    gtk_widget_hide(app->login_status_label);

    GtkWidget *btn = gtk_button_new_with_label("ĐĂNG NHẬP");

    gtk_widget_set_name(card, "login-card");
    gtk_style_context_add_class(gtk_widget_get_style_context(card),  "glass-card");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "login-title");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn),   "primary-btn");
    app->login_btn = btn;  /* save reference for lockout */

    gtk_entry_set_visibility(GTK_ENTRY(app->login_pass_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(app->login_pass_entry), 0x25CF); /* ● */
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->login_user_entry), "admin");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->login_pass_entry), "••••••");

    gtk_box_pack_start(GTK_BOX(card), title,                   FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(card), sep,                     FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(card), user_row,                FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), pass_row,                FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), app->login_status_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), btn,                     FALSE, FALSE, 8);

    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(card, 420, -1);
    gtk_box_pack_start(GTK_BOX(root), card, TRUE, TRUE, 0);

    /* Enter key submits login from either field */
    g_signal_connect(app->login_user_entry, "activate", G_CALLBACK(on_login_clicked), app);
    g_signal_connect(app->login_pass_entry, "activate", G_CALLBACK(on_login_clicked), app);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_login_clicked), app);
    return root;
}

/* ── USB Storage Manager Dialog ─────────────────────────────────────────── */

typedef struct {
    GtkWidget *dialog;
    GtkWidget *status_connected;
    GtkWidget *status_mounted;
    GtkWidget *status_mount_point;
    GtkWidget *status_reads;
    GtkWidget *status_writes;
    GtkWidget *status_bar;
    GtkWidget *file_tree;
    GtkListStore *file_store;
    GtkWidget *file_entry;
    GtkWidget *text_view;
    GtkWidget *log_view;
    GtkTextBuffer *log_buffer;
    char current_mount[256];
    int total_reads;
    int total_writes;
    AppState *app;
} UsbManagerState;

static void usb_mgr_log(UsbManagerState *st, const char *fmt, ...) {
    if (!st->log_buffer) return;
    char msg[512], timestamp[32], full_msg[600];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S]", t);
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    snprintf(full_msg, sizeof(full_msg), "%s %s\n", timestamp, msg);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(st->log_buffer, &end);
    gtk_text_buffer_insert(st->log_buffer, &end, full_msg, -1);
    if (st->log_view) {
        GtkTextMark *mark = gtk_text_buffer_get_insert(st->log_buffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(st->log_view), mark, 0.0, TRUE, 0.0, 1.0);
    }
}

static void usb_mgr_update_status(UsbManagerState *st) {
    char buf[32];
    
    /* Check if stored mount point is still valid */
    int mounted = 0;
    int connected = 0;
    
    if (st->current_mount[0] != '\0') {
        /* Verify mount point still exists and is accessible */
        struct stat st_buf;
        if (stat(st->current_mount, &st_buf) == 0 && S_ISDIR(st_buf.st_mode)) {
            /* Also verify it's still in /proc/mounts */
            UsbMount mounts[MAX_USB_MOUNTS];
            int n = detect_usb_mounts(mounts, MAX_USB_MOUNTS);
            for (int i = 0; i < n; i++) {
                if (strcmp(mounts[i].path, st->current_mount) == 0) {
                    mounted = 1;
                    connected = 1;
                    break;
                }
            }
            if (!mounted) {
                /* Mount point no longer valid, clear it */
                usb_mgr_log(st, "USB disconnected: %s", st->current_mount);
                st->current_mount[0] = '\0';
                gtk_list_store_clear(st->file_store);
            }
        } else {
            /* Directory not accessible, USB removed */
            usb_mgr_log(st, "USB removed: %s", st->current_mount);
            st->current_mount[0] = '\0';
            gtk_list_store_clear(st->file_store);
        }
    }
    
    /* Check for any connected USB even if not mounted by us */
    if (!connected) {
        UsbMount mounts[MAX_USB_MOUNTS];
        int n = detect_usb_mounts(mounts, MAX_USB_MOUNTS);
        connected = (n > 0);
    }
    
    gtk_label_set_text(GTK_LABEL(st->status_connected), connected ? "Yes" : "No");
    gtk_label_set_text(GTK_LABEL(st->status_mounted), mounted ? "Yes" : "No");
    gtk_label_set_text(GTK_LABEL(st->status_mount_point), mounted ? st->current_mount : "N/A");
    snprintf(buf, sizeof(buf), "%d", st->total_reads);
    gtk_label_set_text(GTK_LABEL(st->status_reads), buf);
    snprintf(buf, sizeof(buf), "%d", st->total_writes);
    gtk_label_set_text(GTK_LABEL(st->status_writes), buf);
    
    if (mounted)
        gtk_label_set_markup(GTK_LABEL(st->status_bar), "<span foreground='#4CAF50' font_weight='bold'>● USB Mounted</span>");
    else if (connected)
        gtk_label_set_markup(GTK_LABEL(st->status_bar), "<span foreground='#FFA500' font_weight='bold'>● USB Connected (Not Mounted)</span>");
    else
        gtk_label_set_markup(GTK_LABEL(st->status_bar), "<span foreground='#f44336' font_weight='bold'>● No USB Connected</span>");
}

static void usb_mgr_refresh_files(UsbManagerState *st) {
    gtk_list_store_clear(st->file_store);
    if (st->current_mount[0] == '\0') return;
    
    DIR *dir = opendir(st->current_mount);
    if (!dir) {
        usb_mgr_log(st, "Cannot open: %s", st->current_mount);
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", st->current_mount, entry->d_name);
        struct stat file_stat;
        if (stat(full_path, &file_stat) != 0) continue;
        
        const char *type = S_ISDIR(file_stat.st_mode) ? "📁 Folder" : "📄 File";
        char size_str[32];
        if (S_ISDIR(file_stat.st_mode)) strcpy(size_str, "-");
        else if (file_stat.st_size < 1024) snprintf(size_str, sizeof(size_str), "%ld B", file_stat.st_size);
        else if (file_stat.st_size < 1024*1024) snprintf(size_str, sizeof(size_str), "%.1f KB", file_stat.st_size/1024.0);
        else snprintf(size_str, sizeof(size_str), "%.1f MB", file_stat.st_size/(1024.0*1024.0));
        
        GtkTreeIter iter;
        gtk_list_store_append(st->file_store, &iter);
        gtk_list_store_set(st->file_store, &iter, 0, entry->d_name, 1, type, 2, size_str, -1);
    }
    closedir(dir);
    usb_mgr_log(st, "File list refreshed");
}

static void on_usb_mgr_mount(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    UsbMount devices[MAX_USB_MOUNTS];
    UsbMount candidates[MAX_USB_MOUNTS];
    int total;
    int cand_count = 0;
    int selected = 0;

    if (!usb_driver_available()) {
        usb_mgr_log(st, "USB driver not available");
        return;
    }

    total = detect_usb_devices(devices, MAX_USB_MOUNTS);
    for (int i = 0; i < total && cand_count < MAX_USB_MOUNTS; i++) {
        if (devices[i].path[0] == '\0') {
            candidates[cand_count++] = devices[i];
        }
    }

    if (cand_count == 0) {
        usb_mgr_log(st, "No unmounted USB partition found");
        return;
    }

    if (cand_count > 1) {
        GtkWidget *sel_dialog = gtk_dialog_new_with_buttons("Select USB Partition", GTK_WINDOW(st->dialog),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Cancel", GTK_RESPONSE_CANCEL, "Mount", GTK_RESPONSE_ACCEPT, NULL);
        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(sel_dialog));
        GtkWidget *list_box = gtk_list_box_new();

        for (int i = 0; i < cand_count; i++) {
            char label[512];
            snprintf(label, sizeof(label), "%.200s", candidates[i].device);
            GtkWidget *row = gtk_label_new(label);
            gtk_label_set_xalign(GTK_LABEL(row), 0);
            gtk_widget_set_margin_start(row, 10);
            gtk_widget_set_margin_end(row, 10);
            gtk_widget_set_margin_top(row, 5);
            gtk_widget_set_margin_bottom(row, 5);
            gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);
        }

        gtk_container_add(GTK_CONTAINER(content), list_box);
        gtk_widget_show_all(sel_dialog);

        if (gtk_dialog_run(GTK_DIALOG(sel_dialog)) == GTK_RESPONSE_ACCEPT) {
            GtkListBoxRow *sel = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
            if (!sel) {
                gtk_widget_destroy(sel_dialog);
                usb_mgr_log(st, "Mount cancelled (no partition selected)");
                return;
            }
            selected = gtk_list_box_row_get_index(sel);
        } else {
            gtk_widget_destroy(sel_dialog);
            usb_mgr_log(st, "Mount cancelled");
            return;
        }
        gtk_widget_destroy(sel_dialog);
    }

    {
        const char *device = candidates[selected].device;
        const char *dev_name = strrchr(device, '/');
        char mount_point[256];
        struct stat st_buf;
        int ret;

        if (dev_name)
            dev_name++;
        else
            dev_name = device;

        snprintf(mount_point, sizeof(mount_point), "/tmp/usb_bridge_%.220s", dev_name);

        if (stat(mount_point, &st_buf) == 0) {
            if (!S_ISDIR(st_buf.st_mode)) {
                usb_mgr_log(st, "Cannot use mount point: %s", mount_point);
                return;
            }
        } else if (mkdir(mount_point, 0755) != 0) {
            usb_mgr_log(st, "Cannot create mount point: %s", mount_point);
            return;
        }

        usb_mgr_log(st, "Mounting %s...", device);
        char actual_mp[256] = {0};
        ret = usb_mount_detect(device, mount_point, "auto", "rw",
                               actual_mp, sizeof(actual_mp));
        if (ret == 0) {
            /* Dùng mount point thực tế (udisksctl có thể chọn path khác) */
            const char *used_mp = (actual_mp[0] != '\0') ? actual_mp : mount_point;
            strncpy(st->current_mount, used_mp, sizeof(st->current_mount) - 1);
            st->current_mount[sizeof(st->current_mount) - 1] = '\0';
            usb_mgr_log(st, "Mounted at: %s", st->current_mount);
            log_audit(st->app->logged_in_user, "Mounted USB %s at %s", device, st->current_mount);
            usb_mgr_update_status(st);
            usb_mgr_refresh_files(st);
        } else {
            usb_mgr_log(st, "Mount failed (driver + udisksctl both failed)");
        }
    }
}

static void on_usb_mgr_unmount(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    int managed = 0;
    int ownership_ret;
    int ret;
    const char *mode = "system";
    
    if (st->current_mount[0] == '\0') {
        usb_mgr_log(st, "No USB mounted");
        return;
    }

    ownership_ret = usb_is_driver_managed_mount(st->current_mount, &managed);
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(st->dialog),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "Unmount USB at %s?", st->current_mount);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
        if (ownership_ret == 0 && managed) {
            mode = "driver";
            usb_mgr_log(st, "Unmounting %s via driver...", st->current_mount);
            ret = usb_unmount(st->current_mount);
        } else {
            if (ownership_ret < 0)
                usb_mgr_log(st, "Ownership check unavailable, using system unmount");
            else
                usb_mgr_log(st, "Mount not owned by driver, using system unmount");
            ret = usb_unmount_system(st->current_mount);
        }

        if (ret == 0) {
            log_audit(st->app->logged_in_user, "Unmounted USB at %s via %s", st->current_mount, mode);
            st->current_mount[0] = '\0';
            gtk_list_store_clear(st->file_store);
            usb_mgr_log(st, "Unmounted successfully");
            usb_mgr_update_status(st);
        } else {
            usb_mgr_log(st, "Unmount failed");
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_usb_mgr_detect(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    UsbMount mounts[MAX_USB_MOUNTS];
    int n = detect_usb_mounts(mounts, MAX_USB_MOUNTS);
    
    usb_mgr_log(st, "Detecting USB devices...");
    
    if (n == 0) {
        usb_mgr_log(st, "No mounted USB found");
        return;
    }
    
    if (n == 1) {
        strncpy(st->current_mount, mounts[0].path, sizeof(st->current_mount) - 1);
        usb_mgr_log(st, "Found USB: %s (%s)", mounts[0].path, mounts[0].device);
        usb_mgr_update_status(st);
        usb_mgr_refresh_files(st);
        return;
    }
    
    usb_mgr_log(st, "Found %d USB devices, select one", n);
    GtkWidget *sel_dialog = gtk_dialog_new_with_buttons("Select USB", GTK_WINDOW(st->dialog),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Cancel", GTK_RESPONSE_CANCEL, "Select", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(sel_dialog));
    GtkWidget *list_box = gtk_list_box_new();
    
    for (int i = 0; i < n; i++) {
        char label[512];
        snprintf(label, sizeof(label), "%.200s  (%.200s)", mounts[i].path, mounts[i].device);
        GtkWidget *row = gtk_label_new(label);
        gtk_label_set_xalign(GTK_LABEL(row), 0);
        gtk_widget_set_margin_start(row, 10);
        gtk_widget_set_margin_end(row, 10);
        gtk_widget_set_margin_top(row, 5);
        gtk_widget_set_margin_bottom(row, 5);
        gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);
    }
    
    gtk_container_add(GTK_CONTAINER(content), list_box);
    gtk_widget_show_all(sel_dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(sel_dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkListBoxRow *sel = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
        if (sel) {
            int idx = gtk_list_box_row_get_index(sel);
            if (idx >= 0 && idx < n) {
                strncpy(st->current_mount, mounts[idx].path, sizeof(st->current_mount) - 1);
                usb_mgr_log(st, "Selected: %s", mounts[idx].path);
                usb_mgr_update_status(st);
                usb_mgr_refresh_files(st);
            }
        }
    }
    gtk_widget_destroy(sel_dialog);
}

static void on_usb_mgr_refresh(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    usb_mgr_update_status(st);
    usb_mgr_refresh_files(st);
}

static void on_usb_mgr_file_selected(GtkTreeSelection *sel, gpointer data) {
    UsbManagerState *st = (UsbManagerState *)data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        gchar *filename;
        gtk_tree_model_get(model, &iter, 0, &filename, -1);
        gtk_entry_set_text(GTK_ENTRY(st->file_entry), filename);
        g_free(filename);
    }
}

static void on_usb_mgr_read(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    const char *filename = gtk_entry_get_text(GTK_ENTRY(st->file_entry));
    
    if (!filename || filename[0] == '\0') {
        usb_mgr_log(st, "Please enter or select a filename");
        return;
    }
    if (st->current_mount[0] == '\0') {
        usb_mgr_log(st, "No USB mounted");
        return;
    }
    
    char content[4096];
    usb_mgr_log(st, "Reading %s...", filename);
    if (usb_read_text_file(st->current_mount, filename, content, sizeof(content)) == 0) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st->text_view));
        gtk_text_buffer_set_text(buf, content, -1);
        st->total_reads++;
        usb_mgr_update_status(st);
        usb_mgr_log(st, "File read successfully");
        log_audit(st->app->logged_in_user, "USB Read: %s/%s", st->current_mount, filename);
    } else {
        usb_mgr_log(st, "Read failed");
    }
}

static void on_usb_mgr_write(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    const char *filename = gtk_entry_get_text(GTK_ENTRY(st->file_entry));
    
    if (!filename || filename[0] == '\0') {
        usb_mgr_log(st, "Please enter a filename");
        return;
    }
    if (st->current_mount[0] == '\0') {
        usb_mgr_log(st, "No USB mounted");
        return;
    }
    
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    gchar *content = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    
    usb_mgr_log(st, "Writing %s...", filename);
    if (usb_write_text_file(st->current_mount, filename, content) == 0) {
        st->total_writes++;
        usb_mgr_update_status(st);
        usb_mgr_refresh_files(st);
        usb_mgr_log(st, "File written successfully");
        log_audit(st->app->logged_in_user, "USB Write: %s/%s", st->current_mount, filename);
    } else {
        usb_mgr_log(st, "Write failed");
    }
    g_free(content);
}

static void on_usb_mgr_copy_to(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    
    if (st->current_mount[0] == '\0') {
        usb_mgr_log(st, "No USB mounted");
        return;
    }
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select file to copy", GTK_WINDOW(st->dialog),
        GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text/CSV");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *src = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char *base = g_path_get_basename(src);
        char dst[512];
        snprintf(dst, sizeof(dst), "%s/%s", st->current_mount, base);
        
        usb_mgr_log(st, "Copying %s to USB...", base);
        unsigned int copied = 0;
        if (usb_copy_to_device(src, dst, &copied) == 0) {
            st->total_writes++;
            usb_mgr_update_status(st);
            usb_mgr_refresh_files(st);
            usb_mgr_log(st, "Copied %u bytes", copied);
            log_audit(st->app->logged_in_user, "USB Copy To: %s", base);
        } else {
            usb_mgr_log(st, "Copy failed");
        }
        g_free(base);
        g_free(src);
    }
    gtk_widget_destroy(dialog);
}

static void on_usb_mgr_copy_from(GtkButton *btn, gpointer data) {
    (void)btn;
    UsbManagerState *st = (UsbManagerState *)data;
    const char *filename = gtk_entry_get_text(GTK_ENTRY(st->file_entry));
    
    if (!filename || filename[0] == '\0') {
        usb_mgr_log(st, "Please select a file");
        return;
    }
    if (st->current_mount[0] == '\0') {
        usb_mgr_log(st, "No USB mounted");
        return;
    }
    
    char src[512];
    snprintf(src, sizeof(src), "%s/%s", st->current_mount, filename);
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save to", GTK_WINDOW(st->dialog),
        GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), filename);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *dst = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        usb_mgr_log(st, "Copying %s from USB...", filename);
        unsigned int copied = 0;
        if (usb_copy_from_device(src, dst, &copied) == 0) {
            st->total_reads++;
            usb_mgr_update_status(st);
            usb_mgr_log(st, "Copied %u bytes", copied);
            log_audit(st->app->logged_in_user, "USB Copy From: %s", filename);
        } else {
            usb_mgr_log(st, "Copy failed");
        }
        g_free(dst);
    }
    gtk_widget_destroy(dialog);
}

static void on_open_usb_manager(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    
    UsbManagerState *st = g_new0(UsbManagerState, 1);
    st->app = app;
    
    /* Create dialog */
    st->dialog = gtk_dialog_new_with_buttons("USB Storage Manager", GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(st->dialog), 800, 550);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(st->dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    gtk_box_set_spacing(GTK_BOX(content), 8);
    
    /* Status Frame */
    GtkWidget *status_frame = gtk_frame_new("USB Status");
    GtkWidget *status_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(status_grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(status_grid), 15);
    gtk_container_set_border_width(GTK_CONTAINER(status_grid), 8);
    
    gtk_grid_attach(GTK_GRID(status_grid), gtk_label_new("Driver:"), 0, 0, 1, 1);
    st->status_connected = gtk_label_new("No");
    gtk_grid_attach(GTK_GRID(status_grid), st->status_connected, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(status_grid), gtk_label_new("Mounted:"), 2, 0, 1, 1);
    st->status_mounted = gtk_label_new("No");
    gtk_grid_attach(GTK_GRID(status_grid), st->status_mounted, 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(status_grid), gtk_label_new("Reads:"), 4, 0, 1, 1);
    st->status_reads = gtk_label_new("0");
    gtk_grid_attach(GTK_GRID(status_grid), st->status_reads, 5, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(status_grid), gtk_label_new("Writes:"), 6, 0, 1, 1);
    st->status_writes = gtk_label_new("0");
    gtk_grid_attach(GTK_GRID(status_grid), st->status_writes, 7, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(status_grid), gtk_label_new("Mount Point:"), 0, 1, 1, 1);
    st->status_mount_point = gtk_label_new("N/A");
    gtk_label_set_xalign(GTK_LABEL(st->status_mount_point), 0);
    gtk_grid_attach(GTK_GRID(status_grid), st->status_mount_point, 1, 1, 5, 1);
    st->status_bar = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(st->status_bar), "<span foreground='#f44336' font_weight='bold'>● USB Not Mounted</span>");
    gtk_grid_attach(GTK_GRID(status_grid), st->status_bar, 6, 1, 2, 1);
    gtk_container_add(GTK_CONTAINER(status_frame), status_grid);
    
    /* Controls */
    GtkWidget *ctrl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *btn_mount = gtk_button_new_with_label("🔌 Mount via Driver");
    GtkWidget *btn_unmount = gtk_button_new_with_label("⏏ Unmount");
    GtkWidget *btn_detect = gtk_button_new_with_label("🔍 Detect USB");
    GtkWidget *btn_refresh = gtk_button_new_with_label("🔄 Refresh");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_mount), "primary-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_unmount), "danger-btn");
    gtk_box_pack_start(GTK_BOX(ctrl_box), btn_mount, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_box), btn_unmount, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_box), btn_detect, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctrl_box), btn_refresh, FALSE, FALSE, 0);
    
    /* Main paned: File list | Content */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);
    
    /* Left: File list */
    GtkWidget *files_frame = gtk_frame_new("USB Files");
    GtkWidget *files_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(files_scroll, 220, -1);
    st->file_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    st->file_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st->file_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(st->file_tree), -1, "Name", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(st->file_tree), -1, "Type", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(st->file_tree), -1, "Size", renderer, "text", 2, NULL);
    gtk_container_add(GTK_CONTAINER(files_scroll), st->file_tree);
    gtk_container_add(GTK_CONTAINER(files_frame), files_scroll);
    
    /* Right: Operations + Content + Log */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    
    /* File ops */
    GtkWidget *ops_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *fn_lbl = gtk_label_new("Filename:");
    st->file_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(st->file_entry), "example.txt");
    GtkWidget *btn_read = gtk_button_new_with_label("📖 Read");
    GtkWidget *btn_write = gtk_button_new_with_label("💾 Write");
    GtkWidget *btn_copy_to = gtk_button_new_with_label("📤 Copy To USB");
    GtkWidget *btn_copy_from = gtk_button_new_with_label("📥 Copy From USB");
    gtk_box_pack_start(GTK_BOX(ops_box), fn_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ops_box), st->file_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(ops_box), btn_read, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ops_box), btn_write, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ops_box), btn_copy_to, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ops_box), btn_copy_from, FALSE, FALSE, 0);
    
    /* Text content */
    GtkWidget *text_frame = gtk_frame_new("File Content");
    GtkWidget *text_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(text_scroll, TRUE);
    st->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(st->text_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(text_scroll), st->text_view);
    gtk_container_add(GTK_CONTAINER(text_frame), text_scroll);
    
    /* Log */
    GtkWidget *log_frame = gtk_frame_new("Log Messages");
    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(log_scroll, -1, 100);
    st->log_view = gtk_text_view_new();
    st->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st->log_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(st->log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(st->log_view), FALSE);
    gtk_container_add(GTK_CONTAINER(log_scroll), st->log_view);
    gtk_container_add(GTK_CONTAINER(log_frame), log_scroll);
    
    gtk_box_pack_start(GTK_BOX(right_box), ops_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), text_frame, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), log_frame, FALSE, FALSE, 0);
    
    gtk_paned_pack1(GTK_PANED(paned), files_frame, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), right_box, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 230);
    
    gtk_box_pack_start(GTK_BOX(content), status_frame, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), ctrl_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), paned, TRUE, TRUE, 0);
    
    /* Signals */
    g_signal_connect(btn_mount, "clicked", G_CALLBACK(on_usb_mgr_mount), st);
    g_signal_connect(btn_unmount, "clicked", G_CALLBACK(on_usb_mgr_unmount), st);
    g_signal_connect(btn_detect, "clicked", G_CALLBACK(on_usb_mgr_detect), st);
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_usb_mgr_refresh), st);
    g_signal_connect(btn_read, "clicked", G_CALLBACK(on_usb_mgr_read), st);
    g_signal_connect(btn_write, "clicked", G_CALLBACK(on_usb_mgr_write), st);
    g_signal_connect(btn_copy_to, "clicked", G_CALLBACK(on_usb_mgr_copy_to), st);
    g_signal_connect(btn_copy_from, "clicked", G_CALLBACK(on_usb_mgr_copy_from), st);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(st->file_tree));
    g_signal_connect(sel, "changed", G_CALLBACK(on_usb_mgr_file_selected), st);
    
    /* Init */
    usb_mgr_log(st, "USB Storage Manager opened");
    if (usb_driver_available())
        usb_mgr_log(st, "USB driver connected");
    else
        usb_mgr_log(st, "WARNING: USB driver not available");
    usb_mgr_update_status(st);
    
    gtk_widget_show_all(content);
    gtk_dialog_run(GTK_DIALOG(st->dialog));
    gtk_widget_destroy(st->dialog);
    g_free(st);
}

/* ── User Management Dialog ────────────────────────────────────────────── */

typedef struct {
    GtkWidget *tree;
    GtkListStore *store;
    GtkWidget *user_entry;
    GtkWidget *pass_entry;
    GtkWidget *role_combo;
    AppState *app;
} ManageUsersState;

static void refresh_user_list(ManageUsersState *st) {
    char users[MAX_USERS][MAX_USERNAME];
    char roles[MAX_USERS][MAX_ROLE_LEN];
    /* Đọc lại config.txt để bảng user trong dialog luôn phản ánh dữ liệu mới nhất. */
    int count = list_users("config.txt", users, roles, MAX_USERS);
    gtk_list_store_clear(st->store);
    for (int i = 0; i < count; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(st->store, &iter);
        gtk_list_store_set(st->store, &iter, 0, users[i], 1, roles[i], -1);
    }
}

static void on_mu_add_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    ManageUsersState *st = (ManageUsersState *)data;
    const char *u = gtk_entry_get_text(GTK_ENTRY(st->user_entry));
    const char *p = gtk_entry_get_text(GTK_ENTRY(st->pass_entry));
    char *r = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(st->role_combo));
    /* Hộp thoại này là lớp GUI mỏng; validate sâu hơn nằm trong auth.c. */
    if (u[0] && p[0] && r) {
        add_user("config.txt", u, p, r);
        log_audit(st->app->logged_in_user, "Created user: %s (Role: %s)", u, r);
        refresh_user_list(st);
        gtk_entry_set_text(GTK_ENTRY(st->user_entry), "");
        gtk_entry_set_text(GTK_ENTRY(st->pass_entry), "");
        g_free(r);
    }
}

static void on_mu_del_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    ManageUsersState *st = (ManageUsersState *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(st->tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        gchar *u;
        gtk_tree_model_get(model, &iter, 0, &u, -1);
        /* Không cho xóa chính tài khoản đang đăng nhập để tránh tự khóa phiên hiện tại. */
        if (strcmp(u, st->app->logged_in_user) != 0) {
            delete_user("config.txt", u);
            log_audit(st->app->logged_in_user, "Deleted user: %s", u);
            refresh_user_list(st);
        }
        g_free(u);
    }
}

static void on_mu_reset_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    ManageUsersState *st = (ManageUsersState *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(st->tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        gchar *u;
        gtk_tree_model_get(model, &iter, 0, &u, -1);
        const char *new_p = gtk_entry_get_text(GTK_ENTRY(st->pass_entry));
        
        if (new_p[0]) {
            int ret = admin_reset_password("config.txt", u, new_p);
            if (ret == 0) {
                log_audit(st->app->logged_in_user, "Reset password for user: %s", u);
                refresh_user_list(st);
            }
        }
        g_free(u);
    }
}

static void on_manage_users_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Quản lý tài khoản (Admin)",
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Đóng", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 320);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    ManageUsersState *st = g_new0(ManageUsersState, 1);
    st->app = app;
    st->store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    st->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st->store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(st->tree), -1, "Tài khoản", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(st->tree), -1, "Phân quyền", renderer, "text", 1, NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), st->tree);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    st->user_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(st->user_entry), "Tài khoản mới");
    st->pass_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(st->pass_entry), "Mật khẩu");
    gtk_entry_set_visibility(GTK_ENTRY(st->pass_entry), FALSE);
    st->role_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->role_combo), ROLE_VIEWER);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->role_combo), ROLE_ADMIN);
    gtk_combo_box_set_active(GTK_COMBO_BOX(st->role_combo), 0);

    GtkWidget *btn_add = gtk_button_new_with_label("Thêm");
    GtkWidget *btn_reset = gtk_button_new_with_label("Đổi pass");
    GtkWidget *btn_del = gtk_button_new_with_label("Xóa chọn");

    gtk_style_context_add_class(gtk_widget_get_style_context(btn_add), "primary-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_reset), "secondary-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_del), "danger-btn");

    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_mu_add_clicked), st);
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_mu_reset_clicked), st);
    g_signal_connect(btn_del, "clicked", G_CALLBACK(on_mu_del_clicked), st);

    gtk_box_pack_start(GTK_BOX(hbox), st->user_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), st->pass_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), st->role_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_add, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_reset, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_del, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(content), hbox, FALSE, FALSE, 0);

    refresh_user_list(st);
    gtk_widget_show_all(content);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(st);
}

/* ── Dashboard view ────────────────────────────────────────────────────── */

static GtkWidget *build_dashboard_view(AppState *app) {

    /* ── Outer shell: sidebar | content ──────────────────────────────── */
    GtkWidget *shell = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(shell, "app-shell");

    /* ════════ SIDEBAR ════════ */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(sidebar, "sidebar");

    GtkWidget *brand = gtk_label_new("QLSV");
    gtk_style_context_add_class(gtk_widget_get_style_context(brand), "sidebar-brand");
    gtk_widget_set_halign(brand, GTK_ALIGN_START);

    GtkWidget *brand_sub = gtk_label_new("Student Manager");
    gtk_style_context_add_class(gtk_widget_get_style_context(brand_sub), "sidebar-sub");
    gtk_widget_set_halign(brand_sub, GTK_ALIGN_START);

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    GtkWidget *nav_label = gtk_label_new("QUẢN LÝ");
    gtk_style_context_add_class(gtk_widget_get_style_context(nav_label), "nav-section-label");
    gtk_widget_set_halign(nav_label, GTK_ALIGN_START);

    GtkWidget *btn_list = gtk_button_new_with_label("☰  Tất cả SV");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_list), "side-action");
    GtkWidget *btn_clear_form = gtk_button_new_with_label("✕  Xóa form");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_clear_form), "side-action");

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    GtkWidget *sort_label = gtk_label_new("SẮP XẾP");
    gtk_style_context_add_class(gtk_widget_get_style_context(sort_label), "nav-section-label");
    gtk_widget_set_halign(sort_label, GTK_ALIGN_START);

    app->sort_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->sort_combo), "Tên A-Z");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->sort_combo), "GPA tăng dần");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->sort_combo), "GPA giảm dần");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->sort_combo), 0);
    GtkWidget *btn_sort = gtk_button_new_with_label("⇅  Sắp xếp");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_sort), "side-action");

    GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    GtkWidget *file_label = gtk_label_new("TỆP DỮ LIỆU");
    gtk_style_context_add_class(gtk_widget_get_style_context(file_label), "nav-section-label");
    gtk_widget_set_halign(file_label, GTK_ALIGN_START);

    GtkWidget *file_lbox = make_labeled_entry("ĐƯỜNG DẪN", &app->file_entry);
    gtk_entry_set_text(GTK_ENTRY(app->file_entry), "students.txt");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->file_entry), "students.txt");

    GtkWidget *btn_browse_file = gtk_button_new_with_label("📂  Chọn file");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_browse_file), "side-action");
    app->btn_save = gtk_button_new_with_label("💾  Lưu file");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_save), "side-action");
    GtkWidget *btn_load = gtk_button_new_with_label("📥  Tải file");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_load), "side-action");

    /* Spacer to push logout to bottom */
    GtkWidget *sidebar_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(sidebar_spacer, TRUE);

    GtkWidget *sep4 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    app->btn_manage_users = gtk_button_new_with_label("👥 Quản lý tài khoản");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_manage_users), "side-action");
    /* Hidden by default until login confirms admin role */
    gtk_widget_set_no_show_all(app->btn_manage_users, TRUE);

    GtkWidget *btn_usb_manager = gtk_button_new_with_label("💾 USB Storage Manager");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_usb_manager), "primary-btn");

    GtkWidget *btn_change_pw = gtk_button_new_with_label("🔑  Đổi mật khẩu");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_change_pw), "side-action");
    GtkWidget *btn_logout = gtk_button_new_with_label("⏻  Đăng xuất");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_logout), "danger-btn");

    /* Pack sidebar */
    gtk_box_pack_start(GTK_BOX(sidebar), brand,           FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), brand_sub,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), sep1,            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), nav_label,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_list,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_clear_form,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), sep2,            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), sort_label,      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), app->sort_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_sort,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), sep3,            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), file_label,      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), file_lbox,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_browse_file, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), app->btn_save,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_load,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), sidebar_spacer,  TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(sidebar), sep4,            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_usb_manager, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), app->btn_manage_users, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_change_pw,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), btn_logout,      FALSE, FALSE, 0);

    /* ════════ CONTENT AREA ════════ */
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(content, "content");
    gtk_widget_set_hexpand(content, TRUE);

    /* ── Top bar ─────────────────────────────────────────────────────── */
    GtkWidget *topbar       = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *topbar_title = gtk_label_new("Hệ Thống Quản Lý Sinh Viên");
    app->topbar_user_label  = gtk_label_new("");
    GtkWidget *chip_driver  = gtk_label_new("Driver: string_norm");
    GtkWidget *chip_ready   = gtk_label_new("● Sẵn sàng");

    gtk_widget_set_name(topbar, "topbar");
    gtk_style_context_add_class(gtk_widget_get_style_context(topbar_title), "topbar-title");
    gtk_style_context_add_class(gtk_widget_get_style_context(chip_driver),  "chip");
    gtk_style_context_add_class(gtk_widget_get_style_context(chip_ready),   "chip");
    gtk_widget_set_hexpand(topbar_title, TRUE);
    gtk_widget_set_halign(topbar_title, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(topbar), topbar_title, TRUE,  TRUE,  0);
    gtk_box_pack_end  (GTK_BOX(topbar), app->topbar_user_label, FALSE, FALSE, 10);
    gtk_box_pack_end  (GTK_BOX(topbar), chip_ready,   FALSE, FALSE, 0);
    gtk_box_pack_end  (GTK_BOX(topbar), chip_driver,  FALSE, FALSE, 0);

    /* ── Main card (table + inline search) ───────────────────────────── */
    GtkWidget *main_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_card), "panel-card");
    gtk_widget_set_vexpand(main_card, TRUE);

    GtkWidget *title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    app->list_title_label = gtk_label_new("Danh sách sinh viên");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->list_title_label), "section-title");
    gtk_widget_set_halign(app->list_title_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(app->list_title_label, TRUE);

    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    app->search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->search_entry), "🔍 Tìm kiếm sinh viên...");
    gtk_widget_set_size_request(app->search_entry, 240, -1);

    GtkWidget *btn_search = gtk_button_new_with_label("Tìm");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_search), "primary-btn");

    gtk_box_pack_start(GTK_BOX(search_box), app->search_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(search_box), btn_search,        FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(title_row), app->list_title_label, TRUE,  TRUE,  0);
    gtk_box_pack_end  (GTK_BOX(title_row), search_box,            FALSE, FALSE, 0);

    /* Tree view */
    GtkWidget *scrolled  = gtk_scrolled_window_new(NULL, NULL);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_widget_set_vexpand(scrolled, TRUE);

    app->store = gtk_list_store_new(5,
                                    G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING);
    app->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->store));

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app->tree), -1,
        "MÃ SV",    renderer, "text", COL_CODE,  NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app->tree), -1,
        "HỌ TÊN",   renderer, "text", COL_NAME,  NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app->tree), -1,
        "LỚP",      renderer, "text", COL_CLASS, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app->tree), -1,
        "NGÀY SINH",renderer, "text", COL_DOB,   NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app->tree), -1,
        "GPA",      renderer, "text", COL_GPA,   NULL);

    for (int c = 0; c < 5; c++) {
        GtkTreeViewColumn *col = gtk_tree_view_get_column(GTK_TREE_VIEW(app->tree), c);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_min_width(col, c == COL_NAME ? 200 : 90);
    }

    gtk_container_add(GTK_CONTAINER(scrolled), app->tree);

    gtk_box_pack_start(GTK_BOX(main_card), title_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_card), scrolled,  TRUE,  TRUE,  0);

    /* ── Form card ──────────────────────────────────────────────────── */
    GtkWidget *form_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    app->form_card = form_card;
    gtk_style_context_add_class(gtk_widget_get_style_context(form_card), "form-card");

    GtkWidget *form_header = gtk_label_new("THÔNG TIN SINH VIÊN");
    gtk_style_context_add_class(gtk_widget_get_style_context(form_header), "form-section-label");
    gtk_widget_set_halign(form_header, GTK_ALIGN_START);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing   (GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    gtk_grid_attach(GTK_GRID(grid),
        make_labeled_entry("MÃ SINH VIÊN",           &app->code_entry),  0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        make_labeled_entry("HỌ VÀ TÊN",              &app->name_entry),  1, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid),
        make_labeled_entry("LỚP",                    &app->class_entry), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        make_labeled_entry("NGÀY SINH (dd/mm/yyyy)", &app->dob_entry),   4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        make_labeled_entry("GPA (0.0–4.0)",          &app->gpa_entry),   5, 0, 1, 1);

    gtk_entry_set_placeholder_text(GTK_ENTRY(app->code_entry),  "B21DCCN001");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->name_entry),  "Nguyen Van A");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->class_entry), "CT7A");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->dob_entry),   "dd/mm/yyyy");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->gpa_entry),   "0.00");

    GtkWidget *form_btn_row  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->btn_add       = gtk_button_new_with_label("＋ Thêm");
    app->btn_edit      = gtk_button_new_with_label("✎ Sửa");
    app->btn_delete    = gtk_button_new_with_label("✕ Xóa");

    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_add),    "primary-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_edit),   "secondary-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_delete), "danger-btn");

    gtk_box_pack_start(GTK_BOX(form_btn_row), app->btn_add,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_btn_row), app->btn_edit,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_btn_row), app->btn_delete, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(form_card), form_header,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_card), grid,         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_card), form_btn_row, FALSE, FALSE, 0);

    /* ── Export buttons row ─────────────────────────────────────────── */
    GtkWidget *export_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(export_row), "form-card");
    GtkWidget *btn_export = gtk_button_new_with_label("📄 Xuất Text (.txt)");
    GtkWidget *btn_export_csv = gtk_button_new_with_label("📊 Xuất Excel (.csv)");
    app->btn_export_txt = btn_export;
    app->btn_export_csv = btn_export_csv;
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_export_csv), "primary-btn");
    gtk_box_pack_start(GTK_BOX(export_row), btn_export, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(export_row), btn_export_csv, FALSE, FALSE, 0);

    /* ── Status bar ─────────────────────────────────────────────────── */
    app->status_label = gtk_label_new("Sẵn sàng");
    gtk_widget_set_name(app->status_label, "status-label");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(app->status_label, TRUE);

    /* ── Assemble content ───────────────────────────────────────────── */
    gtk_box_pack_start(GTK_BOX(content), topbar,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), main_card,     TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(content), form_card,     FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), export_row,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), app->status_label, FALSE, FALSE, 0);

    /* ── Assemble shell ─────────────────────────────────────────────── */
    gtk_box_pack_start(GTK_BOX(shell), sidebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(shell), content, TRUE,  TRUE,  0);

    /* ── Signal connections ─────────────────────────────────────────── */
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->tree));
    g_signal_connect(selection, "changed", G_CALLBACK(on_row_selected), app);

    g_signal_connect(btn_search,        "clicked",  G_CALLBACK(on_search_clicked), app);
    g_signal_connect(app->search_entry, "activate", G_CALLBACK(on_search_clicked), app);
    g_signal_connect(btn_list,          "clicked",  G_CALLBACK(on_list_clicked),   app);

    g_signal_connect(app->btn_add,    "clicked", G_CALLBACK(on_add_clicked),    app);
    g_signal_connect(app->btn_edit,   "clicked", G_CALLBACK(on_edit_clicked),   app);
    g_signal_connect(app->btn_delete, "clicked", G_CALLBACK(on_delete_clicked), app);
    g_signal_connect(btn_clear_form,  "clicked", G_CALLBACK(on_clear_form_clicked), app);

    g_signal_connect(btn_sort, "clicked", G_CALLBACK(on_sort_clicked), app);

    g_signal_connect(app->btn_save,   "clicked", G_CALLBACK(on_save_clicked),   app);
    g_signal_connect(btn_load,        "clicked", G_CALLBACK(on_load_clicked),   app);
    g_signal_connect(btn_browse_file, "clicked", G_CALLBACK(on_browse_file_clicked), app);

    g_signal_connect(btn_export,           "clicked", G_CALLBACK(on_export_usb_clicked),       app);
    g_signal_connect(btn_export_csv,       "clicked", G_CALLBACK(on_export_csv_clicked),       app);

    g_signal_connect(btn_usb_manager, "clicked", G_CALLBACK(on_open_usb_manager), app);
    g_signal_connect(app->btn_manage_users, "clicked", G_CALLBACK(on_manage_users_clicked), app);
    g_signal_connect(btn_change_pw, "clicked", G_CALLBACK(on_change_password_clicked), app);
    g_signal_connect(btn_logout,    "clicked", G_CALLBACK(on_logout_clicked), app);

    set_status(app, "Mẹo: chọn một dòng trong bảng để tự động điền vào form");
    
    return shell;
}

/* ── CSS loader ────────────────────────────────────────────────────────── */

static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkScreen *screen        = gdk_screen_get_default();
    GError    *error         = NULL;

    gtk_css_provider_load_from_path(provider, "style.css", &error);
    if (error) {
        g_printerr("Cannot load style.css: %s\n", error->message);
        g_error_free(error);
    } else {
        gtk_style_context_add_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(provider);
}

/* ── main ──────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    AppState app;
    memset(&app, 0, sizeof(app));

    gtk_init(&argc, &argv);

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "Hệ thống quản lý sinh viên");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 980, 680);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    load_css();

    app.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app.stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(app.stack), 300);

    gtk_stack_add_named(GTK_STACK(app.stack), build_login_view(&app),     "login");
    gtk_stack_add_named(GTK_STACK(app.stack), build_dashboard_view(&app), "dashboard");
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "login");

    gtk_container_add(GTK_CONTAINER(app.window), app.stack);

    load_from_file("students.txt", app.list, &app.count);
    refresh_main_table(&app);

    gtk_widget_show_all(app.window);
    gtk_main();

    save_to_file("students.txt", app.list, app.count);
    return 0;
}
