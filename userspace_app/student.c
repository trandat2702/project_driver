#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include "student.h"

/*
 * Lớp nghiệp vụ quản lý sinh viên.
 *
 * Đây là nơi nối 3 luồng chính:
 * - CRUD dữ liệu trong bộ nhớ
 * - chuẩn hóa tên thông qua /dev/string_norm
 * - lưu / tải dữ liệu ra file text hoặc CSV
 */

/* ========== Driver communication ========== */

#ifndef UNIT_TESTING
int normalize_via_driver(const char *input, char *output, int buf_size) {
    int fd = open("/dev/string_norm", O_RDWR);
    if (fd < 0) {
        return -1;
    }

    ssize_t w = write(fd, input, strlen(input));
    if (w < 0) { close(fd); return -1; }

    ssize_t r = read(fd, output, buf_size - 1);
    if (r < 0) { close(fd); return -1; }
    output[r] = '\0';

    close(fd);
    return 0;
}
#endif

static void sanitize_name_alpha_space(const char *input, char *output, int buf_size) {
    int out_i = 0;
    int in_word = 0;

    if (!output || buf_size <= 0) return;
    output[0] = '\0';
    if (!input) return;

    while (*input && out_i < buf_size - 1) {
        unsigned char c = (unsigned char)*input;

        if (isspace(c)) {
            if (in_word && out_i < buf_size - 1) {
                output[out_i++] = ' ';
            }
            in_word = 0;
        } else if (isalpha(c)) {
            if (!in_word) {
                output[out_i++] = (char)toupper(c);
                in_word = 1;
            } else {
                output[out_i++] = (char)tolower(c);
            }
        } else {
            /* Drop digits and punctuation so names cannot keep special symbols. */
        }
        input++;
    }

    while (out_i > 0 && output[out_i - 1] == ' ') {
        out_i--;
    }
    output[out_i] = '\0';
}

int normalize_name_best_effort(const char *input, char *output, int buf_size) {
    if (!output || buf_size <= 0) return -1;

    if (normalize_via_driver(input, output, buf_size) == 0) {
        char filtered[MAX_NAME_LEN];
        sanitize_name_alpha_space(output, filtered, sizeof(filtered));
        strncpy(output, filtered, buf_size - 1);
        output[buf_size - 1] = '\0';
        return 0;
    }

    sanitize_name_alpha_space(input, output, buf_size);
    return 0;
}

/* Dùng để phân biệt chuỗi rỗng thật sự với chuỗi chỉ toàn khoảng trắng. */
static int is_blank_string(const char *s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

int is_valid_name(const char *s) {
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isalpha((unsigned char)*s) && !isspace((unsigned char)*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}

static int is_valid_load_code(const char *s) {
    /* Mã sinh viên chỉ chấp nhận chữ và số để tránh ký tự lạ trong file dữ liệu. */
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isalnum((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_load_class(const char *s) {
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isspace((unsigned char)*s) && !isalnum((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_load_dob(const char *s) {
    /*
     * Kiểm tra mức cơ bản:
     * - đúng độ dài dd/mm/yyyy
     * - có dấu '/'
     * - ngày/tháng/năm nằm trong khoảng chấp nhận được
     *
     * Hàm này chưa kiểm tra sâu các trường hợp như 31/02 hay năm nhuận.
     */
    int d, m, y;
    if (!s || strlen(s) != 10) return 0;
    if (s[2] != '/' || s[5] != '/') return 0;
    if (sscanf(s, "%d/%d/%d", &d, &m, &y) != 3) return 0;
    if (d < 1 || d > 31) return 0;
    if (m < 1 || m > 12) return 0;
    if (y < 1900 || y > 2100) return 0;
    return 1;
}

/*
 * Chuẩn hóa các trường định danh do ứng dụng tự xử lý ở user space.
 * Hàm này không đi qua driver; nó dùng cho mã sinh viên và lớp học.
 */

static void normalize_field_upper_no_space(char *dest, const char *src, int max_len) {
    if (!dest || !src) return;
    int j = 0;
    for (int i = 0; src[i] != '\0' && j < max_len - 1; i++) {
        /* Bỏ toàn bộ khoảng trắng và ép về chữ in hoa để thống nhất định dạng khóa. */
        if (!isspace((unsigned char)src[i])) {
            dest[j++] = toupper((unsigned char)src[i]);
        }
    }
    dest[j] = '\0';
}

static void trim_inplace(char *s) {
    char *start;
    char *end;

    if (!s) return;

    start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
}

static void strip_utf8_bom(char *s) {
    if (!s) return;
    if ((unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF) {
        memmove(s, s + 3, strlen(s + 3) + 1);
    }
}

static int split_fixed_fields(char *line, char delimiter, char **fields, int expected) {
    int count = 0;
    char *p = line;

    if (!line || !fields || expected <= 0) return -1;

    fields[count++] = p;
    while (*p) {
        if (*p == delimiter) {
            *p = '\0';
            if (count >= expected) {
                return -1;
            }
            fields[count++] = p + 1;
        }
        p++;
    }

    return (count == expected) ? 0 : -1;
}

/* ========== CRUD ========== */

int add_student(Student *list, int *count, const char *code,
                const char *raw_name, const char *student_class,
                const char *dob, float gpa) {
    /*
     * add_student() chỉ thao tác trên mảng trong RAM.
     * Việc lưu xuống file là trách nhiệm của save_to_file().
     */
    if (*count >= MAX_STUDENTS) {
        printf("ERROR: Student list is full\n");
        return -1;
    }

    /* Mã sinh viên được coi là khóa chính nên không được phép trùng. */
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i].student_code, code) == 0) {
            printf("ERROR: Student code '%s' already exists\n", code);
            return -1;
        }
    }

    Student *s = &list[*count];

    /* Mã sinh viên luôn được đưa về dạng in hoa, không khoảng trắng. */
    normalize_field_upper_no_space(s->student_code, code, MAX_CODE_LEN);

    if (!is_valid_name(raw_name)) {
        printf("ERROR: Name contains invalid characters\n");
        return -1;
    }

    /* Giữ nguyên raw_name để còn phục vụ hiển thị / export khi cần. */
    strncpy(s->raw_name, raw_name, MAX_NAME_LEN - 1);
    s->raw_name[MAX_NAME_LEN - 1] = '\0';

    if (normalize_name_best_effort(raw_name, s->normalized_name, MAX_NAME_LEN) < 0) {
        printf("ERROR: Cannot normalize name via kernel driver\n");
        return -1;
    }

    /* Lớp học cũng được đưa về dạng "không khoảng trắng + in hoa". */
    normalize_field_upper_no_space(s->student_class, student_class, MAX_CLASS_LEN);

    strncpy(s->dob, dob, MAX_DOB_LEN - 1);
    s->dob[MAX_DOB_LEN - 1] = '\0';

    s->gpa = gpa;

    (*count)++;
    printf("Added: [%s] %s -> [%s] Class=%s DOB=%s GPA=%.2f\n",
           code, raw_name, s->normalized_name, student_class, dob, gpa);
    return 0;
}

int delete_student(Student *list, int *count, const char *code) {
    /* Xóa theo cơ chế mảng tĩnh: tìm thấy thì dịch toàn bộ phần đuôi sang trái. */
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i].student_code, code) == 0) {
            /* Dồn mảng sang trái để giữ danh sách liên tục, không có lỗ trống. */
            for (int j = i; j < *count - 1; j++)
                list[j] = list[j + 1];

            (*count)--;
            printf("Deleted student code=%s\n", code);
            return 0;
        }
    }

    printf("Student code=%s not found\n", code);
    return -1;
}

int search_student(Student *list, int count,
                   const char *name, Student *results) {
    int found_count = 0;
    /*
     * Tìm trên cả 3 trường:
     * - normalized_name: để tìm theo tên đã chuẩn hóa
     * - raw_name: để vẫn khớp với dữ liệu gốc người dùng nhập
     * - student_code: để tìm nhanh theo mã sinh viên
     */
    for (int i = 0; i < count; i++) {
        /* strstr() tìm theo kiểu "chuỗi con", nên nhập một phần tên vẫn có thể khớp. */
        if (strstr(list[i].normalized_name, name) ||
            strstr(list[i].raw_name, name) ||
            strstr(list[i].student_code, name)) {
            results[found_count++] = list[i];
        }
    }
    return found_count;
}

/* ========== Edit ========== */

int edit_student(Student *list, int count, const char *code) {
    /*
     * edit_student() phục vụ bản CLI.
     * GUI không gọi hàm này mà tự sửa trực tiếp trên AppState trong gui_app.c.
     */
    int idx = -1;
    char buf[MAX_NAME_LEN];

    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].student_code, code) == 0) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        printf("Student code=%s not found\n", code);
        return -1;
    }

    Student *s = &list[idx];
    printf("\n--- Editing student [%s] ---\n", s->student_code);
    printf("Current name : %s\n", s->normalized_name);
    printf("Current class: %s\n", s->student_class);
    printf("Current DOB  : %s\n", s->dob);
    printf("Current GPA  : %.2f\n", s->gpa);
    printf("(Press Enter to keep current value)\n\n");

    /* Tên mới cũng ưu tiên chạy lại qua driver để giữ định dạng thống nhất. */
    printf("New name [%s]: ", s->normalized_name);
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (!is_blank_string(buf)) {
            if (!is_valid_name(buf)) {
                printf("ERROR: Name contains invalid characters\n");
                return -1;
            }
            strncpy(s->raw_name, buf, MAX_NAME_LEN - 1);
            s->raw_name[MAX_NAME_LEN - 1] = '\0';
            if (normalize_name_best_effort(buf, s->normalized_name, MAX_NAME_LEN) < 0) {
                printf("ERROR: Cannot normalize name via kernel driver\n");
                return -1;
            }
        }
    }

    /* Lớp học được chuẩn hóa ngay tại user space. */
    printf("New class [%s]: ", s->student_class);
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] != '\0') {
            normalize_field_upper_no_space(s->student_class, buf, MAX_CLASS_LEN);
        }
    }

    /* Ngày sinh hiện chỉ thay thế trực tiếp, không chuẩn hóa thêm ngoài độ dài buffer. */
    printf("New DOB [%s]: ", s->dob);
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] != '\0') {
            strncpy(s->dob, buf, MAX_DOB_LEN - 1);
            s->dob[MAX_DOB_LEN - 1] = '\0';
        }
    }

    /*
     * GPA dùng vòng lặp riêng vì đây là trường số dễ nhập sai.
     * Người dùng có thể nhấn Enter để giữ nguyên.
     */
    while (1) {
        printf("New GPA [%.2f]: ", s->gpa);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0') break; /* Giu nguyen */

        char *endptr;
        float new_gpa = strtof(buf, &endptr);
        while (endptr && *endptr && isspace((unsigned char)*endptr)) {
            endptr++;
        }
        if (!endptr || endptr == buf || *endptr != '\0') {
            printf("Error: GPA khong hop le. Vui long nhap so thuc.\n");
            continue;
        }

        if (new_gpa >= 0.0f && new_gpa <= 4.0f) {
            s->gpa = new_gpa;
            break;
        }
        printf("Error: GPA phai nam trong khoang 0.0 den 4.0. Vui long nhap lai.\n");
    }

    printf("Student [%s] updated successfully.\n", s->student_code);
    return 0;
}

/* ========== Sort ========== */

/* qsort() cần callback so sánh riêng cho từng tiêu chí. */

static int cmp_by_name(const void *a, const void *b) {
    /* qsort() truyền vào con trỏ void*, nên phải ép kiểu về Student trước khi so sánh. */
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return strcmp(sa->normalized_name, sb->normalized_name);
}

static int cmp_by_gpa_asc(const void *a, const void *b) {
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    if (sa->gpa < sb->gpa) return -1;
    if (sa->gpa > sb->gpa) return  1;
    return 0;
}

static int cmp_by_gpa_desc(const void *a, const void *b) {
    return cmp_by_gpa_asc(b, a);
}

void sort_by_name(Student *list, int count) {
    /* count <= 1 thì qsort() không cần thiết; return sớm để đơn giản hóa luồng. */
    if (count <= 1) return;
    qsort(list, count, sizeof(Student), cmp_by_name);
    printf("Sorted by name (A-Z).\n");
}

void sort_by_gpa(Student *list, int count, int ascending) {
    /* Chọn callback tăng/giảm ngay tại lời gọi qsort() để tránh lặp code. */
    if (count <= 1) return;
    qsort(list, count, sizeof(Student),
          ascending ? cmp_by_gpa_asc : cmp_by_gpa_desc);
    printf("Sorted by GPA (%s).\n", ascending ? "ascending" : "descending");
}

/* ========== Display ========== */

void print_student_list(Student *list, int count) {
    if (count == 0) {
        printf("  (No students)\n");
        return;
    }

    printf("\n%-12s %-25s %-10s %-12s %-5s\n",
           "Ma SV", "Ten (da chuan hoa)", "Lop", "Ngay sinh", "GPA");
    printf("%-12s %-25s %-10s %-12s %-5s\n",
           "----------", "-------------------------",
           "--------", "----------", "-----");

    for (int i = 0; i < count; i++) {
        printf("%-12s %-25s %-10s %-12s %.2f\n",
               list[i].student_code,
               list[i].normalized_name,
               list[i].student_class,
               list[i].dob,
               list[i].gpa);
    }
    printf("\n");
}

/*
 * Tầng file I/O của nghiệp vụ.
 *
 * File text là "nguồn dữ liệu bền vững" của ứng dụng.
 * Danh sách trong RAM luôn được coi là dữ liệu đang thao tác,
 * còn save/load chịu trách nhiệm đồng bộ RAM <-> file.
 */

int save_to_file(const char *filename, Student *list, int count) {
    /*
     * File lưu theo định dạng:
     *   code|name|class|dob|gpa
     * Dòng đầu tiên là comment/header để người đọc dễ nhận biết cấu trúc.
     */
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    /* flock() giúp tránh ghi chồng khi nhiều tiến trình cùng đụng vào file này. */
    flock(fileno(fp), LOCK_EX);
    fprintf(fp, "# code|name|class|dob|gpa\n");
    for (int i = 0; i < count; i++) {
        /* Lưu normalized_name để lần sau mở lại vẫn hiển thị dữ liệu đã làm sạch. */
        fprintf(fp, "%s|%s|%s|%s|%.2f\n",
                list[i].student_code,
                list[i].normalized_name,
                list[i].student_class,
                list[i].dob,
                list[i].gpa);
    }
    flock(fileno(fp), LOCK_UN);
    fclose(fp);

    printf("Saved %d students to %s\n", count, filename);
    return 0;
}

int load_from_file(const char *filename, Student *list, int *count) {
    /*
     * Hàm này đóng vai trò như "bộ lọc đầu vào" cho dữ liệu file.
     * Thay vì tin tưởng toàn bộ nội dung file, nó chỉ nhận các dòng hợp lệ.
     */
    FILE *fp = fopen(filename, "r");
    char line[512];
    Student temp[MAX_STUDENTS];
    int temp_count = 0;

    if (!fp)
        return -1;

    /*
     * Khi load, danh sách cũ trong RAM bị thay thế hoàn toàn.
     * Đây là lý do GUI phải xác nhận trước khi tải nếu đang có dữ liệu.
     */
    while (fgets(line, sizeof(line), fp)) {
        char *fields[5];
        char delimiter = '\0';
        char code_buf[MAX_CODE_LEN];
        char name_buf[MAX_NAME_LEN];
        char class_buf[MAX_CLASS_LEN];
        char dob_buf[MAX_DOB_LEN];
        float gpa_val;
        char *gpa_end = NULL;

        line[strcspn(line, "\r\n")] = '\0';
        strip_utf8_bom(line);
        trim_inplace(line);

        /* Bỏ qua comment và dòng trống để file có thể đọc được cả khi người dùng ghi chú tay. */
        if (line[0] == '#' || line[0] == '\0')
            continue;

        if (temp_count >= MAX_STUDENTS) {
            fclose(fp);
            return -2;
        }

        if (strchr(line, '|')) {
            delimiter = '|';
        } else if (strchr(line, ',')) {
            delimiter = ',';
        } else {
            fclose(fp);
            return -2;
        }

        if (split_fixed_fields(line, delimiter, fields, 5) != 0) {
            fclose(fp);
            return -2;
        }

        for (int i = 0; i < 5; i++) {
            trim_inplace(fields[i]);
        }

        if (delimiter == ',' && strcasecmp(fields[4], "GPA") == 0) {
            continue;
        }

        if (snprintf(code_buf, sizeof(code_buf), "%s", fields[0]) >= (int)sizeof(code_buf) ||
            snprintf(name_buf, sizeof(name_buf), "%s", fields[1]) >= (int)sizeof(name_buf) ||
            snprintf(class_buf, sizeof(class_buf), "%s", fields[2]) >= (int)sizeof(class_buf) ||
            snprintf(dob_buf, sizeof(dob_buf), "%s", fields[3]) >= (int)sizeof(dob_buf)) {
            fclose(fp);
            return -2;
        }

        gpa_val = strtof(fields[4], &gpa_end);
        while (gpa_end && *gpa_end && isspace((unsigned char)*gpa_end)) {
            gpa_end++;
        }
        if (!gpa_end || gpa_end == fields[4] || *gpa_end != '\0') {
            fclose(fp);
            return -2;
        }

        Student *s = &temp[temp_count];

        /* Strict mode: chỉ cần 1 dòng sai là từ chối toàn bộ file. */
        if (!is_valid_load_code(code_buf) ||
            !is_valid_name(name_buf) ||
            !is_valid_load_class(class_buf) ||
            !is_valid_load_dob(dob_buf) ||
            gpa_val < 0.0f || gpa_val > 4.0f) {
            fclose(fp);
            return -2;
        }

        normalize_field_upper_no_space(s->student_code, code_buf, MAX_CODE_LEN);

        for (int i = 0; i < temp_count; i++) {
            if (strcmp(temp[i].student_code, s->student_code) == 0) {
                fclose(fp);
                return -2;
            }
        }

        strncpy(s->raw_name, name_buf, MAX_NAME_LEN - 1);
        s->raw_name[MAX_NAME_LEN - 1] = '\0';
        if (normalize_name_best_effort(name_buf, s->normalized_name, MAX_NAME_LEN) < 0) {
            fclose(fp);
            return -2;
        }

        normalize_field_upper_no_space(s->student_class, class_buf, MAX_CLASS_LEN);

        strncpy(s->dob, dob_buf, MAX_DOB_LEN - 1);
        s->dob[MAX_DOB_LEN - 1] = '\0';
        s->gpa = gpa_val;
        temp_count++;
    }
    fclose(fp);

    memcpy(list, temp, sizeof(Student) * (size_t)temp_count);
    *count = temp_count;

    printf("Loaded %d students from %s\n", *count, filename);
    return 0;
}

int export_to_csv(const char *filename, Student *list, int count) {
    /*
     * Export CSV khác save_to_file():
     * - save_to_file() phục vụ chương trình đọc lại
     * - export_to_csv() phục vụ con người / Excel xem báo cáo
     */
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    /* Ghi tiêu đề cột (sử dụng format có sẵn chuẩn tiếng việt UTF-8) */
    /* BOM cho Excel nhận diện UTF-8 */
    fprintf(fp, "\xEF\xBB\xBF");
    fprintf(fp, "Mã SV,Họ và Tên,Lớp,Ngày Sinh,GPA\n");

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s,%s,%s,%s,%.2f\n",
                list[i].student_code,
                list[i].raw_name,
                list[i].student_class,
                list[i].dob,
                list[i].gpa);
    }
    fclose(fp);
    return 0;
}
