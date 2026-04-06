#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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
/*
 * Giao tiếp tối giản với character driver.
 *
 * Nếu driver đã được nạp, chuỗi đầu vào sẽ đi qua kernel module để chuẩn hóa.
 * Nếu mở device thất bại, hàm trả lỗi để lớp gọi phía trên tự quyết định fallback.
 */
int normalize_via_driver(const char *input, char *output, int buf_size) {
    int fd = open("/dev/string_norm", O_RDWR);
    if (fd < 0) {
        perror("Cannot open /dev/string_norm");
        fprintf(stderr, "Hint: sudo insmod string_norm.ko\n");
        return -1;
    }

    ssize_t w = write(fd, input, strlen(input));
    if (w < 0) { perror("write"); close(fd); return -1; }

    ssize_t r = read(fd, output, buf_size - 1);
    if (r < 0) { perror("read");  close(fd); return -1; }
    output[r] = '\0';

    close(fd);
    return 0;
}
#endif

/* Dùng để phân biệt chuỗi rỗng thật sự với chuỗi chỉ toàn khoảng trắng. */
static int is_blank_string(const char *s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_load_code(const char *s) {
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isalnum((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int is_valid_load_dob(const char *s) {
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
        if (!isspace((unsigned char)src[i])) {
            dest[j++] = toupper((unsigned char)src[i]);
        }
    }
    dest[j] = '\0';
}

/* ========== CRUD ========== */

int add_student(Student *list, int *count, const char *code,
                const char *raw_name, const char *student_class,
                const char *dob, float gpa) {
    if (*count >= MAX_STUDENTS) {
        printf("ERROR: Student list is full\n");
        return -1;
    }

    /* Kiem tra trung Ma sinh vien */
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i].student_code, code) == 0) {
            printf("ERROR: Student code '%s' already exists\n", code);
            return -1;
        }
    }

    Student *s = &list[*count];

    /* Mã sinh viên luôn được đưa về dạng in hoa, không khoảng trắng. */
    normalize_field_upper_no_space(s->student_code, code, MAX_CODE_LEN);

    /* Giữ nguyên raw_name để còn phục vụ hiển thị / export khi cần. */
    strncpy(s->raw_name, raw_name, MAX_NAME_LEN - 1);
    s->raw_name[MAX_NAME_LEN - 1] = '\0';

    /*
     * Tên hiển thị ưu tiên đi qua driver.
     * Nếu driver chưa load hoặc giao tiếp lỗi, hệ thống vẫn tiếp tục hoạt động
     * bằng cách dùng chuỗi gốc như một cơ chế fallback mềm.
     */
    if (normalize_via_driver(raw_name, s->normalized_name, MAX_NAME_LEN) < 0) {
        strncpy(s->normalized_name, raw_name, MAX_NAME_LEN - 1);
        s->normalized_name[MAX_NAME_LEN - 1] = '\0';
    }

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
            strncpy(s->raw_name, buf, MAX_NAME_LEN - 1);
            s->raw_name[MAX_NAME_LEN - 1] = '\0';
            if (normalize_via_driver(buf, s->normalized_name, MAX_NAME_LEN) < 0) {
                strncpy(s->normalized_name, buf, MAX_NAME_LEN - 1);
                s->normalized_name[MAX_NAME_LEN - 1] = '\0';
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

    /* GPA */
    /* GPA - Vong lap cho den khi hop le hoac trong */
    while (1) {
        printf("New GPA [%.2f]: ", s->gpa);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0') break; /* Giu nguyen */

        float new_gpa;
        if (sscanf(buf, "%f", &new_gpa) == 1) {
            if (new_gpa >= 0.0f && new_gpa <= 4.0f) {
                s->gpa = new_gpa;
                break;
            }
            printf("Error: GPA phai nam trong khoang 0.0 den 4.0. Vui long nhap lai.\n");
        } else {
            printf("Error: GPA khong hop le. Vui long nhap so thuc.\n");
        }
    }

    printf("Student [%s] updated successfully.\n", s->student_code);
    return 0;
}

/* ========== Sort ========== */

/* qsort() cần callback so sánh riêng cho từng tiêu chí. */

static int cmp_by_name(const void *a, const void *b) {
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
    if (count <= 1) return;
    qsort(list, count, sizeof(Student), cmp_by_name);
    printf("Sorted by name (A-Z).\n");
}

void sort_by_gpa(Student *list, int count, int ascending) {
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
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    /* flock() giúp tránh ghi chồng khi nhiều tiến trình cùng đụng vào file này. */
    flock(fileno(fp), LOCK_EX);
    fprintf(fp, "# code|name|class|dob|gpa\n");
    for (int i = 0; i < count; i++) {
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
    FILE *fp = fopen(filename, "r");
    char line[512];

    if (!fp)
        return -1;

    /*
     * Khi load, danh sách cũ trong RAM bị thay thế hoàn toàn.
     * Đây là lý do GUI phải xác nhận trước khi tải nếu đang có dữ liệu.
     */
    *count = 0;
    while (fgets(line, sizeof(line), fp) && *count < MAX_STUDENTS) {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        Student *s = &list[*count];
        char code_buf[MAX_CODE_LEN];
        char name_buf[MAX_NAME_LEN];
        char class_buf[MAX_CLASS_LEN];
        char dob_buf[MAX_DOB_LEN];
        float gpa_val;

        if (sscanf(line, "%19[^|]|%255[^|]|%19[^|]|%14[^|]|%f",
                   code_buf, name_buf, class_buf, dob_buf, &gpa_val) == 5) {

            /*
             * 1. Lọc dữ liệu lỗi / bẩn ngay khi nhập từ file.
             *    Dòng nào sai format hoặc GPA/DOB không hợp lệ sẽ bị bỏ qua.
             */
            if (!is_valid_load_code(code_buf)) continue;
            if (is_blank_string(name_buf)) continue;
            if (is_blank_string(class_buf)) continue;
            if (!is_valid_load_dob(dob_buf)) continue;
            if (gpa_val < 0.0f || gpa_val > 4.0f) continue;

            normalize_field_upper_no_space(s->student_code, code_buf, MAX_CODE_LEN);

            /*
             * 2. Chống trùng mã sinh viên ngay trong quá trình load.
             *    Nếu file bị sửa tay và có nhiều dòng cùng mã, chỉ giữ dòng đầu tiên hợp lệ.
             */
            int is_dup = 0;
            for (int i = 0; i < *count; i++) {
                if (strcmp(list[i].student_code, s->student_code) == 0) {
                    is_dup = 1;
                    break;
                }
            }
            if (is_dup) continue; /* Skip malformed duplicate line */

            strncpy(s->normalized_name, name_buf, MAX_NAME_LEN - 1);
            s->normalized_name[MAX_NAME_LEN - 1] = '\0';

            strncpy(s->raw_name, name_buf, MAX_NAME_LEN - 1);
            s->raw_name[MAX_NAME_LEN - 1] = '\0';

            normalize_field_upper_no_space(s->student_class, class_buf, MAX_CLASS_LEN);

            strncpy(s->dob, dob_buf, MAX_DOB_LEN - 1);
            s->dob[MAX_DOB_LEN - 1] = '\0';

            s->gpa = gpa_val;
            (*count)++;
        }
    }
    fclose(fp);

    printf("Loaded %d students from %s\n", *count, filename);
    return 0;
}

int export_to_csv(const char *filename, Student *list, int count) {
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
