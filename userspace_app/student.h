#ifndef STUDENT_H
#define STUDENT_H

#define MAX_STUDENTS  100
#define MAX_NAME_LEN  256
#define MAX_CODE_LEN  20
#define MAX_CLASS_LEN 20
#define MAX_DOB_LEN   15

typedef struct {
    char  student_code[MAX_CODE_LEN];
    char  raw_name[MAX_NAME_LEN];
    char  normalized_name[MAX_NAME_LEN];
    char  student_class[MAX_CLASS_LEN];
    char  dob[MAX_DOB_LEN];
    float gpa;
} Student;

int  normalize_via_driver(const char *input, char *output, int buf_size);
int  add_student(Student *list, int *count, const char *code,
                 const char *raw_name, const char *student_class,
                 const char *dob, float gpa);
int  delete_student(Student *list, int *count, const char *code);
int  search_student(Student *list, int count, const char *name,
                    Student *results);
void print_student_list(Student *list, int count);
int  save_to_file(const char *filename, Student *list, int count);
int  load_from_file(const char *filename, Student *list, int *count);
int  edit_student(Student *list, int count, const char *code);
void sort_by_name(Student *list, int count);
void sort_by_gpa(Student *list, int count, int ascending);

#endif
