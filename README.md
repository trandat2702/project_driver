# Student Management System with Linux Character Drivers and GTK3

Dự án này là một hệ thống quản lý sinh viên viết bằng C trên Linux, kết hợp Linux character driver, ứng dụng desktop GTK3, xác thực tài khoản, lưu trữ file và thao tác USB trong cùng một bài toán.

- `kernel_module/`: 2 Linux character driver
  - `string_norm`: chuẩn hóa chuỗi họ tên
  - `usb_bridge`: bridge `ioctl()` cho mount/unmount và thao tác file text/CSV trên USB
- `userspace_app/`: ứng dụng GTK3 cho đăng nhập, quản lý sinh viên, quản lý tài khoản, audit log và USB manager
- `tests/`: unit test userspace và integration test cho driver

README này được viết lại theo đúng cấu trúc và mã nguồn hiện có trong repo. Nội dung bên dưới ưu tiên mô tả hành vi thật của source code và `Makefile` hiện tại hơn là các tài liệu demo cũ.

## Tổng quan kiến trúc

```text
+-----------------------------+
|      GTK3 GUI App           |
|  userspace_app/gui_app.c    |
+-------------+---------------+
              |
   +----------+-----------+------------------+
   |                      |                  |
   v                      v                  v
+---------+         +-----------+      +-------------+
| auth.c  |         | student.c |      | usb_file.c  |
| SHA-256 |         | CRUD      |      | USB access  |
| roles   |         | import    |      | + fallback  |
+----+----+         | export    |      +------+------+
     |              +-----+-----+             |
     |                    |                   v
     |                    v             /dev/usb_bridge
     |             /dev/string_norm
     v
+-----------+
| config.txt|
+-----------+
```

## Cấu trúc thư mục

```text
.
|-- kernel_module/
|   |-- Makefile
|   |-- string_norm.c
|   |-- string_norm.h
|   |-- usb_bridge.c
|   `-- usb_bridge_ioctl.h
|-- userspace_app/
|   |-- Makefile
|   |-- gui_app.c
|   |-- auth.c
|   |-- auth.h
|   |-- student.c
|   |-- student.h
|   |-- usb_file.c
|   |-- usb_file.h
|   |-- usb_driver_client.c
|   |-- usb_driver_client.h
|   |-- audit.c
|   |-- audit.h
|   |-- config.txt
|   |-- students.txt
|   `-- style.css
|-- tests/
|   |-- Makefile
|   |-- integration_tests/
|   `-- userspace_tests/
`-- scripts/
```

## Nên đọc code theo thứ tự nào để học nhanh

Nếu bạn muốn học dự án này như một hệ thống hoàn chỉnh thay vì đọc rời từng file, nên đi theo thứ tự dưới đây.

### Bước 1. Nắm mô hình dữ liệu trước

Đọc:

- `userspace_app/student.h`
- `userspace_app/auth.h`
- `userspace_app/usb_file.h`
- `userspace_app/usb_driver_client.h`
- `kernel_module/string_norm.h`
- `kernel_module/usb_bridge_ioctl.h`

Mục tiêu của bước này:

- hiểu struct `Student`
- hiểu các role `admin` / `viewer`
- hiểu các API userspace đang gọi nhau như thế nào
- hiểu ABI `ioctl()` giữa userspace và kernel

Nếu chưa hiểu header mà đã đọc `.c`, bạn rất dễ bị lạc vì không biết mỗi module “hứa hẹn” điều gì với module khác.

### Bước 2. Đọc nghiệp vụ userspace cốt lõi

Đọc:

- `userspace_app/student.c`
- `userspace_app/auth.c`
- `userspace_app/audit.c`

Bạn sẽ thấy các ý quan trọng:

- `student.c` quản lý danh sách sinh viên trong RAM
- `save_to_file()` và `load_from_file()` là cầu nối giữa RAM và file
- chuẩn hóa tên ưu tiên đi qua `/dev/string_norm`
- `auth.c` quản lý xác thực bằng `config.txt` và SHA-256
- `audit.c` chỉ ghi log, không chứa business logic chính

Đây là phần nên hiểu chắc nhất, vì GUI và test chủ yếu chỉ gọi lại các hàm ở đây.

### Bước 3. Đọc luồng USB ở userspace

Đọc:

- `userspace_app/usb_file.c`
- `userspace_app/usb_driver_client.c`

Thứ tự nên là:

1. `usb_file.c`
2. `usb_driver_client.c`

Lý do:

- `usb_file.c` cho bạn thấy chính sách thật của ứng dụng: validate, fallback, mount/unmount, copy, detect USB
- `usb_driver_client.c` chỉ là lớp bọc `ioctl()` mỏng để gửi yêu cầu xuống driver

Nếu đọc ngược lại, bạn sẽ thấy nhiều hàm `ioctl()` nhưng chưa hiểu tại sao GUI lại gọi chúng vào thời điểm đó.

### Bước 4. Đọc driver kernel

Đọc:

- `kernel_module/string_norm.c`
- `kernel_module/usb_bridge.c`

Nên hiểu theo góc nhìn:

- `string_norm.c` là driver nhỏ, dễ nắm hơn, phù hợp để học vòng đời character device
- `usb_bridge.c` phức tạp hơn vì có `ioctl()`, mount helper, VFS I/O và tracking mount point

Khi đọc `string_norm.c`, hãy tập trung vào:

- `init/exit`
- `file_operations`
- `write -> normalize -> read`
- mutex và buffer dùng chung

Khi đọc `usb_bridge.c`, hãy tập trung vào:

- validate path
- từng nhánh `switch(cmd)` trong `usb_bridge_ioctl()`
- `run_mount_helper()`
- `do_kernel_read()`, `do_kernel_write()`, `do_kernel_copy()`
- `tracked_mounts`

### Bước 5. Đọc GUI sau cùng

Đọc:

- `userspace_app/gui_app.c`
- `userspace_app/style.css`

Không nên đọc `gui_app.c` đầu tiên, vì file này là nơi “ghép” mọi module lại với nhau nên rất dài.

Cách đọc hiệu quả:

1. đọc `AppState`
2. đọc helper validate/UI
3. đọc callback đăng nhập, thêm, sửa, xóa, load/save
4. đọc USB Manager dialog
5. đọc `build_login_view()` và `build_dashboard_view()`
6. đọc `main()`

Nếu đi theo thứ tự đó, bạn sẽ thấy rõ:

- mỗi nút trên GUI gọi vào hàm nghiệp vụ nào
- role `admin` / `viewer` ảnh hưởng widget nào
- vì sao GUI phải refresh `GtkTreeView` sau khi sửa dữ liệu trong RAM

### Bước 6. Đọc test để kiểm tra hiểu biết

Đọc:

- `tests/userspace_tests/test_student.c`
- `tests/userspace_tests/test_auth.c`
- `tests/userspace_tests/test_normalize_mock.c`
- `tests/integration_tests/test_driver_io.c`
- `tests/integration_tests/test_usb_bridge_io.c`

Test giúp bạn xác nhận:

- hàm nào được kỳ vọng trả lỗi ra sao
- dữ liệu nào bị coi là hợp lệ / không hợp lệ
- ABI driver đang được dùng theo cách nào ở integration test

### Tóm tắt lộ trình khuyến nghị

```text
Header -> student/auth -> usb userspace -> kernel driver -> GUI -> test
```

Hoặc chi tiết hơn:

```text
student.h / auth.h
    -> student.c / auth.c
    -> usb_file.c / usb_driver_client.c
    -> string_norm.c / usb_bridge.c
    -> gui_app.c
    -> tests/
```

Nếu mục tiêu của bạn là học theo môn:

- Hệ điều hành: ưu tiên `string_norm.c`, `usb_bridge.c`, `usb_bridge_ioctl.h`
- Lập trình C ứng dụng: ưu tiên `student.c`, `auth.c`, `usb_file.c`
- GTK/GUI: ưu tiên `gui_app.c`, nhưng chỉ sau khi đã hiểu tầng nghiệp vụ
- Kiểm thử: ưu tiên thư mục `tests/`

## Thành phần chính

### 1. Kernel drivers

#### `string_norm`

Driver này cung cấp device:

```bash
/dev/string_norm
```

Luồng sử dụng:

1. user space `open()` device
2. gửi chuỗi gốc bằng `write()`
3. driver chuẩn hóa chuỗi
4. đọc kết quả bằng `read()`

Hành vi normalize hiện có:

- bỏ khoảng trắng đầu/cuối
- gộp nhiều khoảng trắng liên tiếp
- viết hoa ký tự đầu mỗi từ
- phần còn lại đưa về lowercase

Ví dụ:

```text
"  NGUYEN   VAN   AN  " -> "Nguyen Van An"
```

#### `usb_bridge`

Driver này cung cấp device:

```bash
/dev/usb_bridge
```

Các `ioctl()` chính được khai báo trong `kernel_module/usb_bridge_ioctl.h`:

- `USB_OP_MOUNT`
- `USB_OP_UNMOUNT`
- `USB_OP_READ_TEXT`
- `USB_OP_WRITE_TEXT`
- `USB_OP_COPY_HOST_TO_USB`
- `USB_OP_COPY_USB_TO_HOST`
- `USB_OP_CHECK_MOUNT_OWNERSHIP`

Driver chỉ cho phép file `.txt` và `.csv`, có kiểm tra path cơ bản, và theo dõi mount point do chính nó quản lý.

### 2. Ứng dụng GTK3

Binary được build ra là:

```bash
userspace_app/student_manager_gui
```

Các module chính:

- `gui_app.c`: giao diện và luồng điều khiển chính
- `auth.c`: xác thực, role `admin`/`viewer`, đổi mật khẩu, thêm/xóa user, reset mật khẩu
- `student.c`: CRUD sinh viên, validate dữ liệu, load/save, import/export
- `usb_file.c`: thao tác USB ở userspace, có fallback khi driver không dùng được trong một số luồng
- `usb_driver_client.c`: wrapper gọi `ioctl()` tới `/dev/usb_bridge`
- `audit.c`: ghi audit log ra `audit.log`

### 3. Dữ liệu và cấu hình

#### Tài khoản

File tài khoản:

```text
userspace_app/config.txt
```

Format:

```text
username:sha256_hash:role
```

Role hiện dùng:

- `admin`
- `viewer`

Tài khoản mẫu đang có trong repo:

- `admin` / `admin`
- `student` / `123`

Mật khẩu được băm bằng SHA-256 qua OpenSSL.

#### Dữ liệu sinh viên

File mặc định:

```text
userspace_app/students.txt
```

Ứng dụng nạp file này khi khởi động và lưu lại khi thoát.

Struct `Student` trong `userspace_app/student.h`:

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

Giới hạn hiện tại:

- tối đa `100` sinh viên trong RAM
- `MAX_NAME_LEN = 256`
- `MAX_CODE_LEN = 20`
- `MAX_CLASS_LEN = 20`
- `MAX_DOB_LEN = 15`

### 4. Chức năng chính

#### Quản lý sinh viên

- thêm, sửa, xóa sinh viên
- tìm kiếm theo mã hoặc tên
- sắp xếp theo tên hoặc GPA
- lưu file nội bộ
- nạp lại từ file
- export `.txt` và `.csv`

#### Chuẩn hóa dữ liệu

- mã sinh viên: bỏ khoảng trắng, giữ chữ/số, chuyển uppercase
- họ tên: ưu tiên normalize qua `/dev/string_norm`, nếu không được thì fallback ở userspace
- lớp: bỏ khoảng trắng và chuyển uppercase
- ngày sinh: kiểm tra format `dd/mm/yyyy`
- GPA: giới hạn `0.0` đến `4.0`

#### Phân quyền

- `admin`: có quyền CRUD dữ liệu, lưu file, quản lý tài khoản
- `viewer`: xem danh sách, tìm kiếm, export, đổi mật khẩu, dùng USB manager

#### Bảo mật và audit

- đăng nhập có lockout sau 3 lần sai liên tiếp trong 30 giây
- hỗ trợ đổi mật khẩu
- có audit log tại `userspace_app/audit.log`

#### USB Storage Manager

GUI có màn hình quản lý USB cho các thao tác:

- detect USB đã mount
- mount/unmount
- đọc/ghi file text
- copy `.txt` và `.csv` giữa host và USB

`usb_file.c` có fallback sang `udisksctl` hoặc userspace I/O ở một số luồng, nhưng một số nút trong GUI vẫn kiểm tra sự hiện diện của driver trước khi chạy.

## Yêu cầu môi trường

Đây là project Linux. Để build và chạy đầy đủ, cần:

- `gcc`
- `make`
- `pkg-config`
- GTK3 development package
- OpenSSL development package
- kernel headers tương ứng với kernel đang chạy
- quyền `sudo` để load/unload module và chạy integration test

Ví dụ trên Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-3-dev libssl-dev linux-headers-$(uname -r)
```

## Build

### Build kernel module

```bash
cd kernel_module
make
```

Một số target có sẵn:

- `make load`
- `make load-usb`
- `make load-all`
- `make unload`
- `make unload-usb`
- `make unload-all`
- `make status`

### Build GUI

```bash
cd userspace_app
make
```

### Build test

```bash
cd tests
make all
```

## Cách chạy

### 1. Build và load driver

```bash
cd kernel_module
make
make load-all
make status
```

### 2. Build GUI

```bash
cd ../userspace_app
make
```

### 3. Chạy ứng dụng

```bash
cd ../userspace_app
./student_manager_gui
```

Nên chạy từ đúng thư mục `userspace_app` vì chương trình dùng path tương đối tới:

- `config.txt`
- `students.txt`
- `style.css`
- `audit.log`

## Test

Trong `tests/Makefile` hiện có các lệnh:

```bash
cd tests
make run_mock
make run_auth
make run_student
make run_integration
make run_usb_bridge
make run_all
```

Ý nghĩa:

- `run_mock`: test normalize userspace
- `run_auth`: test xác thực và quản lý user
- `run_student`: test nghiệp vụ sinh viên
- `run_integration`: test `/dev/string_norm`
- `run_usb_bridge`: test `/dev/usb_bridge`

Các integration test cần `sudo` và cần driver được load trước.

## Hạn chế hiện tại

- xử lý tên chưa hỗ trợ Unicode tiếng Việt đầy đủ
- `string_norm` dùng buffer dùng chung trong driver
- dữ liệu sinh viên đang dùng mảng tĩnh tối đa 100 bản ghi
- validate ngày sinh mới ở mức cơ bản
- import file theo kiểu strict, dòng lỗi có thể làm fail cả file
- tìm kiếm đang là substring search
- GUI phụ thuộc vào path tương đối khi chạy
- repo đang chứa cả source lẫn build artifact

## Ghi chú

Nếu `README.md` cũ hiển thị lỗi tiếng Việt, nguyên nhân là file trước đó bị sai encoding. Bản hiện tại đã được viết lại theo nội dung thực tế của repo và lưu ở dạng UTF-8.

## Phụ lục kỹ thuật chi tiết

Phần này đi sâu hơn vào các hành vi bên trong của project để README có thể dùng như tài liệu kỹ thuật, tài liệu bàn giao hoặc tài liệu chuẩn bị demo.

### 1. Luồng chạy end-to-end

#### 1.1. Khi ứng dụng GUI khởi động

Luồng `main()` trong `userspace_app/gui_app.c` hiện tại:

1. `gtk_init()`
2. tạo `GtkWindow`
3. nạp `style.css`
4. tạo `GtkStack` gồm 2 màn:
   - `login`
   - `dashboard`
5. gọi `load_from_file("students.txt", app.list, &app.count)`
6. đổ dữ liệu từ RAM lên `GtkTreeView`
7. hiển thị cửa sổ
8. khi thoát chương trình, luôn gọi `save_to_file("students.txt", app.list, app.count)`

Hệ quả quan trọng:

- chương trình dùng path tương đối, nên nên chạy từ đúng thư mục `userspace_app`
- nếu `students.txt` đang là CSV hợp lệ, app vẫn nạp được
- khi đóng app, file `students.txt` mặc định sẽ bị ghi lại theo format nội bộ dùng dấu `|`

#### 1.2. Luồng đăng nhập

Màn hình login dùng:

- `authenticate_with_role("config.txt", username, password, role, sizeof(role))`
- inline error label thay vì popup riêng
- Enter ở ô user/password cũng submit form

Logic lockout hiện tại:

- sai 1 hoặc 2 lần: hiện số lần còn lại
- sai lần thứ 3: khóa form trong `30` giây
- trong thời gian khóa:
  - disable ô user
  - disable ô password
  - disable nút login
- dùng `g_timeout_add_seconds(1, ...)` để đếm ngược

#### 1.3. Luồng CRUD sinh viên

Khi thêm mới từ GUI:

1. GUI validate mã, tên, lớp, DOB, GPA
2. gọi `add_student(...)`
3. `add_student()` chuẩn hóa:
   - mã sinh viên ở userspace
   - tên qua driver `string_norm`, nếu không có driver thì fallback userspace
   - lớp ở userspace
4. dữ liệu được thêm vào mảng `app.list`
5. GUI refresh lại bảng
6. ghi audit log

Khi sửa:

- GUI sửa trực tiếp record trong `app.list`
- không gọi `edit_student()` của bản CLI
- sau khi sửa xong mới refresh `GtkTreeView`

Khi xóa:

- GUI ưu tiên lấy mã từ dòng đang chọn
- nếu chưa chọn dòng, có thể lấy mã từ ô nhập
- trước khi xóa có hộp xác nhận

#### 1.4. Luồng file dữ liệu

- `save_to_file()` ghi snapshot của mảng sinh viên hiện tại ra file
- `load_from_file()` nạp vào mảng tạm rồi mới thay thế danh sách hiện tại nếu toàn bộ file hợp lệ
- `export_to_csv()` tạo file báo cáo cho người dùng/Excel, không phải format nội bộ để app đọc lại

#### 1.5. Luồng USB

Từ GUI xuống driver, đường đi logic là:

```text
gui_app.c
  -> usb_file.c
    -> usb_driver_client.c
      -> ioctl(/dev/usb_bridge)
```

Trong một số trường hợp fallback sẽ đi theo nhánh khác:

- mount/unmount: fallback `udisksctl`, rồi `umount2()`
- read/write text: fallback `libc`
- copy file: fallback `libc` hiện đang nằm trong callback GUI, không nằm hoàn toàn trong `usb_file.c`

### 2. Chi tiết kỹ thuật theo module

#### 2.1. `userspace_app/student.c`

Đây là module nghiệp vụ trung tâm cho dữ liệu sinh viên.

##### Cấu trúc `Student`

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

Ý nghĩa từng trường:

- `student_code`: mã sinh viên sau chuẩn hóa, dùng như khóa chính ở tầng ứng dụng
- `raw_name`: tên gốc người dùng nhập
- `normalized_name`: tên sau khi chuẩn hóa
- `student_class`: lớp sau chuẩn hóa
- `dob`: ngày sinh dạng `dd/mm/yyyy`
- `gpa`: thang điểm `0.0..4.0`

##### Quy tắc validate/normalize

`student_code`:

- ở GUI: chỉ cho phép chữ và số
- khi lưu: bỏ toàn bộ khoảng trắng và chuyển uppercase

`raw_name`:

- `is_valid_name()` chỉ cho phép ký tự alphabet + khoảng trắng
- ký tự số và ký tự đặc biệt bị từ chối
- normalize ưu tiên qua `/dev/string_norm`
- sau khi driver trả kết quả, vẫn qua thêm bước `sanitize_name_alpha_space()` để lọc lại ký tự
- nếu driver không khả dụng, normalize ngay ở userspace

`student_class`:

- cho phép chữ, số, khoảng trắng ở đầu vào
- khi lưu sẽ bỏ khoảng trắng và chuyển uppercase

`dob`:

- phải dài đúng 10 ký tự
- bắt buộc có `/` ở vị trí `2` và `5`
- parse được `dd/mm/yyyy`
- ngày `1..31`, tháng `1..12`, năm `1900..2100`

`gpa`:

- parse bằng `strtof`
- không được có ký tự rác phía sau
- phải nằm trong khoảng `0.0..4.0`

##### Hành vi tìm kiếm và sắp xếp

`search_student()` hiện:

- tìm bằng `strstr()`
- là tìm theo substring
- có phân biệt hoa thường
- tìm trên cả:
  - `normalized_name`
  - `raw_name`
  - `student_code`

`sort_by_name()`:

- sắp xếp theo `normalized_name`
- dùng `qsort()`

`sort_by_gpa()`:

- có cả tăng dần và giảm dần
- cũng dùng `qsort()`

##### File I/O của `student.c`

`save_to_file()`:

- ghi header `# code|name|class|dob|gpa`
- mỗi record dùng delimiter `|`
- ghi `normalized_name`
- dùng `flock(..., LOCK_EX)` để tránh ghi chồng ở mức cơ bản

`load_from_file()`:

- nhận cả format nội bộ `|`
- và format CSV `,`
- bỏ qua:
  - dòng rỗng
  - dòng comment bắt đầu bằng `#`
  - UTF-8 BOM ở đầu dòng
- trim khoảng trắng ở từng field
- với CSV, nếu field cuối là `GPA` thì coi là header và bỏ qua
- strict mode:
  - chỉ cần 1 dòng sai là fail cả file
  - chỉ khi toàn bộ file hợp lệ mới copy sang danh sách chính

`export_to_csv()`:

- ghi BOM UTF-8
- header tiếng Việt
- xuất `raw_name`
- phù hợp hơn cho người dùng mở bằng Excel

##### Mã trả về đáng chú ý của `student.c`

| Hàm | Thành công | Lỗi |
|---|---:|---|
| `add_student()` | `0` | `-1` |
| `delete_student()` | `0` | `-1` |
| `save_to_file()` | `0` | `-1` |
| `load_from_file()` | `0` | `-1` không mở được file, `-2` sai format/vi phạm rule |
| `export_to_csv()` | `0` | `-1` |

#### 2.2. `userspace_app/auth.c`

Module này quản lý xác thực và role.

##### Format file tài khoản

`config.txt` hiện hỗ trợ 2 kiểu:

Format đầy đủ:

```text
username:sha256_hash:role
```

Format cũ tương thích ngược:

```text
username:sha256_hash
```

Nếu file dùng format cũ 2 cột, role mặc định sẽ là `admin`.

##### Tài khoản mẫu trong repo hiện tại

Theo `userspace_app/config.txt`:

- `admin` / mật khẩu `admin` / role `admin`
- `student` / mật khẩu `123` / role `viewer`

##### Cách hoạt động

- mật khẩu được băm bằng `SHA256()` từ OpenSSL
- lưu thành hex string dài 64 ký tự
- file không lưu plaintext password
- hiện tại chưa có salt và chưa có key stretching

##### Các hàm chính

`authenticate_credentials()`:

- chỉ trả đúng/sai

`authenticate_with_role()`:

- vừa xác thực vừa trả role
- đây là hàm GUI dùng trực tiếp khi login

`change_password()`:

- xác minh mật khẩu cũ trước
- giữ nguyên role
- rewrite lại toàn bộ file

`admin_reset_password()`:

- bỏ qua bước kiểm tra mật khẩu cũ
- giữ nguyên role

`add_user()`:

- chỉ chấp nhận `admin` hoặc `viewer`
- không cho username trùng
- append user mới vào cuối file

`delete_user()`:

- rewrite lại file và bỏ qua dòng user cần xóa

`list_users()`:

- trả danh sách user + role cho dialog quản lý tài khoản

##### Mã trả về đáng chú ý của `auth.c`

| Hàm | Thành công | Mã lỗi / ý nghĩa |
|---|---:|---|
| `authenticate_credentials()` | `1` | `0` |
| `authenticate_with_role()` | `1` | `0` |
| `get_user_role()` | `0` | `-1` |
| `change_password()` | `0` | `-1` sai mật khẩu cũ, `-2` mật khẩu mới rỗng, `-3` lỗi file |
| `admin_reset_password()` | `0` | `-2` mật khẩu mới rỗng, `-3` lỗi file, `-4` không tìm thấy user |
| `add_user()` | `0` | `-1` tham số sai, `-2` username/password rỗng, `-3` role sai, `-4` trùng username, `-5` lỗi file |
| `delete_user()` | `0` | `-1` tham số sai, `-2` lỗi file, `-3` không tìm thấy user |
| `list_users()` | số lượng user | `0` nếu không đọc được hoặc không có dữ liệu hợp lệ |

#### 2.3. `userspace_app/gui_app.c`

Đây là tầng giao diện và orchestration của toàn bộ ứng dụng.

##### `AppState`

`AppState` giữ:

- danh sách sinh viên trong RAM
- danh sách kết quả tìm kiếm
- toàn bộ widget chính
- username và role của phiên hiện tại
- bộ đếm login sai và lockout

##### Những gì dashboard đang có

- sidebar quản lý
- khu vực sắp xếp
- khu vực file dữ liệu
- nút mở USB manager
- nút quản lý tài khoản
- nút đổi mật khẩu
- nút đăng xuất
- `GtkTreeView` danh sách sinh viên
- form thêm/sửa/xóa
- 2 nút export
- status bar

##### RBAC ở mức GUI

Sau khi login:

- `admin` nhìn thấy:
  - nút `Thêm`
  - nút `Sửa`
  - nút `Xóa`
  - nút `Lưu file`
  - form nhập liệu
  - nút `Quản lý tài khoản`
- `viewer` không thấy các phần trên

`viewer` vẫn có thể:

- xem danh sách
- tìm kiếm
- sắp xếp
- tải file
- export text/csv
- dùng USB manager
- đổi mật khẩu

##### Dialog quản lý tài khoản

Admin có thể:

- xem danh sách user và role
- thêm user
- xóa user được chọn
- reset password user được chọn

Nuance quan trọng:

- GUI không cho xóa chính tài khoản đang đăng nhập
- nhưng dialog này hiện không hiển thị đầy đủ mọi mã lỗi nghiệp vụ từ `auth.c`
- ví dụ một số thao tác add/delete/reset có thể thất bại mà dialog chỉ đơn giản refresh hoặc không báo chi tiết

##### Audit trong GUI

Các action hiện đang được ghi log:

- failed login attempt
- thêm/sửa/xóa sinh viên
- save/load file
- export CSV/TXT
- mount/unmount USB
- USB read/write/copy
- tạo user
- xóa user
- reset password
- đổi mật khẩu

Hiện chưa thấy log riêng cho:

- successful login
- logout

#### 2.4. `userspace_app/usb_file.c` và `userspace_app/usb_driver_client.c`

Hai file này tạo thành tầng policy + driver wrapper cho luồng USB.

##### `usb_driver_client.c`

Đây là lớp bọc trực tiếp quanh `ioctl()`:

- `usb_driver_mount()`
- `usb_driver_unmount()`
- `usb_driver_read_text()`
- `usb_driver_write_text()`
- `usb_driver_copy_to_usb()`
- `usb_driver_copy_from_usb()`
- `usb_driver_available()`
- `usb_driver_is_managed_mount()`

Giới hạn chính:

- path tối đa `256`
- content tối đa `65536` byte

##### `usb_file.c`

Lớp này thêm validation và fallback.

`usb_write_text_file()`:

- build full path từ `mount_path + file_name`
- không cho `file_name` chứa `/`
- nếu driver có sẵn: ưu tiên `USB_OP_WRITE_TEXT`
- nếu driver lỗi: fallback `fopen/fputs`

`usb_read_text_file()`:

- tương tự write
- fallback `fread`

`usb_mount()` / `usb_mount_detect()`:

- ưu tiên driver
- nếu driver lỗi hoặc không có: fallback `udisksctl mount -b ...`
- `usb_mount_detect()` còn cố gắng parse mount point thực tế từ output của `udisksctl`

`usb_unmount()`:

- ưu tiên driver
- fallback `udisksctl`
- cuối cùng fallback `umount2()`

`usb_copy_to_device()` / `usb_copy_from_device()`:

- ở lớp `usb_file.c`, hai hàm này hiện yêu cầu driver phải có sẵn
- nếu driver không có, chúng trả lỗi luôn

Nhưng trong GUI:

- callback copy vẫn có thêm fallback `libc` riêng
- tức là fallback copy hiện nằm ở tầng GUI nhiều hơn là ở tầng `usb_file.c`

##### Detect USB

`detect_usb_mounts()`:

- đọc `/proc/mounts`
- nhận diện USB theo:
  - device bắt đầu bằng `/dev/sd`
  - hoặc path nằm trong `/media/` hay `/run/media/`
- chỉ coi là filesystem “thật” nếu thuộc một trong các loại:
  - `vfat`
  - `exfat`
  - `ntfs`
  - `ntfs3`
  - `fuseblk`
  - `ext4`
  - `ext3`
  - `ext2`

`detect_usb_devices()`:

- gọi `lsblk -rno NAME,SIZE,TYPE,MOUNTPOINT`
- lấy các partition có tên bắt đầu `sd*`

#### 2.5. `userspace_app/audit.c`

Audit log hiện rất gọn:

- file mặc định: `audit.log`
- mở file ở mode append
- format mỗi dòng:

```text
[YYYY-MM-DD HH:MM:SS] [username] action...
```

Không có:

- log rotation
- log level
- locking riêng cho file log

### 3. Chi tiết sâu về kernel driver

#### 3.1. `kernel_module/string_norm.c`

##### Thông số chính

- `DEVICE_NAME = "string_norm"`
- `CLASS_NAME = "string_norm_class"`
- `BUF_SIZE = 1024`

Driver dùng:

- `alloc_chrdev_region()`
- `cdev_add()`
- `class_create()`
- `device_create()`
- `copy_from_user()`
- `copy_to_user()`
- `DEFINE_MUTEX(dev_mutex)`

##### Cách driver làm việc

- user space `write()` chuỗi vào `input_buf`
- driver normalize vào `processed_buf`
- user space `read()` lấy kết quả

##### Quy tắc normalize trong kernel

- bỏ khoảng trắng đầu chuỗi
- gộp khoảng trắng liên tiếp thành 1 dấu cách
- viết hoa chữ cái đầu của mỗi từ
- các ký tự còn lại đưa về lowercase
- bỏ khoảng trắng cuối nếu có

##### Giới hạn và nuance

- buffer dùng chung toàn cục:
  - `input_buf[1024]`
  - `processed_buf[1024]`
- state không tách theo từng file descriptor
- chỉ có `read()` và `write()`, không có `ioctl()`
- không có xử lý Unicode/UTF-8 cho tiếng Việt có dấu
- `kernel_module/string_norm.h` khai báo `normalize_string(...)`, nhưng hàm thực tế đang dùng trong driver là `normalize_string_kernel(...)` dạng `static`

#### 3.2. `kernel_module/usb_bridge.c`

##### Thông số chính

- `DEVICE_NAME = "usb_bridge"`
- `CLASS_NAME = "usb_bridge_class"`
- `CHUNK_SIZE = 4096`
- `MAX_TRACKED_MOUNTS = 16`
- path tối đa `256`
- content tối đa `65536` byte

##### Những gì driver hỗ trợ

- mount
- unmount
- read `.txt` / `.csv`
- write `.txt` / `.csv`
- copy file `.txt` / `.csv`
- hỏi trạng thái mount point có được driver track hay không

##### Bảo vệ đầu vào

`validate_path()` hiện kiểm tra:

- path không rỗng
- phải là path tuyệt đối
- không chứa `..`
- ngắn hơn `USB_MAX_PATH_LEN`

`validate_text_extension()` hiện chỉ cho:

- `.txt`
- `.csv`

##### Mount / unmount

Driver không tự mount bằng cơ chế VFS riêng, mà gọi helper:

- mount qua `/bin/mount`
- unmount qua `/bin/umount`

Mount point được track trong `tracked_mounts`.

Nuance quan trọng:

- driver có hỗ trợ `USB_OP_CHECK_MOUNT_OWNERSHIP`
- nhưng trong nhánh `USB_OP_UNMOUNT` hiện tại, driver không hard-reject mount point “không do nó track” trước khi gọi `/bin/umount`
- nó chỉ log trạng thái tracked và nếu unmount thành công thì `untrack_mount()`

##### Read / write / copy

- `do_kernel_read()` dùng `filp_open()` + `kernel_read()`
- `do_kernel_write()` dùng `filp_open()` + `kernel_write()`
- `do_kernel_copy()` copy theo block `4096` byte

##### Đồng bộ và concurrency

- toàn bộ `ioctl()` hiện đi qua cùng một `bridge_mutex`
- đây là mô hình serialize đơn giản, không phải thiết kế song song tinh vi theo session

### 4. Định dạng dữ liệu chi tiết

#### 4.1. `config.txt`

Ví dụ hợp lệ:

```text
# Format: username:sha256_hash:role
admin:<sha256>:admin
student:<sha256>:viewer
```

#### 4.2. Format nội bộ để app lưu lại

```text
# code|name|class|dob|gpa
B21DCCN001|Nguyen Van A|CT7A|01/01/2003|3.50
```

#### 4.3. Format CSV app chấp nhận khi import

```text
Mã SV,Họ và Tên,Lớp,Ngày Sinh,GPA
B21DCCN001,Nguyen Van A,CT7A,01/01/2003,3.50
```

#### 4.4. Format CSV export

Header hiện do code ghi ra:

```text
Mã SV,Họ và Tên,Lớp,Ngày Sinh,GPA
```

Khác biệt chính so với format nội bộ:

- delimiter là `,`
- xuất `raw_name`
- có UTF-8 BOM

#### 4.5. `students.txt` hiện đang ở trạng thái nào?

Trong repo hiện tại, `userspace_app/students.txt` đang là file CSV mẫu.

Điều này hợp lệ vì:

- `load_from_file()` đọc được CSV

Nhưng cần nhớ:

- khi app đóng, `main()` sẽ gọi `save_to_file("students.txt", ...)`
- nên `students.txt` mặc định có thể bị đổi về format nội bộ dùng `|`

### 5. Test coverage chi tiết

#### 5.1. `tests/userspace_tests/test_normalize_mock.c`

Mục tiêu:

- test logic normalize khi không có driver
- mock `normalize_via_driver()` trả `-1`
- kiểm tra:
  - title case
  - trim khoảng trắng
  - bỏ số và ký tự đặc biệt
  - xử lý buffer nhỏ

#### 5.2. `tests/userspace_tests/test_auth.c`

Mục tiêu:

- test SHA-256
- test login đúng/sai
- test lấy role
- test thêm user / xóa user
- test đổi mật khẩu
- test edge case cơ bản

#### 5.3. `tests/userspace_tests/test_student.c`

Mục tiêu:

- test validate tên
- test thêm/xóa sinh viên
- test search
- test sort
- test save/load file
- test export CSV

#### 5.4. `tests/integration_tests/test_driver_io.c`

Mục tiêu:

- test `/dev/string_norm`
- nếu driver không load, test tự đánh dấu `SKIP`

#### 5.5. `tests/integration_tests/test_usb_bridge_io.c`

Mục tiêu:

- test read/write/copy qua `usb_bridge`
- test path validation
- test extension validation
- test behavior khi file không tồn tại

#### 5.6. Cách nên chạy test

Nên ưu tiên:

```bash
cd tests
make run_mock
make run_auth
make run_student
make run_integration
make run_usb_bridge
```

Lý do:

- `tests/Makefile` đang là nguồn điều phối gọn và khớp hơn với mã nguồn hiện tại

### 6. Script hỗ trợ và tài liệu demo đi kèm

#### 6.1. `scripts/setup_usb_permissions.sh`

Script này có ý định:

- reload `usb_bridge.ko`
- tạo udev rules cho quyền thiết bị
- tạo polkit rule cho mount/unmount không cần password
- tạo/cài SELinux policy

Nhưng script này hiện hardcode:

- user `dat`
- path `/home/dat/project_driver`

Nghĩa là:

- nếu môi trường không phải máy của user `dat`, cần sửa script trước khi dùng

#### 6.2. `scripts/install_selinux_policy.sh`

Script này:

- cố gắng sinh/cài SELinux policy cho `usb_bridge`
- có thể lấy AVC từ audit log rồi build policy
- hoặc compile từ file `.te`

Script cũng hardcode path `/home/dat/project_driver`.

#### 6.3. `scripts/usb_bridge_selinux.*`

Repo hiện có:

- `usb_bridge_selinux.te`
- `usb_bridge_selinux.mod`
- `usb_bridge_selinux.pp`

Đây là artifact/policy hỗ trợ SELinux, không phải thành phần bắt buộc để đọc hiểu logic core của project.

#### 6.4. `workflows/demo.md`

File này hiện:

- bị lỗi encoding
- chứa path tuyệt đối `/home/dat/project_driver`
- chứa một số giả định demo cũ

Vì vậy:

- nên coi `README.md`, `Makefile`, `config.txt` và source code hiện tại là nguồn sự thật chính
- không nên bám tuyệt đối vào `workflows/demo.md` nếu nội dung mâu thuẫn

### 7. Những gotcha quan trọng nếu tiếp tục phát triển project

- `add_student()` kiểm tra trùng mã trước khi normalize mã đầu vào, nên tính duy nhất của các biến thể khác nhau về hoa/thường/khoảng trắng hiện dựa nhiều vào validate ở GUI.
- `is_valid_name()` không tự chặn trường hợp chuỗi chỉ toàn khoảng trắng; GUI đang chặn trước khi gọi.
- `load_from_file()` strict mode: sai 1 dòng là fail cả file.
- `search_student()` là substring search và có phân biệt hoa thường.
- top bar GUI hiện có label `Driver: string_norm` và `● Sẵn sàng`, nhưng đây không phải health check động của driver.
- nút `Mount` trong USB Manager yêu cầu driver sẵn sàng ngay từ đầu, dù `usb_file.c` có fallback `udisksctl`.
- USB Manager đọc file vào buffer userspace cỡ `4096` byte trước khi đổ lên `GtkTextView`, trong khi driver hỗ trợ content lớn hơn (`65536` byte).
- copy fallback sang `libc` hiện nằm trong callback GUI, không phải chính thức ở tầng `usb_file.c`.
- dialog quản lý tài khoản chưa phản hồi đầy đủ mọi nhánh lỗi từ `auth.c`.
- audit hiện log failed login nhưng không log successful login hoặc logout.
- repo đang chứa cả source và build artifact:
  - `.ko`
  - `.o`
  - `.mod`
  - binary test
  - `student_manager_gui`

### 8. Khắc phục sự cố nâng cao

#### 8.1. App mở lên nhưng danh sách rỗng

Kiểm tra:

- có chạy từ đúng `userspace_app/` không
- `students.txt` có đang đúng format không
- file có chứa ký tự ngoài rule validate không

Lưu ý:

- `main()` hiện không hiện popup lỗi riêng nếu `load_from_file("students.txt", ...)` thất bại; app vẫn có thể mở với danh sách rỗng

#### 8.2. Không mount được USB

Kiểm tra lần lượt:

- đã `make load-usb` hoặc `make load-all` chưa
- có `/dev/usb_bridge` chưa
- hệ thống có `udisksctl`, `lsblk`, `blkid` chưa
- mount point trong `/tmp` có tạo được không
- nếu dùng SELinux/polkit script, đã sửa lại path và username cho đúng máy hiện tại chưa

#### 8.3. Copy file USB bị từ chối

Với luồng qua driver, source/destination phải thỏa:

- path tuyệt đối
- không chứa `..`
- extension là `.txt` hoặc `.csv`

#### 8.4. Script test tổng `run_all_tests.sh` không chạy đúng

Nên kiểm tra kỹ vì script này hiện có dấu hiệu cũ/out-of-sync:

- compile một số test bằng câu lệnh khác với `tests/Makefile`
- có phần hardcode `/tmp`
- phụ thuộc `sudo`

Khuyến nghị:

- ưu tiên chạy test bằng `make` trong thư mục `tests/`
