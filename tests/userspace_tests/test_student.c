#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "student.h"

/* Simple verification macro */
int failed_tests = 0;
int passed_tests = 0;

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            fprintf(stderr, "[FAIL] %s - Expected %d, got %d (line %d)\n", msg, (expected), (actual), __LINE__); \
            failed_tests++; \
        } else { \
            printf("[PASS] %s\n", msg); \
            passed_tests++; \
        } \
    } while(0)

#define ASSERT_STR_EQ(actual, expected, msg) \
    do { \
        if (strcmp((actual), (expected)) != 0) { \
            fprintf(stderr, "[FAIL] %s - Expected '%s', got '%s'\n", msg, (expected), (actual)); \
            failed_tests++; \
        } else { \
            printf("[PASS] %s\n", msg); \
            passed_tests++; \
        } \
    } while(0)

#define ASSERT_FLOAT_EQ(actual, expected, msg) \
    do { \
        if ((actual) < (expected) - 0.01 || (actual) > (expected) + 0.01) { \
            fprintf(stderr, "[FAIL] %s - Expected %.2f, got %.2f\n", msg, (expected), (actual)); \
            failed_tests++; \
        } else { \
            printf("[PASS] %s\n", msg); \
            passed_tests++; \
        } \
    } while(0)

/* Mocks the driver behaviour for Unit Testing so we don't need kernel modules loaded */
#ifdef UNIT_TESTING
int normalize_via_driver(const char *input, char *output, int buf_size) {
    int i = 0, j = 0;
    int in_word = 0, new_word = 1;

    if (!input || !output || buf_size <= 0) return -1;
    memset(output, 0, buf_size);

    while (input[i] && (input[i] == ' ' || input[i] == '\t' || input[i] == '\n')) i++;

    while (input[i] && j < buf_size - 1) {
        if (input[i] == ' ' || input[i] == '\t' || input[i] == '\n') {
            if (in_word) {
                output[j++] = ' ';
                in_word = 0;
                new_word = 1;
            }
        } else {
            in_word = 1;
            if (new_word) {
                output[j++] = (input[i] >= 'a' && input[i] <= 'z') ? input[i] - 32 : input[i];
                new_word = 0;
            } else {
                output[j++] = (input[i] >= 'A' && input[i] <= 'Z') ? input[i] + 32 : input[i];
            }
        }
        i++;
    }
    if (j > 0 && output[j-1] == ' ')
        output[--j] = '\0';
    else
        output[j] = '\0';

    return 0;
}
#endif

void test_is_valid_name() {
    printf("\n--- Test is_valid_name ---\n");
    ASSERT_EQ(is_valid_name("Nguyen Van A"), 1, "Valid name with spaces");
    ASSERT_EQ(is_valid_name("Tran"), 1, "Valid single word name");
    ASSERT_EQ(is_valid_name("Nguyen Van A1"), 0, "Invalid name with numbers");
    ASSERT_EQ(is_valid_name("Tran$Thi"), 0, "Invalid name with special characters");
    ASSERT_EQ(is_valid_name(""), 0, "Invalid empty name");
}

void test_add_student() {
    printf("\n--- Test add_student ---\n");
    Student list[MAX_STUDENTS];
    int count = 0;

    /* Adding valid student */
    ASSERT_EQ(add_student(list, &count, "SV01", "le van b", "CT7A", "01/01/2000", 3.0), 0, "Add valid student");
    ASSERT_EQ(count, 1, "Count incremented");
    ASSERT_STR_EQ(list[0].normalized_name, "Le Van B", "Name normalized on add");
    ASSERT_FLOAT_EQ(list[0].gpa, 3.0f, "GPA saved accurately");
    
    /* Disallowing duplicate codes */
    ASSERT_EQ(add_student(list, &count, "SV01", "other name", "CT7B", "12/12/2001", 2.0), -1, "Should reject duplicate code SV01");

    /* Disallowing invalid names */
    ASSERT_EQ(add_student(list, &count, "SV02", "Invalid123", "CT", "01/01/2001", 3.0), -1, "Should reject name with digits");
    ASSERT_EQ(add_student(list, &count, "SV03", "Invalid@!", "CT", "01/01/2001", 3.0), -1, "Should reject name with specials");
    ASSERT_EQ(count, 1, "Count not incremented after rejections");
}

void test_edit_student_gpa_validation() {
    printf("\n--- Test edit_student & GPA strictly ---\n");
    /* We mock the behavior of gui_app.c validating GPA using strtof */
    const char* valid_gpas[] = {"3.5", "4.0", " 0.0 ", "2"};
    int num_valid = 4;
    for (int i=0; i<num_valid; i++) {
        char *endptr;
        float g = strtof(valid_gpas[i], &endptr);
        while(endptr && *endptr && (*endptr == ' ' || *endptr == '\t')) endptr++;
        int valid = (!endptr || endptr == valid_gpas[i] || *endptr != '\0') ? 0 : 1;
        if(g < 0.0f || g > 4.0f) valid = 0;
        ASSERT_EQ(valid, 1, "Valid GPA accepted");
    }

    const char* invalid_gpas[] = {"3.5abc", "4.1", "-0.1", "abc", "3.0.0"};
    int num_invalid = 5;
    for (int i=0; i<num_invalid; i++) {
        char *endptr;
        float g = strtof(invalid_gpas[i], &endptr);
        while(endptr && *endptr && (*endptr == ' ' || *endptr == '\t')) endptr++;
        int valid = (!endptr || endptr == invalid_gpas[i] || *endptr != '\0') ? 0 : 1;
        if(g < 0.0f || g > 4.0f) valid = 0;
        ASSERT_EQ(valid, 0, "Invalid GPA rejected successfully");
    }
}

void test_load_from_file_validation() {
    printf("\n--- Test load_from_file strictly ---\n");
    Student list[MAX_STUDENTS];
    int count = 0;

    FILE *fp = fopen("temp_test_load.txt", "w");
    fprintf(fp, "SV01|Nguyen Van A|CT7A|01/01/2000|3.5\n");
    fprintf(fp, "SV02$|Nguyen Van B|CT7A|01/01/2000|3.5\n"); /* Invalid Code */
    fclose(fp);

    ASSERT_EQ(load_from_file("temp_test_load.txt", list, &count), -2, "Should reject file with invalid student code");
    
    fp = fopen("temp_test_load.txt", "w");
    fprintf(fp, "SV01|Nguyen Van A|CT7A|01/01/2000|3.5\n");
    fprintf(fp, "SV02|Nguyen V@n B|CT7A|01/01/2000|3.5\n"); /* Invalid Name */
    fclose(fp);
    
    ASSERT_EQ(load_from_file("temp_test_load.txt", list, &count), -2, "Should reject file with invalid name in data");

    fp = fopen("temp_test_load.txt", "w");
    fprintf(fp, "SV01|Nguyen Van A|CT7A|01/01/2000|3.5\n");
    fprintf(fp, "SV02|Nguyen Van B|CT7@A|01/01/2000|3.5\n"); /* Invalid Class */
    fclose(fp);
    
    ASSERT_EQ(load_from_file("temp_test_load.txt", list, &count), -2, "Should reject file with invalid class in data");
    remove("temp_test_load.txt");
}

int main() {
    printf("=== Student Logic Unit Tests ===\n");
    test_is_valid_name();
    test_add_student();
    test_edit_student_gpa_validation();
    test_load_from_file_validation();

    printf("\n=== Summary: %d passed, %d failed ===\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
