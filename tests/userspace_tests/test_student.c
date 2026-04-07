#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "student.h"

int normalize_via_driver(const char *input, char *output, int buf_size) {
    (void)input; (void)output; (void)buf_size;
    return -1;
}

static int g_total = 0, g_failed = 0;

#define TEST(name, cond) do { \
    g_total++; \
    if (cond) { printf("[PASS] %s\n", name); } \
    else { printf("[FAIL] %s (line %d)\n", name, __LINE__); g_failed++; } \
} while (0)

#define TEST_STR(name, actual, expected) do { \
    g_total++; \
    if (strcmp((actual), (expected)) == 0) { printf("[PASS] %s\n", name); } \
    else { printf("[FAIL] %s - Expected '%s', Got '%s'\n", name, expected, actual); g_failed++; } \
} while (0)

#define TEST_FLOAT(name, actual, expected) do { \
    g_total++; \
    if ((actual) >= (expected) - 0.01 && (actual) <= (expected) + 0.01) { printf("[PASS] %s\n", name); } \
    else { printf("[FAIL] %s - Expected %.2f, Got %.2f\n", name, expected, actual); g_failed++; } \
} while (0)

int main(void) {
    Student list[MAX_STUDENTS];
    int count = 0;

    printf("=== STUDENT MANAGEMENT TESTS ===\n\n");

    /* --- Name Validation --- */
    printf("--- Name Validation ---\n");

    TEST("Valid name: 'Nguyen Van A'", is_valid_name("Nguyen Van A") == 1);
    TEST("Valid name: 'Tran'", is_valid_name("Tran") == 1);
    TEST("Valid name: 'Le Thi Binh Minh'", is_valid_name("Le Thi Binh Minh") == 1);
    TEST("Valid name: 'A B C'", is_valid_name("A B C") == 1);

    TEST("Invalid: contains digit", is_valid_name("Nguyen Van A1") == 0);
    TEST("Invalid: contains @", is_valid_name("Nguyen@Van") == 0);
    TEST("Invalid: contains #", is_valid_name("Nguyen#Van") == 0);
    TEST("Invalid: contains $", is_valid_name("Tran$Thi") == 0);
    TEST("Invalid: empty string", is_valid_name("") == 0);
    TEST("Invalid: only spaces", is_valid_name("   ") == 0);

    /* --- Add Student --- */
    printf("\n--- Add Student ---\n");

    count = 0;
    TEST("Add valid student", add_student(list, &count, "SV001", "nguyen van an", "CT7A", "01/01/2000", 3.5) == 0);
    TEST("Count = 1", count == 1);
    TEST_STR("Code saved", list[0].student_code, "SV001");
    TEST_STR("Name normalized", list[0].normalized_name, "Nguyen Van An");
    TEST_STR("Class saved", list[0].student_class, "CT7A");
    TEST_STR("DOB saved", list[0].dob, "01/01/2000");
    TEST_FLOAT("GPA saved", list[0].gpa, 3.5f);

    TEST("Add second student", add_student(list, &count, "SV002", "tran thi binh", "CT7B", "15/06/2001", 3.8) == 0);
    TEST("Count = 2", count == 2);

    TEST("Add third student", add_student(list, &count, "SV003", "le van c", "CT7C", "20/12/2002", 3.2) == 0);
    TEST("Count = 3", count == 3);

    TEST("Reject duplicate code", add_student(list, &count, "SV001", "other name", "CT7C", "01/01/2001", 3.0) == -1);
    TEST("Count still 3", count == 3);

    TEST("Reject invalid name (digits)", add_student(list, &count, "SV004", "Nguyen123", "CT7A", "01/01/2001", 3.0) == -1);
    TEST("Reject invalid name (special)", add_student(list, &count, "SV005", "Tran@Thi", "CT7A", "01/01/2001", 3.0) == -1);
    TEST("Count still 3", count == 3);

    TEST("Add with GPA 0.0", add_student(list, &count, "SV006", "le van d", "CT7A", "01/01/2000", 0.0) == 0);
    TEST("Add with GPA 4.0", add_student(list, &count, "SV007", "pham thi e", "CT7A", "01/01/2000", 4.0) == 0);
    TEST("Count = 5", count == 5);

    /* --- Name Normalization --- */
    printf("\n--- Name Normalization on Add ---\n");

    count = 0;
    add_student(list, &count, "T001", "NGUYEN VAN AN", "CT", "01/01/2000", 3.0);
    TEST_STR("Uppercase normalized", list[0].normalized_name, "Nguyen Van An");

    add_student(list, &count, "T002", "  tran   thi   binh  ", "CT", "01/01/2000", 3.0);
    TEST_STR("Spaces normalized", list[1].normalized_name, "Tran Thi Binh");

    add_student(list, &count, "T003", "lE vAn C", "CT", "01/01/2000", 3.0);
    TEST_STR("Mixed case normalized", list[2].normalized_name, "Le Van C");

    /* --- Delete Student --- */
    printf("\n--- Delete Student ---\n");

    count = 0;
    add_student(list, &count, "DEL01", "nguyen van a", "CT", "01/01/2000", 3.0);
    add_student(list, &count, "DEL02", "tran thi b", "CT", "01/01/2000", 3.0);
    add_student(list, &count, "DEL03", "le van c", "CT", "01/01/2000", 3.0);
    TEST("Initial count = 3", count == 3);

    TEST("Delete middle student", delete_student(list, &count, "DEL02") == 0);
    TEST("Count = 2", count == 2);
    TEST_STR("First still exists", list[0].student_code, "DEL01");
    TEST_STR("Last shifted", list[1].student_code, "DEL03");

    TEST("Delete first student", delete_student(list, &count, "DEL01") == 0);
    TEST("Count = 1", count == 1);

    TEST("Delete last student", delete_student(list, &count, "DEL03") == 0);
    TEST("Count = 0", count == 0);

    TEST("Delete nonexist fails", delete_student(list, &count, "DEL99") == -1);

    /* --- Search Student --- */
    printf("\n--- Search Student ---\n");

    count = 0;
    add_student(list, &count, "S001", "nguyen van an", "CT7A", "01/01/2000", 3.5);
    add_student(list, &count, "S002", "tran thi binh", "CT7B", "15/06/2001", 3.8);
    add_student(list, &count, "S003", "le van an", "CT7C", "20/12/2002", 3.2);

    Student results[MAX_STUDENTS];
    int found;

    found = search_student(list, count, "an", results);
    TEST("Search 'an' finds 2", found == 2);

    found = search_student(list, count, "tran", results);
    TEST("Search 'tran' finds 1", found == 1);

    found = search_student(list, count, "S001", results);
    TEST("Search by code finds 1", found == 1);

    found = search_student(list, count, "xyz", results);
    TEST("Search 'xyz' finds 0", found == 0);

    /* --- GPA Validation --- */
    printf("\n--- GPA Validation ---\n");

    const char *valid_gpas[] = {"0.0", "1.5", "2.75", "3.5", "4.0", "0", "4"};
    for (int i = 0; i < 7; i++) {
        char *endptr;
        float g = strtof(valid_gpas[i], &endptr);
        int valid = (endptr != valid_gpas[i] && *endptr == '\0' && g >= 0.0f && g <= 4.0f);
        char msg[64];
        snprintf(msg, sizeof(msg), "Valid GPA: '%s'", valid_gpas[i]);
        TEST(msg, valid == 1);
    }

    const char *invalid_gpas[] = {"4.1", "-0.1", "abc", "3.5x", "", "4.01", "-1"};
    for (int i = 0; i < 7; i++) {
        char *endptr;
        float g = strtof(invalid_gpas[i], &endptr);
        int valid = (endptr != invalid_gpas[i] && *endptr == '\0' && g >= 0.0f && g <= 4.0f);
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid GPA: '%s'", invalid_gpas[i]);
        TEST(msg, valid == 0);
    }

    /* --- File Operations --- */
    printf("\n--- File Operations ---\n");

    const char *test_file = "/tmp/test_students.txt";
    
    count = 0;
    add_student(list, &count, "B21001", "nguyen van a", "CT7A", "01/01/2003", 3.5);
    add_student(list, &count, "B21002", "tran thi b", "CT7B", "15/06/2003", 3.8);
    add_student(list, &count, "B21003", "le van c", "CT7C", "20/12/2003", 3.2);

    TEST("Save to file", save_to_file(test_file, list, count) == 0);

    Student loaded[MAX_STUDENTS];
    int loaded_count = 0;
    TEST("Load from file", load_from_file(test_file, loaded, &loaded_count) == 0);
    TEST("Loaded count matches", loaded_count == count);
    TEST_STR("First student code", loaded[0].student_code, "B21001");
    TEST_STR("First student name", loaded[0].normalized_name, "Nguyen Van A");

    TEST("Load nonexist file fails", load_from_file("/nonexist/file.txt", loaded, &loaded_count) == -1);

    /* Test invalid file content */
    FILE *fp = fopen(test_file, "w");
    fprintf(fp, "VALID01|Nguyen Van A|CT7A|01/01/2000|3.5\n");
    fprintf(fp, "INV@LID|Nguyen Van B|CT7A|01/01/2000|3.5\n");
    fclose(fp);
    TEST("Reject file with invalid code", load_from_file(test_file, loaded, &loaded_count) == -2);

    fp = fopen(test_file, "w");
    fprintf(fp, "VALID01|Nguyen Van A|CT7A|01/01/2000|3.5\n");
    fprintf(fp, "VALID02|Nguyen V@n B|CT7A|01/01/2000|3.5\n");
    fclose(fp);
    TEST("Reject file with invalid name", load_from_file(test_file, loaded, &loaded_count) == -2);

    remove(test_file);

    /* --- Sorting --- */
    printf("\n--- Sorting ---\n");

    count = 0;
    add_student(list, &count, "Z01", "z name", "CT", "01/01/2000", 2.0);
    add_student(list, &count, "A01", "a name", "CT", "01/01/2000", 4.0);
    add_student(list, &count, "M01", "m name", "CT", "01/01/2000", 3.0);

    sort_by_name(list, count);
    TEST_STR("Sort by name: first", list[0].normalized_name, "A Name");
    TEST_STR("Sort by name: last", list[2].normalized_name, "Z Name");

    sort_by_gpa(list, count, 1);
    TEST_FLOAT("Sort by GPA asc: first", list[0].gpa, 2.0f);
    TEST_FLOAT("Sort by GPA asc: last", list[2].gpa, 4.0f);

    sort_by_gpa(list, count, 0);
    TEST_FLOAT("Sort by GPA desc: first", list[0].gpa, 4.0f);
    TEST_FLOAT("Sort by GPA desc: last", list[2].gpa, 2.0f);

    /* --- CSV Export --- */
    printf("\n--- CSV Export ---\n");

    const char *csv_file = "/tmp/test_students.csv";
    count = 0;
    add_student(list, &count, "CSV01", "nguyen van a", "CT7A", "01/01/2003", 3.5);
    add_student(list, &count, "CSV02", "tran thi b", "CT7B", "15/06/2003", 3.8);

    TEST("Export to CSV", export_to_csv(csv_file, list, count) == 0);
    
    FILE *csvfp = fopen(csv_file, "r");
    TEST("CSV file exists", csvfp != NULL);
    if (csvfp) {
        char line[512];
        fgets(line, sizeof(line), csvfp);
        TEST("CSV has BOM or header", strlen(line) > 0);
        fclose(csvfp);
    }
    remove(csv_file);

    /* --- Capacity Test --- */
    printf("\n--- Capacity Test ---\n");

    count = 0;
    for (int i = 0; i < 20; i++) {
        char code[16], name[32];
        snprintf(code, sizeof(code), "CAP%03d", i);
        snprintf(name, sizeof(name), "test name %d", i);
        name[11] = '\0';
        add_student(list, &count, code, "test name", "CT7A", "01/01/2000", 3.0);
    }
    TEST("Added 20 students", count == 20);

    printf("\n=== Results: %d/%d passed ===\n", g_total - g_failed, g_total);
    return g_failed > 0 ? 1 : 0;
}
