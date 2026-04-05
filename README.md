# Student Management System with Linux Drivers

Dự án C trên Linux gồm 3 phần chạy cùng nhau:

- `kernel_module/`: kernel module tạo thiết bị ký tự `/dev/string_norm` để chuẩn hóa chuỗi họ tên.
- `userspace_app/`: ứng dụng quản lý sinh viên ở cả chế độ CLI và GUI GTK3.
- `usb_driver/`: USB mass-storage monitor demo để ghi log khi cắm/rút thiết bị USB.

Ngoài ra repo có `tests/` để kiểm thử logic userspace và kiểm thử tích hợp với driver chuẩn hóa chuỗi.

## 1. Mục tiêu dự án

Hệ thống minh họa cách kết hợp:

- kernel module trong Linux
- ứng dụng userspace viết bằng C
- xác thực bằng SHA-256 với OpenSSL
- GUI GTK3
- thao tác file thường và file trên USB mount path

Luồng chính của bài toán:

1. Người dùng đăng nhập từ CLI hoặc GUI.
2. Khi thêm/sửa sinh viên, họ tên được gửi xuống `/dev/string_norm`.
3. Kernel module chuẩn hóa chuỗi theo kiểu title case, loại bỏ khoảng trắng thừa.
4. Dữ liệu được lưu trong `students.txt`.

## 2. Cấu trúc thư mục

```text
.
├── kernel_module/
│   ├── string_norm.c
│   ├── string_norm.h
│   └── Makefile
├── userspace_app/
│   ├── main.c
│   ├── gui_app.c
│   ├── student.c
│   ├── auth.c
│   ├── usb_file.c
│   ├── config.txt
│   ├── students.txt
│   ├── style.css
│   └── Makefile
├── usb_driver/
│   ├── usb_driver.c
│   ├── usb_file_demo.c
│   └── Makefile
├── tests/
│   ├── userspace_tests/
│   ├── integration_tests/
│   └── Makefile
└── README.md
```

## 3. Thành phần chính

### 3.1 `kernel_module/string_norm`

Module này đăng ký character device `/dev/string_norm`.

Chức năng:

- nhận chuỗi từ userspace qua `write()`
- bỏ khoảng trắng đầu/cuối
- gộp nhiều khoảng trắng liên tiếp thành 1 dấu cách
- viết hoa chữ cái đầu mỗi từ, viết thường các ký tự còn lại
- trả chuỗi đã xử lý qua `read()`

Ví dụ:

```text
Input : "   nGuYeN   vAn   aN   "
Output: "Nguyen Van An"
```

### 3.2 `userspace_app`

Ứng dụng quản lý sinh viên có 2 giao diện:

- CLI: `student_manager`
- GUI GTK3: `student_manager_gui`

Chức năng hiện có trong mã nguồn:

- đăng nhập bằng tài khoản lưu trong `config.txt`
- băm mật khẩu bằng SHA-256
- khóa đăng nhập sau 3 lần sai trong 30 giây
- thêm, sửa, xóa, tìm kiếm sinh viên
- sắp xếp theo tên hoặc GPA
- lưu/đọc danh sách sinh viên từ file
- ghi file text ra USB
- đọc file text từ USB
- export danh sách sinh viên ra USB
- đổi mật khẩu trong GUI

Thông tin sinh viên được lưu trong struct:

```c
typedef struct {
    char  student_code[MAX_CODE_LEN];
    char  raw_name[MAX_NAME_LEN];
    char  normalized_name[MAX_NAME_LEN];
    char  student_class[MAX_CLASS_LEN];
    char  dob[MAX_DOB_LEN];
    float gpa;
} Student;
```

### 3.3 `usb_driver`

Đây là module demo cho USB mass-storage.

Chức năng:

- đăng ký `usb_driver`
- bắt sự kiện cắm/rút USB storage
- in ra `dmesg` các thông tin như vendor ID, product ID, hãng, tên thiết bị, serial, bus, address, speed

Repo cũng có `usb_file_demo.c` để thử ghi/đọc một file test trên thư mục mount của USB từ userspace.

## 4. Dữ liệu và định dạng file

### 4.1 `userspace_app/config.txt`

Định dạng:

```text
username:sha256_hash
```

Mẫu đang có trong repo:

- `admin` / mật khẩu `admin`
- `student` / mật khẩu `123`

Lưu ý: mã nguồn không lưu plaintext password, chỉ so sánh SHA-256 hash.

### 4.2 `userspace_app/students.txt`

Định dạng:

```text
# code|name|class|dob|gpa
CT070310|Tran Quoc Aaaaaa|CT7C|27/02/2004|3.00
```

Trong file lưu, trường `name` là tên đã chuẩn hóa.

## 5. Yêu cầu môi trường

Repo này được viết để chạy trên Linux, không dành cho Windows native.

Khuyến nghị:

- Linux kernel 6.x hoặc tương thích
- GCC
- `make`
- `kernel-devel` hoặc kernel headers tương ứng với kernel đang chạy
- `pkg-config`
- GTK+ 3.0 development package
- OpenSSL development package

Ví dụ với CentOS Stream / RHEL:

```bash
sudo dnf install -y gcc make kernel-devel kernel-headers pkg-config gtk3-devel openssl-devel
```

Ví dụ với Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) pkg-config libgtk-3-dev libssl-dev
```

## 6. Build dự án

### 6.1 Build kernel module chuẩn hóa chuỗi

```bash
cd kernel_module
make
```

Load module và tạo device node:

```bash
make load
```

Kiểm tra trạng thái:

```bash
make status
```

Gỡ module:

```bash
make unload
```

### 6.2 Build ứng dụng userspace

```bash
cd userspace_app
make
make student_manager_gui
```

Trong `userspace_app/Makefile`:

- `make` build CLI app `student_manager`
- `make student_manager_gui` build GUI app
- `make run` chạy CLI
- `make run-gui` chạy GUI

### 6.3 Build USB driver demo

```bash
cd usb_driver
make
make demo
```

Load USB driver:

```bash
make load
```

Gỡ USB driver:

```bash
make unload
```

## 7. Cách chạy

### 7.1 Chạy CLI

```bash
cd userspace_app
make run
```

Menu CLI hiện có:

- thêm sinh viên
- xóa sinh viên
- tìm kiếm sinh viên
- liệt kê danh sách
- lưu/đọc file
- ghi file text ra USB
- đọc file text từ USB
- export danh sách ra USB
- sửa sinh viên
- sắp xếp

### 7.2 Chạy GUI

```bash
cd userspace_app
make student_manager_gui
./student_manager_gui
```

GUI dùng GTK3 và `style.css`, có các khu vực:

- màn hình đăng nhập
- bảng danh sách sinh viên
- form thêm/sửa
- thanh bên cho sort, save/load, đổi mật khẩu, logout
- vùng thao tác USB

Đăng nhập mẫu:

- username: `admin`
- password: `admin`

### 7.3 Chạy USB file demo

```bash
cd usb_driver
make demo
./usb_file_demo /run/media/$USER/YOUR_USB_LABEL
```

Demo sẽ:

- kiểm tra mount path
- liệt kê file trong USB
- ghi file test
- đọc lại file test
- kiểm tra nội dung round-trip

## 8. Kiểm thử

### 8.1 Build test

```bash
cd tests
make
```

### 8.2 Chạy từng nhóm test

```bash
make run_mock
make run_auth
make run_student
make run_integration
```

Các nhóm test hiện có:

- `test_normalize_mock.c`: kiểm thử thuật toán chuẩn hóa chuỗi ở userspace, không cần kernel driver
- `test_auth.c`: kiểm tra SHA-256 hash với các giá trị mẫu
- `test_student.c`: unit test cho CRUD, search, sort, save/load, giới hạn `MAX_STUDENTS`
- `test_driver_io.c`: integration test với `/dev/string_norm`

### 8.3 Chạy full test script

```bash
cd tests
sudo make run_all
```

Script `integration_tests/run_all_tests.sh` sẽ:

1. build `string_norm.ko`
2. load driver và tạo `/dev/string_norm`
3. chạy mock test
4. chạy auth test
5. chạy student unit test
6. chạy integration test với driver
7. in log kernel liên quan
8. cleanup

## 9. Một số hành vi kỹ thuật đáng chú ý

- Nếu `userspace_app/student.c` không mở được `/dev/string_norm`, hàm thêm/sửa sinh viên sẽ fallback sang tên gốc thay vì dừng chương trình.
- `save_to_file()` dùng `flock()` để khóa file trong lúc ghi.
- `search_student()` tìm theo `normalized_name`, `raw_name` và `student_code`.
- Khi build unit test cho `student.c`, macro `UNIT_TESTING` được dùng để mock driver.
- GUI tự kiểm tra mount path của USB qua `/proc/mounts` theo chu kỳ.

## 10. Hạn chế hiện tại

- Chuẩn hóa tên chỉ xử lý tốt ký tự ASCII; tên tiếng Việt có dấu chưa được xử lý riêng.
- Kiểm tra ngày sinh mới dừng ở format `dd/mm/yyyy`, chưa validate ngày theo từng tháng hoặc năm nhuận.
- USB driver hiện chỉ log thông tin, chưa cung cấp device file hay thao tác I/O kernel-level.
- Repo đang chứa cả artifact build như `.ko`, binary test, binary app; đây không phải trạng thái tối ưu để phát hành.

## 11. Luồng chạy nhanh đề xuất

```bash
cd kernel_module
make
make load

cd ../userspace_app
make
make student_manager_gui
./student_manager_gui
```

Khi xong:

```bash
cd ../kernel_module
make unload
```

## 12. Gợi ý phát triển tiếp

- bổ sung `.gitignore` để loại artifact build khỏi repo
- chuẩn hóa Unicode cho tên tiếng Việt
- tách phần business logic khỏi UI để test tốt hơn
- thêm validate DOB đầy đủ
- thêm script setup tự động cho Linux lab machine
