#ifndef STUDENT_H
#define STUDENT_H

#define MAX_STUDENTS  100
#define MAX_NAME_LEN  256
#define MAX_CODE_LEN  20
#define MAX_CLASS_LEN 20
#define MAX_DOB_LEN   15

/*
 * Cấu trúc dữ liệu trung tâm của module quản lý sinh viên.
 *
 * Ý nghĩa từng trường:
 * - student_code: mã sinh viên đã được chuẩn hóa, dùng làm khóa nhận diện chính
 * - raw_name: tên gốc người dùng nhập vào, giữ lại để truy vết / export
 * - normalized_name: tên sau khi được chuẩn hóa qua driver hoặc fallback
 * - student_class: lớp đã được chuẩn hóa ở user space
 * - dob: ngày sinh dạng chuỗi dd/mm/yyyy
 * - gpa: điểm trung bình hệ 4
 */
typedef struct {
    char  student_code[MAX_CODE_LEN];
    char  raw_name[MAX_NAME_LEN];
    char  normalized_name[MAX_NAME_LEN];
    char  student_class[MAX_CLASS_LEN];
    char  dob[MAX_DOB_LEN];
    float gpa;
} Student;

/* Gửi chuỗi tên xuống /dev/string_norm để chuẩn hóa trong kernel space. */
int  normalize_via_driver(const char *input, char *output, int buf_size);

/* Thêm một sinh viên mới vào mảng trong RAM sau khi kiểm tra trùng mã. */
int  add_student(Student *list, int *count, const char *code,
                 const char *raw_name, const char *student_class,
                 const char *dob, float gpa);

/* Xóa sinh viên theo mã và dồn lại mảng để không tạo phần tử rỗng. */
int  delete_student(Student *list, int *count, const char *code);

/* Tìm kiếm sinh viên theo tên gốc, tên chuẩn hóa hoặc mã sinh viên. */
int  search_student(Student *list, int count, const char *name,
                    Student *results);

/* In toàn bộ danh sách sinh viên ra stdout theo dạng bảng. */
void print_student_list(Student *list, int count);

/* Ghi toàn bộ danh sách hiện tại từ RAM xuống file text. */
int  save_to_file(const char *filename, Student *list, int count);

/* Đọc file text, lọc dữ liệu lỗi và nạp lại danh sách vào RAM. */
int  load_from_file(const char *filename, Student *list, int *count);

/* Xuất danh sách sang CSV UTF-8 BOM để mở tốt bằng Excel. */
int  export_to_csv(const char *filename, Student *list, int count);

/* Phiên bản chỉnh sửa sinh viên cho giao diện CLI. */
int  edit_student(Student *list, int count, const char *code);

/* Sắp xếp tăng dần theo tên chuẩn hóa. */
void sort_by_name(Student *list, int count);

/* Sắp xếp theo GPA; ascending = 1 là tăng dần, 0 là giảm dần. */
void sort_by_gpa(Student *list, int count, int ascending);

#endif
