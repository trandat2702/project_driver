# Student Management System

Hệ thống quản lý sinh viên viết bằng C trên Linux, gồm 2 phần chạy cùng nhau:

- `kernel_module/`: character device driver `string_norm` để chuẩn hóa chuỗi trong kernel space.
- `userspace_app/`: ứng dụng quản lý sinh viên ở user space, có cả bản CLI và GUI GTK3.

Điểm nổi bật của project là luồng xử lý đi xuyên từ giao diện/nghiệp vụ ở user space xuống `/dev/string_norm`, sau đó trả kết quả đã chuẩn hóa ngược lên ứng dụng.

## 1. Mục tiêu của project

Project này mô phỏng một hệ thống quản lý sinh viên có:

- Đăng nhập bằng mật khẩu băm SHA-256.
- Phân quyền `admin` và `viewer`.
- CRUD sinh viên trong bộ nhớ và lưu xuống file.
- Chuẩn hóa tên sinh viên thông qua Linux kernel driver.
- Nhập/xuất dữ liệu với file text và USB.
- Xuất báo cáo CSV UTF-8 BOM để mở tốt bằng Excel.
- Ghi audit log cho các thao tác quan trọng.
- Bộ test cho cả phần userspace và tích hợp driver.

## 2. Kiến trúc tổng thể

### 2.1. Kernel space

Driver nằm trong [kernel_module/string_norm.c](/d:/project%20driver/kernel_module/string_norm.c).

Driver tạo thiết bị ký tự:

- Device name: `/dev/string_norm`
- Module name: `string_norm`

Luồng hoạt động:

1. Userspace mở `/dev/string_norm`.
2. Userspace `write()` chuỗi tên thô xuống driver.
3. Driver chuẩn hóa chuỗi:
   - bỏ khoảng trắng đầu chuỗi
   - gom nhiều khoảng trắng liên tiếp thành 1 dấu cách
   - viết hoa chữ cái đầu mỗi từ
   - các ký tự còn lại chuyển về chữ thường
4. Userspace `read()` để lấy chuỗi đã chuẩn hóa.

Driver dùng:

- `mutex` để tránh race condition khi `read/write`.
- `copy_from_user` và `copy_to_user` để trao đổi dữ liệu giữa kernel và user space.
- buffer nội bộ kích thước `1024` byte.

### 2.2. User space

Ứng dụng userspace nằm trong thư mục [userspace_app](/d:/project%20driver/userspace_app).

Các module chính:

- [userspace_app/main.c](/d:/project%20driver/userspace_app/main.c): bản CLI, có menu thao tác trực tiếp trong terminal.
- [userspace_app/gui_app.c](/d:/project%20driver/userspace_app/gui_app.c): bản GUI dùng GTK3.
- [userspace_app/student.c](/d:/project%20driver/userspace_app/student.c): nghiệp vụ sinh viên, CRUD, load/save, export CSV, giao tiếp với driver.
- [userspace_app/auth.c](/d:/project%20driver/userspace_app/auth.c): đăng nhập, băm SHA-256, đổi mật khẩu, quản lý user, phân quyền.
- [userspace_app/usb_file.c](/d:/project%20driver/userspace_app/usb_file.c): đọc/ghi file text qua đường dẫn mount USB.
- [userspace_app/audit.c](/d:/project%20driver/userspace_app/audit.c): ghi nhật ký thao tác vào `audit.log`.
- [userspace_app/style.css](/d:/project%20driver/userspace_app/style.css): giao diện cho bản GUI.

## 3. Cấu trúc thư mục

```text
.
├─ kernel_module/
│  ├─ Makefile
│  ├─ string_norm.c
│  └─ string_norm.h
├─ userspace_app/
│  ├─ Makefile
│  ├─ main.c
│  ├─ gui_app.c
│  ├─ auth.c / auth.h
│  ├─ student.c / student.h
│  ├─ usb_file.c / usb_file.h
│  ├─ audit.c / audit.h
│  ├─ config.txt
│  ├─ students.txt
│  ├─ audit.log
│  └─ style.css
├─ tests/
│  ├─ Makefile
│  ├─ userspace_tests/
│  └─ integration_tests/
└─ .agents/workflows/
```

## 4. Yêu cầu môi trường

Project này được viết cho Linux. README cũ nhắc đến CentOS/RHEL và cách build hiện tại cũng theo hướng đó.

Tối thiểu cần:

- Linux có hỗ trợ build kernel module
- `gcc`
- `make`
- `pkg-config`
- `gtk+-3.0`
- `OpenSSL`
- kernel headers / kernel-devel tương ứng với kernel đang chạy

Ví dụ trên CentOS / RHEL:

```bash
sudo dnf install -y gcc make pkg-config gtk3-devel openssl-devel kernel-devel kernel-headers
```

Ví dụ trên Ubuntu / Debian:

```bash
sudo apt install -y build-essential pkg-config libgtk-3-dev libssl-dev linux-headers-$(uname -r)
```

## 5. Build project

### 5.1. Build kernel module

```bash
cd kernel_module
make
```

Các target chính trong [kernel_module/Makefile](/d:/project%20driver/kernel_module/Makefile):

- `make`: build `string_norm.ko`
- `make load`: nạp module và tạo `/dev/string_norm`
- `make unload`: gỡ module và xóa device node
- `make status`: kiểm tra trạng thái module/device/dmesg
- `make clean`: dọn file build

### 5.2. Build ứng dụng userspace

```bash
cd userspace_app
make
```

Target trong [userspace_app/Makefile](/d:/project%20driver/userspace_app/Makefile):

- `make`: build bản CLI `student_manager`
- `make student_manager_gui`: build bản GUI `student_manager_gui`
- `make run`: chạy bản CLI
- `make run-gui`: chạy bản GUI
- `make clean`: xóa binary

### 5.3. Build test

```bash
cd tests
make all
```

## 6. Cách chạy nhanh

### 6.1. Quy trình khuyến nghị

1. Build driver
2. Load driver
3. Build userspace app
4. Chạy GUI hoặc CLI

Ví dụ:

```bash
cd kernel_module
make
sudo make load

cd ../userspace_app
make student_manager_gui
./student_manager_gui
```

### 6.2. Kiểm tra driver đã sẵn sàng chưa

```bash
ls -l /dev/string_norm
```

hoặc:

```bash
cd kernel_module
make status
```

Nếu chưa load driver, phần userspace vẫn có thể hoạt động ở một số chỗ, nhưng chuẩn hóa tên trong `student.c` sẽ fallback về chuỗi gốc thay vì dữ liệu xử lý từ kernel.

## 7. Tài khoản mặc định

File cấu hình hiện tại là [userspace_app/config.txt](/d:/project%20driver/userspace_app/config.txt).

Định dạng mỗi dòng:

```text
username:sha256_hash:role
```

Role hợp lệ:

- `admin`
- `viewer`

Các tài khoản mẫu đang có trong repo:

- `admin` / `admin`
- `student` / `123`
- `dat` / `123`

Lưu ý:

- Ứng dụng không lưu plaintext password.
- Password được băm SHA-256 trong [userspace_app/auth.c](/d:/project%20driver/userspace_app/auth.c).
- Bản CLI và GUI đều có cơ chế khóa đăng nhập 30 giây sau 3 lần sai liên tiếp.

## 8. Phân quyền

Project dùng RBAC mức đơn giản:

- `admin`:
  - thêm sinh viên
  - sửa sinh viên
  - xóa sinh viên
  - lưu dữ liệu
  - quản lý tài khoản trong GUI
  - đổi mật khẩu
- `viewer`:
  - xem danh sách
  - tìm kiếm
  - đọc/ghi file USB theo các nút cho phép trên GUI
  - không được thao tác phá hủy dữ liệu như thêm/sửa/xóa/lưu

Trong GUI, quyền được áp trực tiếp lên widget. Các nút bị cấm sẽ bị disable hoặc ẩn tùy chức năng.

## 9. Quản lý dữ liệu sinh viên

Struct trung tâm được khai báo trong [userspace_app/student.h](/d:/project%20driver/userspace_app/student.h):

- `student_code`
- `raw_name`
- `normalized_name`
- `student_class`
- `dob`
- `gpa`

Giới hạn hiện tại:

- tối đa `100` sinh viên
- tên dài tối đa `256` ký tự
- mã sinh viên dài tối đa `20`

### 9.1. Quy tắc dữ liệu

Khi thêm hoặc load dữ liệu:

- mã sinh viên chỉ nhận chữ và số
- mã sinh viên không được trùng
- lớp được chuẩn hóa về chữ in hoa và bỏ khoảng trắng
- ngày sinh phải theo dạng `dd/mm/yyyy`
- GPA phải nằm trong `[0.0, 4.0]`

### 9.2. Lưu file nội bộ

File dữ liệu chính là [userspace_app/students.txt](/d:/project%20driver/userspace_app/students.txt).

Định dạng:

```text
# code|name|class|dob|gpa
B21DCCN001|Nguyen Van A|CT7A|15/03/2003|3.50
```

Lưu ý quan trọng:

- `save_to_file()` lưu `normalized_name`, không lưu lại tên raw ban đầu.
- `load_from_file()` có cơ chế lọc dữ liệu lỗi:
  - bỏ dòng sai format
  - bỏ dòng GPA ngoài khoảng cho phép
  - bỏ dòng ngày sinh sai
  - bỏ dòng trùng mã sinh viên

Điều này giúp project có tính "lọc dữ liệu bẩn" ngay khi nạp file.

## 10. Chuẩn hóa tên qua kernel driver

Đây là luồng kỹ thuật quan trọng nhất của project.

Trong [userspace_app/student.c](/d:/project%20driver/userspace_app/student.c), hàm `normalize_via_driver()`:

1. mở `/dev/string_norm`
2. `write()` tên người dùng nhập xuống driver
3. `read()` chuỗi đã chuẩn hóa về
4. gán vào `normalized_name`

Ví dụ:

```text
Input : "  nGuyen     VAn    a  "
Output: "Nguyen Van A"
```

Nếu không mở được `/dev/string_norm`:

- hệ thống in lỗi ra `stderr`
- dùng fallback là giữ nguyên chuỗi nhập để không làm hỏng luồng CRUD

## 11. Tính năng USB

Module [userspace_app/usb_file.c](/d:/project%20driver/userspace_app/usb_file.c) hỗ trợ:

- ghi text file ra USB
- đọc text file từ USB

Điều kiện:

- `mount_path` phải tồn tại
- `mount_path` phải là thư mục
- `file_name` không được chứa dấu `/`

Trong GUI còn có:

- ô nhập thư mục mount USB
- ô nhập tên file
- vùng text để đọc/ghi nội dung
- trạng thái mount được kiểm tra định kỳ qua `/proc/mounts`

Ví dụ mount path:

```text
/run/media/<user>/<USB_NAME>
```

## 12. Xuất báo cáo CSV cho Excel

Hàm `export_to_csv()` trong [userspace_app/student.c](/d:/project%20driver/userspace_app/student.c) ghi file CSV với:

- BOM UTF-8
- tiêu đề cột tiếng Việt

Mục đích:

- mở tốt bằng Microsoft Excel
- hạn chế lỗi font tiếng Việt

Các cột xuất:

- Mã SV
- Họ và Tên
- Lớp
- Ngày Sinh
- GPA

Khác với file lưu nội bộ:

- CSV dùng `raw_name` để phục vụ người đọc
- file nội bộ `.txt` dùng `normalized_name` để phục vụ hệ thống

## 13. Audit log

File log là [userspace_app/audit.log](/d:/project%20driver/userspace_app/audit.log).

Module [userspace_app/audit.h](/d:/project%20driver/userspace_app/audit.h) cung cấp hàm:

```c
void log_audit(const char *username, const char *action_fmt, ...);
```

Audit log được dùng để ghi lại các thao tác như:

- đăng nhập thất bại
- load file
- thao tác với USB
- thao tác dữ liệu trong GUI
- thay đổi tài khoản hoặc mật khẩu

## 14. Giao diện CLI

Bản CLI nằm ở [userspace_app/main.c](/d:/project%20driver/userspace_app/main.c).

Menu hiện có:

1. Add student
2. Delete student
3. Search student
4. List students
5. Save to file
6. Load from file
7. Write text file to USB
8. Read text file from USB
9. Export students to USB
10. Edit student
11. Sort students
0. Exit

Phù hợp để demo luồng nghiệp vụ lõi mà không cần môi trường GTK.

## 15. Giao diện GUI

Bản GUI nằm ở [userspace_app/gui_app.c](/d:/project%20driver/userspace_app/gui_app.c).

Các nhóm tính năng chính:

- màn hình đăng nhập
- dashboard danh sách sinh viên
- form thêm/sửa/xóa
- tìm kiếm
- sắp xếp theo tên hoặc GPA
- load/save dữ liệu
- khu vực thao tác USB
- quản lý user
- đổi mật khẩu
- logout

Một số chi tiết đã có trong mã:

- hiển thị người dùng đang đăng nhập ở top bar
- disable nút theo role
- khóa đăng nhập 30 giây sau 3 lần sai
- chọn file/thư mục bằng `GtkFileChooser`
- kiểm tra trạng thái mount USB định kỳ

Để chạy GUI:

```bash
cd userspace_app
make student_manager_gui
./student_manager_gui
```

## 16. Test

Thư mục test là [tests](/d:/project%20driver/tests).

### 16.1. Userspace tests

- [tests/userspace_tests/test_normalize_mock.c](/d:/project%20driver/tests/userspace_tests/test_normalize_mock.c)
- [tests/userspace_tests/test_auth.c](/d:/project%20driver/tests/userspace_tests/test_auth.c)
- [tests/userspace_tests/test_student.c](/d:/project%20driver/tests/userspace_tests/test_student.c)

Chạy riêng:

```bash
cd tests
make run_mock
make run_auth
make run_student
```

### 16.2. Integration test với driver

File chính:

- [tests/integration_tests/test_driver_io.c](/d:/project%20driver/tests/integration_tests/test_driver_io.c)
- [tests/integration_tests/run_all_tests.sh](/d:/project%20driver/tests/integration_tests/run_all_tests.sh)

Chạy integration test:

```bash
cd tests
make run_integration
```

Chạy toàn bộ:

```bash
cd tests
make run_all
```

Lưu ý:

- test tích hợp cần `sudo`
- cần kernel module đã build được
- script sẽ tự build, load driver, chạy test, in kernel log và cleanup

## 17. Các workflow có sẵn

Thư mục [.agents/workflows](/d:/project%20driver/.agents/workflows) chứa các tài liệu thao tác nhanh:

- [setup-project.md](/d:/project%20driver/.agents/workflows/setup-project.md)
- [run-gui.md](/d:/project%20driver/.agents/workflows/run-gui.md)
- [run-tests.md](/d:/project%20driver/.agents/workflows/run-tests.md)
- [unload-driver.md](/d:/project%20driver/.agents/workflows/unload-driver.md)
- [check-status.md](/d:/project%20driver/.agents/workflows/check-status.md)
- [build-driver.md](/d:/project%20driver/.agents/workflows/build-driver.md)

Chúng hữu ích nếu bạn đang demo project theo từng bước cố định.

## 18. Demo nhanh

### Demo 1: chứng minh luồng userspace -> kernel -> userspace

1. Load driver:

```bash
cd kernel_module
sudo make load
```

2. Chạy GUI hoặc CLI.
3. Thêm sinh viên với tên nhập bẩn, ví dụ:

```text
  nGuyen     VAn     a
```

4. Quan sát danh sách hiển thị thành:

```text
Nguyen Van A
```

### Demo 2: phân quyền

1. Đăng nhập bằng `admin`.
2. Tạo user role `viewer`.
3. Đăng xuất.
4. Đăng nhập lại bằng tài khoản `viewer`.
5. Xác nhận các nút thêm/sửa/xóa/lưu bị khóa.

### Demo 3: xuất file cho Excel

1. Thêm vài sinh viên.
2. Chọn thư mục mount USB.
3. Bấm `Xuất Excel (.csv)`.
4. Mở file bằng Excel để kiểm tra cột và font tiếng Việt.

## 19. Các hạn chế hiện tại

- `load_from_file()` chỉ kiểm tra ngày sinh ở mức format và khoảng giá trị cơ bản, chưa kiểm tra sâu kiểu `31/02`.
- Dữ liệu sinh viên dùng mảng tĩnh tối đa `100` phần tử.
- Driver dùng buffer toàn cục, chưa tách state theo từng file descriptor.
- README cũ nhắc tới một số mô tả marketing nhiều hơn thực tế; bản này ưu tiên mô tả đúng theo source hiện có.

## 20. Tóm tắt lệnh hay dùng

```bash
# Build + load driver
cd kernel_module
make
sudo make load

# Build GUI
cd ../userspace_app
make student_manager_gui
./student_manager_gui

# Build CLI
make
./student_manager

# Chạy test
cd ../tests
make all
make run_all
```

## 21. Ghi chú

Project này kết hợp:

- Linux kernel module
- lập trình C
- file-based storage
- GTK3 GUI
- kiểm thử mức userspace và tích hợp

Nếu cần, tôi có thể viết tiếp README theo kiểu báo cáo môn học hoặc chỉnh lại theo format GitHub đẹp hơn.
