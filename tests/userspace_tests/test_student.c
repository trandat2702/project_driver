/*
 * test_student.c - Unit Tests for Student Management System
 *
 * Bao gom 10 nhom test, kiem tra tat ca cac chuc nang:
 *   1. Add student (them moi + chuan hoa ten + luu truong moi)
 *   2. Add duplicate code (chong trung ma SV)
 *   3. Delete student (xoa theo ma SV)
 *   4. Search student (tim theo ten, ma SV)
 *   5. Sort by name (sap xep A-Z tren ten da chuan hoa)
 *   6. Sort by GPA (tang dan + giam dan)
 *   7. Save + Load file (luu va doc lai day du 5 truong)
 *   8. Data integrity after sort+save+load
 *   9. MAX_STUDENTS limit (gioi han toi da)
 *  10. Empty list operations (thao tac tren danh sach rong)
 *
 * Build:
 *   gcc -Wall -g -DUNIT_TESTING -I../userspace_app \
 *       -o test_student test_student.c ../userspace_app/student.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "student.h"

/* ===================================================================
 * Mock: thay the normalize_via_driver() (khong can kernel driver)
 * =================================================================== */

static void normalize_string_mock(const char *input, char *output, int buf_size)
{
      int i = 0, j = 0, in_word = 0, new_word = 1;

      if (!input || !output || buf_size <= 0)
            return;

      memset(output, 0, buf_size);
      while (input[i] && isspace((unsigned char)input[i]))
            i++;

      while (input[i] && j < buf_size - 1) {
            if (isspace((unsigned char)input[i])) {
                  if (in_word) {
                        output[j++] = ' ';
                        in_word = 0;
                        new_word = 1;
                  }
            } else {
                  in_word = 1;
                  if (new_word) {
                        output[j++] = toupper((unsigned char)input[i]);
                        new_word = 0;
                  } else {
                        output[j++] = tolower((unsigned char)input[i]);
                  }
            }
            i++;
      }

      if (j > 0 && output[j - 1] == ' ')
            output[--j] = '\0';
      else
            output[j] = '\0';
}

int normalize_via_driver(const char *input, char *output, int buf_size)
{
      normalize_string_mock(input, output, buf_size);
      return 0;
}

/* ===================================================================
 * Test Framework
 * =================================================================== */

int g_total = 0;
int g_failed = 0;

#define ASSERT(name, condition) do { \
      g_total++; \
      if (condition) { \
            printf("  [PASS] %s\n", name); \
      } else { \
            printf("  [FAIL] %s\n", name); \
            g_failed++; \
      } \
} while (0)

/* ===================================================================
 * TEST 1: Add Student - kiem tra them sinh vien va chuan hoa ten
 * =================================================================== */

static void test_add_student(void)
{
      Student list[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 1] Add Student\n");

      /* Them sinh vien dau tien */
      ASSERT("Add SV001 returns 0",
               add_student(list, &count, "SV001",
                           "  nguyen van an  ", "CNTT1", "15/03/2003", 3.5f) == 0);
      ASSERT("Count is 1", count == 1);

      /* Kiem tra Ma SV (sv 001 -> SV001) */
      ASSERT("student_code == SV001",
               strcmp(list[0].student_code, "SV001") == 0);

      /* Kiem tra chuan hoa ten */
      ASSERT("normalized_name == Nguyen Van An",
               strcmp(list[0].normalized_name, "Nguyen Van An") == 0);

      /* Kiem tra ten goc duoc giu nguyen */
      ASSERT("raw_name preserved: '  nguyen van an  '",
               strcmp(list[0].raw_name, "  nguyen van an  ") == 0);

      /* Kiem tra lop */
      ASSERT("student_class == CNTT1",
               strcmp(list[0].student_class, "CNTT1") == 0);

      /* Kiem tra ngay sinh */
      ASSERT("dob == 15/03/2003",
               strcmp(list[0].dob, "15/03/2003") == 0);

      /* Kiem tra GPA */
      ASSERT("gpa == 3.50",
               list[0].gpa >= 3.49f && list[0].gpa <= 3.51f);

      /* Them sinh vien thu hai voi ten ALL CAPS */
      ASSERT("Add SV002 returns 0",
               add_student(list, &count, "SV002",
                           "TRAN THI BINH", "DTVT2", "20/10/2004", 3.8f) == 0);
      ASSERT("Count is 2", count == 2);
      ASSERT("SV002 normalized: Tran Thi Binh",
               strcmp(list[1].normalized_name, "Tran Thi Binh") == 0);
      ASSERT("SV002 class: DTVT2",
               strcmp(list[1].student_class, "DTVT2") == 0);

      /* Them sinh vien thu ba voi lop can chuan hoa (ct 7 c -> CT7C) */
      ASSERT("Add SV003 returns 0",
               add_student(list, &count, "SV003",
                           "  lE   vAN   c  ", "ct 7 c", "05/07/2002", 2.8f) == 0);
      ASSERT("Count is 3", count == 3);
      ASSERT("SV003 normalized name: Le Van C",
               strcmp(list[2].normalized_name, "Le Van C") == 0);
      ASSERT("SV003 class normalized: CT7C",
               strcmp(list[2].student_class, "CT7C") == 0);
}

/* ===================================================================
 * TEST 2: Duplicate Code - chong trung ma sinh vien
 * =================================================================== */

static void test_add_duplicate(void)
{
      Student list[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 2] Duplicate Student Code\n");

      add_student(list, &count, "SV001", "nguyen van an", "A", "01/01/2000", 3.5f);
      add_student(list, &count, "SV002", "tran thi binh", "B", "02/02/2001", 3.0f);

      /* Them trung ma SV001 */
      ASSERT("Add duplicate SV001 returns -1",
               add_student(list, &count, "SV001",
                           "le van c", "C", "03/03/2002", 2.0f) == -1);
      ASSERT("Count unchanged (still 2)", count == 2);

      /* Them trung ma SV002 */
      ASSERT("Add duplicate SV002 returns -1",
               add_student(list, &count, "SV002",
                           "pham d", "D", "04/04/2003", 1.5f) == -1);
      ASSERT("Count unchanged (still 2)", count == 2);

      /* Them ma moi thi duoc */
      ASSERT("Add SV003 (unique) returns 0",
               add_student(list, &count, "SV003",
                           "hoang e", "E", "05/05/2004", 3.2f) == 0);
      ASSERT("Count is 3", count == 3);
}

/* ===================================================================
 * TEST 3: Delete Student - xoa sinh vien theo ma SV
 * =================================================================== */

static void test_delete_student(void)
{
      Student list[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 3] Delete Student\n");

      add_student(list, &count, "SV001", "nguyen van an", "A", "01/01/2000", 3.5f);
      add_student(list, &count, "SV002", "tran thi binh", "B", "02/02/2001", 3.0f);
      add_student(list, &count, "SV003", "le van c", "C", "03/03/2002", 2.5f);

      /* Xoa sinh vien o giua */
      ASSERT("Delete SV002 returns 0",
               delete_student(list, &count, "SV002") == 0);
      ASSERT("Count is 2", count == 2);

      /* Kiem tra thu tu sau khi xoa */
      ASSERT("list[0] is still SV001",
               strcmp(list[0].student_code, "SV001") == 0);
      ASSERT("list[1] shifted to SV003",
               strcmp(list[1].student_code, "SV003") == 0);

      /* Xoa ma khong ton tai */
      ASSERT("Delete SV099 (not exist) returns -1",
               delete_student(list, &count, "SV099") == -1);
      ASSERT("Count unchanged (still 2)", count == 2);

      /* Xoa sinh vien dau tien */
      ASSERT("Delete SV001 returns 0",
               delete_student(list, &count, "SV001") == 0);
      ASSERT("Count is 1", count == 1);
      ASSERT("list[0] is now SV003",
               strcmp(list[0].student_code, "SV003") == 0);

      /* Xoa sinh vien cuoi cung */
      ASSERT("Delete SV003 returns 0",
               delete_student(list, &count, "SV003") == 0);
      ASSERT("Count is 0 (empty list)", count == 0);
}

/* ===================================================================
 * TEST 4: Search Student - tim kiem theo ten va ma SV
 * =================================================================== */

static void test_search_student(void)
{
      Student list[MAX_STUDENTS];
      Student found[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 4] Search Student\n");

      add_student(list, &count, "SV001", "nguyen van an", "CNTT1", "01/01/2000", 3.5f);
      add_student(list, &count, "SV002", "tran thi binh", "DTVT2", "02/02/2001", 3.8f);
      add_student(list, &count, "SV003", "nguyen thi cam", "CNTT1", "03/03/2002", 3.2f);

      /* Tim theo ten chuan hoa */
      ASSERT("Search 'Nguyen' finds 2 students",
               search_student(list, count, "Nguyen", found) == 2);

      ASSERT("Search 'Tran' finds 1 student",
               search_student(list, count, "Tran", found) == 1);
      ASSERT("Found student is SV002",
               strcmp(found[0].student_code, "SV002") == 0);

      /* Tim theo ma SV */
      ASSERT("Search 'SV001' finds 1 student",
               search_student(list, count, "SV001", found) == 1);
      ASSERT("Found correct student",
               strcmp(found[0].normalized_name, "Nguyen Van An") == 0);

      /* Tim khong co ket qua */
      ASSERT("Search 'xyz' finds 0 students",
               search_student(list, count, "xyz", found) == 0);

      /* Tim chuoi rong - tim thay tat ca */
      ASSERT("Search empty string finds all (3)",
               search_student(list, count, "", found) == 3);
}

/* ===================================================================
 * TEST 5: Sort by Name - sap xep theo ten A-Z
 * =================================================================== */

static void test_sort_by_name(void)
{
      Student list[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 5] Sort by Name (A-Z)\n");

      /* Them theo thu tu ngau nhien */
      add_student(list, &count, "SV003", "tran thi binh", "C", "03/03/2002", 3.0f);
      add_student(list, &count, "SV001", "nguyen van an", "A", "01/01/2000", 3.5f);
      add_student(list, &count, "SV002", "le van c", "B", "02/02/2001", 2.5f);

      sort_by_name(list, count);

      /* Kiem tra thu tu A-Z */
      ASSERT("After sort: [0] = Le Van C",
               strcmp(list[0].normalized_name, "Le Van C") == 0);
      ASSERT("After sort: [1] = Nguyen Van An",
               strcmp(list[1].normalized_name, "Nguyen Van An") == 0);
      ASSERT("After sort: [2] = Tran Thi Binh",
               strcmp(list[2].normalized_name, "Tran Thi Binh") == 0);

      /* Kiem tra ma SV van di theo sinh vien */
      ASSERT("Le Van C retains code SV002",
               strcmp(list[0].student_code, "SV002") == 0);
      ASSERT("Nguyen Van An retains code SV001",
               strcmp(list[1].student_code, "SV001") == 0);
      ASSERT("Tran Thi Binh retains code SV003",
               strcmp(list[2].student_code, "SV003") == 0);
}

/* ===================================================================
 * TEST 6: Sort by GPA - sap xep theo diem tang/giam
 * =================================================================== */

static void test_sort_by_gpa(void)
{
      Student list[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 6] Sort by GPA\n");

      add_student(list, &count, "SV001", "nguyen van an", "A", "01/01/2000", 3.5f);
      add_student(list, &count, "SV002", "tran thi binh", "B", "02/02/2001", 2.0f);
      add_student(list, &count, "SV003", "le van c", "C", "03/03/2002", 3.8f);
      add_student(list, &count, "SV004", "pham d", "D", "04/04/2003", 1.5f);

      /* Sort GPA tang dan */
      sort_by_gpa(list, count, 1);
      ASSERT("GPA asc: [0] = 1.50",
               list[0].gpa >= 1.49f && list[0].gpa <= 1.51f);
      ASSERT("GPA asc: [1] = 2.00",
               list[1].gpa >= 1.99f && list[1].gpa <= 2.01f);
      ASSERT("GPA asc: [2] = 3.50",
               list[2].gpa >= 3.49f && list[2].gpa <= 3.51f);
      ASSERT("GPA asc: [3] = 3.80",
               list[3].gpa >= 3.79f && list[3].gpa <= 3.81f);

      /* Sort GPA giam dan */
      sort_by_gpa(list, count, 0);
      ASSERT("GPA desc: [0] = 3.80",
               list[0].gpa >= 3.79f && list[0].gpa <= 3.81f);
      ASSERT("GPA desc: [1] = 3.50",
               list[1].gpa >= 3.49f && list[1].gpa <= 3.51f);
      ASSERT("GPA desc: [2] = 2.00",
               list[2].gpa >= 1.99f && list[2].gpa <= 2.01f);
      ASSERT("GPA desc: [3] = 1.50",
               list[3].gpa >= 1.49f && list[3].gpa <= 1.51f);

      /* Kiem tra ma SV van dung sau sort */
      ASSERT("GPA desc: [0] is SV003 (3.80)",
               strcmp(list[0].student_code, "SV003") == 0);
      ASSERT("GPA desc: [3] is SV004 (1.50)",
               strcmp(list[3].student_code, "SV004") == 0);
}

/* ===================================================================
 * TEST 7: Save + Load File - luu va doc lai day du du lieu
 * =================================================================== */

static void test_save_load_file(void)
{
      Student list[MAX_STUDENTS];
      Student loaded[MAX_STUDENTS];
      int count = 0;
      int loaded_count = 0;
      const char *tmp_file = "/tmp/test_students_unit.txt";

      printf("\n[TEST 7] Save + Load File\n");

      add_student(list, &count, "SV001", "nguyen van an", "CNTT1", "15/03/2003", 3.5f);
      add_student(list, &count, "SV002", "tran thi binh", "DTVT2", "20/10/2004", 3.8f);
      add_student(list, &count, "SV003", "le van c", "MMT3", "05/07/2002", 2.8f);

      ASSERT("save_to_file returns 0",
               save_to_file(tmp_file, list, count) == 0);

      ASSERT("load_from_file returns 0",
               load_from_file(tmp_file, loaded, &loaded_count) == 0);

      ASSERT("Loaded count == saved count (3)",
               loaded_count == count);

      /* Kiem tra tung sinh vien */
      for (int i = 0; i < count; i++) {
            char desc[128];

            snprintf(desc, sizeof(desc), "SV%d: code matches", i + 1);
            ASSERT(desc, strcmp(loaded[i].student_code,
                                list[i].student_code) == 0);

            snprintf(desc, sizeof(desc), "SV%d: normalized_name matches", i + 1);
            ASSERT(desc, strcmp(loaded[i].normalized_name,
                                list[i].normalized_name) == 0);

            snprintf(desc, sizeof(desc), "SV%d: class matches", i + 1);
            ASSERT(desc, strcmp(loaded[i].student_class,
                                list[i].student_class) == 0);

            snprintf(desc, sizeof(desc), "SV%d: dob matches", i + 1);
            ASSERT(desc, strcmp(loaded[i].dob, list[i].dob) == 0);

            snprintf(desc, sizeof(desc), "SV%d: GPA matches", i + 1);
            ASSERT(desc, loaded[i].gpa >= list[i].gpa - 0.01f &&
                         loaded[i].gpa <= list[i].gpa + 0.01f);
      }

      unlink(tmp_file);
}

/* ===================================================================
 * TEST 8: Data Integrity - sort + save + load van giu nguyen du lieu
 * =================================================================== */

static void test_data_integrity(void)
{
      Student list[MAX_STUDENTS];
      Student loaded[MAX_STUDENTS];
      int count = 0;
      int loaded_count = 0;
      const char *tmp_file = "/tmp/test_integrity.txt";

      printf("\n[TEST 8] Data Integrity (sort -> save -> load)\n");

      add_student(list, &count, "SV003", "tran thi binh", "C", "03/03/2002", 2.5f);
      add_student(list, &count, "SV001", "nguyen van an", "A", "01/01/2000", 3.5f);
      add_student(list, &count, "SV002", "le van c", "B", "02/02/2001", 3.0f);

      /* Sort theo GPA giam dan -> save -> load */
      sort_by_gpa(list, count, 0);
      save_to_file(tmp_file, list, count);
      load_from_file(tmp_file, loaded, &loaded_count);

      ASSERT("Loaded count matches (3)", loaded_count == 3);

      /* Sau sort desc: [0]=3.5, [1]=3.0, [2]=2.5 */
      ASSERT("loaded[0] has highest GPA (SV001)",
               strcmp(loaded[0].student_code, "SV001") == 0);
      ASSERT("loaded[0].gpa == 3.5",
               loaded[0].gpa >= 3.49f && loaded[0].gpa <= 3.51f);

      ASSERT("loaded[2] has lowest GPA (SV003)",
               strcmp(loaded[2].student_code, "SV003") == 0);
      ASSERT("loaded[2].gpa == 2.5",
               loaded[2].gpa >= 2.49f && loaded[2].gpa <= 2.51f);

      /* Kiem tra truong class+dob van nguyen */
      ASSERT("loaded[0].class == A",
               strcmp(loaded[0].student_class, "A") == 0);
      ASSERT("loaded[0].dob == 01/01/2000",
               strcmp(loaded[0].dob, "01/01/2000") == 0);

      unlink(tmp_file);
}

/* ===================================================================
 * TEST 9: MAX_STUDENTS Limit - gioi han danh sach
 * =================================================================== */

static void test_max_students(void)
{
      Student list[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 9] MAX_STUDENTS Limit (%d)\n", MAX_STUDENTS);

      for (int i = 0; i < MAX_STUDENTS; i++) {
            char sv_name[32], sv_code[20];
            snprintf(sv_name, sizeof(sv_name), "student %d", i);
            snprintf(sv_code, sizeof(sv_code), "SV%03d", i);
            add_student(list, &count, sv_code, sv_name,
                        "X", "01/01/2000", 1.0f + (i % 3));
      }

      ASSERT("Added MAX_STUDENTS (100) successfully",
               count == MAX_STUDENTS);

      ASSERT("Adding #101 returns -1 (full)",
               add_student(list, &count, "SVXXX", "overflow",
                           "X", "01/01/2000", 0.0f) == -1);

      ASSERT("Count stays at MAX_STUDENTS",
               count == MAX_STUDENTS);
}

/* ===================================================================
 * TEST 10: Empty List - thao tac tren danh sach rong
 * =================================================================== */

static void test_empty_operations(void)
{
      Student list[MAX_STUDENTS];
      Student found[MAX_STUDENTS];
      int count = 0;

      printf("\n[TEST 10] Empty List Operations\n");

      ASSERT("Search empty list returns 0",
               search_student(list, count, "anyone", found) == 0);

      ASSERT("Delete from empty list returns -1",
               delete_student(list, &count, "SV001") == -1);

      ASSERT("Load non-existent file returns -1",
               load_from_file("/tmp/nonexistent_file_12345.txt",
                              list, &count) == -1);

      /* Sort danh sach rong khong crash */
      sort_by_name(list, count);
      ASSERT("Sort empty list by name: no crash", 1);

      sort_by_gpa(list, count, 1);
      ASSERT("Sort empty list by GPA: no crash", 1);

      /* Sort danh sach 1 phan tu */
      add_student(list, &count, "ONLY", "single student", "Z", "01/01/2000", 4.0f);
      sort_by_name(list, count);
      ASSERT("Sort single item: no crash", 1);
      ASSERT("Single student preserved",
               strcmp(list[0].student_code, "ONLY") == 0);
}

/* ===================================================================
 * MAIN
 * =================================================================== */

int main(void)
{
      printf("============================================\n");
      printf("  Student Management - Full Unit Test Suite  \n");
      printf("============================================\n");

      test_add_student();
      test_add_duplicate();
      test_delete_student();
      test_search_student();
      test_sort_by_name();
      test_sort_by_gpa();
      test_save_load_file();
      test_data_integrity();
      test_max_students();
      test_empty_operations();

      printf("\n============================================\n");
      if (g_failed == 0)
            printf("  ALL PASSED: %d/%d tests \xE2\x9C\x93\n", g_total, g_total);
      else
            printf("  RESULT: %d/%d passed, %d FAILED\n",
                   g_total - g_failed, g_total, g_failed);
      printf("============================================\n");

      return g_failed > 0 ? 1 : 0;
}
