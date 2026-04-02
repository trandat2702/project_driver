#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "auth.h"
#include "student.h"
#include "usb_file.h"

#define USB_TEXT_BUF_SIZE 2048

static void clear_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

/* Kiem tra chuoi rong / toan khoang trang */
static int is_blank(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Kiem tra ma sinh vien: chi gom chu cai va so, khong rong */
static int is_valid_code(const char *s) {
    if (!s || s[0] == '\0') return 0;
    while (*s) {
        if (!isalnum((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Kiem tra ngay sinh co dung format dd/mm/yyyy khong */
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

int main(void) {
    Student list[MAX_STUDENTS];
    Student found[MAX_STUDENTS];
    int count = 0;
    int choice;
    float gpa;
    char code[MAX_CODE_LEN];
    char name[MAX_NAME_LEN];
    char student_class[MAX_CLASS_LEN];
    char dob[MAX_DOB_LEN];
    char usb_mount[256];
    char usb_file[128];
    char usb_text[USB_TEXT_BUF_SIZE];

    printf("=== Student Manager ===\n");
    if (!authenticate("config.txt")) {
        printf("Authentication failed. Exiting.\n");
        return 1;
    }

    load_from_file("students.txt", list, &count);

    while (1) {
        printf("\n===== MENU =====\n");
        printf(" 1. Add student\n");
        printf(" 2. Delete student\n");
        printf(" 3. Search student\n");
        printf(" 4. List students\n");
        printf(" 5. Save to file\n");
        printf(" 6. Load from file\n");
        printf(" 7. Write text file to USB\n");
        printf(" 8. Read text file from USB\n");
        printf(" 9. Export students to USB\n");
        printf("10. Edit student\n");
        printf("11. Sort students\n");
        printf(" 0. Exit\n");
        printf("Choice: ");

        if (scanf("%d", &choice) != 1) {
            clear_stdin_line();
            printf("Invalid input.\n");
            continue;
        }
        clear_stdin_line();

        switch (choice) {

        /* ===== 1. ADD STUDENT ===== */
        case 1:
            /* Ma sinh vien - vong lap cho den khi hop le */
            while (1) {
                printf("Ma sinh vien (vd: B21DCCN123): ");
                if (!fgets(code, sizeof(code), stdin)) {
                    printf("Input error.\n");
                    code[0] = '\0';
                    break;
                }
                code[strcspn(code, "\n")] = '\0';
                if (is_valid_code(code)) break;
                printf("Error: Ma SV chi duoc chua chu cai va so, khong duoc de trong.\n");
            }
            if (code[0] == '\0') break;

            /* Ten - vong lap cho den khi hop le */
            while (1) {
                printf("Ho va ten: ");
                if (!fgets(name, sizeof(name), stdin)) {
                    printf("Input error.\n");
                    name[0] = '\0';
                    break;
                }
                name[strcspn(name, "\n")] = '\0';
                if (!is_blank(name)) break;
                printf("Error: Ten khong duoc de trong. Vui long nhap lai.\n");
            }
            if (is_blank(name)) break;

            /* Lop - vong lap cho den khi hop le */
            while (1) {
                printf("Lop (vd: CT7C): ");
                if (!fgets(student_class, sizeof(student_class), stdin)) {
                    printf("Input error.\n");
                    student_class[0] = '\0';
                    break;
                }
                student_class[strcspn(student_class, "\n")] = '\0';
                if (!is_blank(student_class)) break;
                printf("Error: Lop khong duoc de trong. Vui long nhap lai.\n");
            }
            if (is_blank(student_class)) break;

            /* Ngay sinh - vong lap cho den khi dung format */
            while (1) {
                printf("Ngay sinh (dd/mm/yyyy, vd: 15/03/2003): ");
                if (!fgets(dob, sizeof(dob), stdin)) {
                    printf("Input error.\n");
                    dob[0] = '\0';
                    break;
                }
                dob[strcspn(dob, "\n")] = '\0';
                if (is_valid_dob(dob)) break;
                printf("Error: Ngay sinh phai dung format dd/mm/yyyy. Vui long nhap lai.\n");
            }
            if (dob[0] == '\0') break;

            /* GPA - vong lap cho den khi hop le */
            while (1) {
                printf("GPA (0.0 - 4.0): ");
                if (scanf("%f", &gpa) != 1) {
                    clear_stdin_line();
                    printf("Error: GPA khong hop le. Vui long nhap so thuc.\n");
                    continue;
                }
                clear_stdin_line();

                if (gpa >= 0.0f && gpa <= 4.0f) {
                    break;
                }
                printf("Error: GPA phai nam trong khoang 0.0 den 4.0. Vui long nhap lai.\n");
            }

            add_student(list, &count, code, name, student_class, dob, gpa);
            break;

        /* ===== 2. DELETE STUDENT ===== */
        case 2:
            printf("Ma sinh vien can xoa: ");
            if (!fgets(code, sizeof(code), stdin)) {
                printf("Input error.\n");
                break;
            }
            code[strcspn(code, "\n")] = '\0';
            delete_student(list, &count, code);
            break;

        /* ===== 3. SEARCH STUDENT ===== */
        case 3:
            printf("Tu khoa tim kiem (ten hoac ma SV): ");
            if (!fgets(name, sizeof(name), stdin)) {
                printf("Input error.\n");
                break;
            }
            name[strcspn(name, "\n")] = '\0';

            {
                int match_count = search_student(list, count, name, found);
                if (match_count > 0) {
                    printf("Found %d matching student(s):\n", match_count);
                    print_student_list(found, match_count);
                } else {
                    printf("No student matched '%s'\n", name);
                }
            }
            break;

        /* ===== 4. LIST STUDENTS ===== */
        case 4:
            print_student_list(list, count);
            break;

        /* ===== 5. SAVE ===== */
        case 5:
            save_to_file("students.txt", list, count);
            break;

        /* ===== 6. LOAD ===== */
        case 6:
            load_from_file("students.txt", list, &count);
            break;

        /* ===== 7. WRITE USB ===== */
        case 7:
            printf("USB mount path (example /run/media/dat/MY_USB): ");
            if (!fgets(usb_mount, sizeof(usb_mount), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_mount[strcspn(usb_mount, "\n")] = '\0';

            printf("File name (example note.txt): ");
            if (!fgets(usb_file, sizeof(usb_file), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_file[strcspn(usb_file, "\n")] = '\0';

            printf("Text content (single line): ");
            if (!fgets(usb_text, sizeof(usb_text), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_text[strcspn(usb_text, "\n")] = '\0';

            if (usb_write_text_file(usb_mount, usb_file, usb_text) == 0)
                printf("Write to USB succeeded.\n");
            else
                printf("Write to USB failed.\n");
            break;

        /* ===== 8. READ USB ===== */
        case 8:
            printf("USB mount path (example /run/media/dat/MY_USB): ");
            if (!fgets(usb_mount, sizeof(usb_mount), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_mount[strcspn(usb_mount, "\n")] = '\0';

            printf("File name (example note.txt): ");
            if (!fgets(usb_file, sizeof(usb_file), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_file[strcspn(usb_file, "\n")] = '\0';

            if (usb_read_text_file(usb_mount, usb_file,
                                   usb_text, sizeof(usb_text)) == 0)
                printf("File content: %s\n", usb_text);
            else
                printf("Read from USB failed.\n");
            break;

        /* ===== 9. EXPORT USB ===== */
        case 9:
            printf("USB mount path (example /run/media/dat/MY_USB): ");
            if (!fgets(usb_mount, sizeof(usb_mount), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_mount[strcspn(usb_mount, "\n")] = '\0';

            printf("File name (example students_export.txt): ");
            if (!fgets(usb_file, sizeof(usb_file), stdin)) {
                printf("Input error.\n");
                break;
            }
            usb_file[strcspn(usb_file, "\n")] = '\0';

            {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", usb_mount, usb_file);
                if (save_to_file(full_path, list, count) == 0) {
                    printf("Export to USB succeeded: %s\n", full_path);
                } else {
                    printf("Export to USB failed.\n");
                }
            }
            break;

        /* ===== 10. EDIT STUDENT ===== */
        case 10:
            printf("Ma sinh vien can sua: ");
            if (!fgets(code, sizeof(code), stdin)) {
                printf("Input error.\n");
                break;
            }
            code[strcspn(code, "\n")] = '\0';
            edit_student(list, count, code);
            break;

        /* ===== 11. SORT STUDENTS ===== */
        case 11:
            printf("\n--- Sort Options ---\n");
            printf("1. Sort by Name (A-Z)\n");
            printf("2. Sort by GPA (Ascending)\n");
            printf("3. Sort by GPA (Descending)\n");
            printf("Sort choice: ");
            {
                int sort_choice;
                if (scanf("%d", &sort_choice) != 1) {
                    clear_stdin_line();
                    printf("Invalid input.\n");
                    break;
                }
                clear_stdin_line();

                switch (sort_choice) {
                case 1: sort_by_name(list, count); break;
                case 2: sort_by_gpa(list, count, 1); break;
                case 3: sort_by_gpa(list, count, 0); break;
                default: printf("Invalid sort option.\n"); break;
                }
                print_student_list(list, count);
            }
            break;

        /* ===== 0. EXIT ===== */
        case 0:
            save_to_file("students.txt", list, count);
            printf("Bye.\n");
            return 0;

        default:
            printf("Invalid choice.\n");
            break;
        }
    }
}
