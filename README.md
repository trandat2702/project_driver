# Student Management System với Linux Character Drivers và GTK3

Đây là dự án quản lý sinh viên viết bằng C trên Linux, gồm 2 phần chính chạy song song:

- `kernel_module/`: các character driver `string_norm` và `usb_bridge`
- `userspace_app/`: ứng dụng desktop GTK3 để đăng nhập, quản lý sinh viên, làm việc với file dữ liệu và thao tác USB

Dự án không chỉ minh họa giao tiếp user space <-> kernel space, mà còn ghép nhiều lớp thực tế vào cùng một chương trình:

- chuẩn hóa tên sinh viên qua driver
- xác thực tài khoản bằng SHA-256
- phân quyền `admin` và `viewer`
- CRUD sinh viên trong RAM
- lưu, nạp, import và export dữ liệu
- audit log cho các thao tác quan trọng
- quản lý file text/CSV trên USB qua driver hoặc fallback ở user space

README này được viết lại theo đúng mã nguồn hiện có trong repository, không mô tả các tính năng chưa tồn tại và cũng không bỏ qua các giới hạn thực tế của code.

## 1. Tổng quan kiến trúc

```text
                 +-----------------------------+
                 |      GTK3 GUI App           |
                 |  userspace_app/gui_app.c    |
                 +-------------+---------------+
                               |
        +----------------------+----------------------+
        |                      |                      |
        v                      v                      v
 +-------------+      +----------------+      +------------------+
 |   auth.c    |      |   student.c    |      |    usb_file.c    |
 | SHA-256     |      | CRUD + import  |      | mount/read/write |
 | role-based  |      | export + sort  |      | copy USB files   |
 +------+------+      +-------+--------+      +---------+--------+
        |                     |                         |
        |                     v                         v
        |             /dev/string_norm          /dev/usb_bridge
        |                     |                         |
        v                     v                         v
 +-------------+     +----------------+      +--------------------+
 |  config.txt |     | string_norm.ko |      |   usb_bridge.ko    |
 +-------------+     +----------------+      +--------------------+

                 +-----------------------------+
                 |          audit.c            |
                 | ghi audit.log theo phiên    |
                 +-----------------------------+
```

## 2. Thành phần của dự án

### 2.1. `kernel_module/`

Thư mục này chứa 2 kernel module:

- `string_norm.c`: driver chuẩn hóa chuỗi tên
- `usb_bridge.c`: driver nhận `ioctl()` để mount/unmount, đọc/ghi/copy file text và CSV

Ngoài source còn có sẵn nhiều artifact build như `.ko`, `.o`, `.mod.c`, `modules.order`, `Module.symvers`. Đây là trạng thái hiện tại của repo.

### 2.2. `userspace_app/`

Ứng dụng chính được build thành binary `student_manager_gui`. Các module chính:

- `gui_app.c`: giao diện, callback GTK, orchestration toàn bộ luồng
- `student.c`: nghiệp vụ sinh viên, normalize, CRUD, load/save, export CSV
- `auth.c`: đăng nhập, đọc role, đổi mật khẩu, thêm/xóa user, reset mật khẩu
- `usb_file.c`: lớp thao tác file USB ở user space
- `usb_driver_client.c`: wrapper gọi `ioctl()` tới `/dev/usb_bridge`
- `audit.c`: ghi audit log
- `style.css`: theme GTK
- `config.txt`: dữ liệu tài khoản
- `students.txt`: dữ liệu sinh viên mặc định

### 2.3. `tests/`

Gồm 2 nhóm:

- `userspace_tests/`: test logic userspace
- `integration_tests/`: test giao tiếp thật với driver

## 3. Cấu trúc thư mục chính

```text
.
├── kernel_module/
│   ├── Makefile
│   ├── string_norm.c
│   ├── string_norm.h
│   ├── usb_bridge.c
│   └── usb_bridge_ioctl.h
├── userspace_app/
│   ├── Makefile
│   ├── gui_app.c
│   ├── student.c
│   ├── student.h
│   ├── auth.c
│   ├── auth.h
│   ├── usb_file.c
│   ├── usb_file.h
│   ├── usb_driver_client.c
│   ├── usb_driver_client.h
│   ├── audit.c
│   ├── audit.h
│   ├── style.css
│   ├── config.txt
│   └── students.txt
└── tests/
    ├── Makefile
    ├── userspace_tests/
    │   ├── test_normalize_mock.c
    │   ├── test_auth.c
    │   └── test_student.c
    └── integration_tests/
        ├── test_driver_io.c
        ├── test_usb_bridge_io.c
        └── run_all_tests.sh
```

## 4. Kernel driver `string_norm`

### 4.1. Vai trò

`string_norm` là character device dùng để chuẩn hóa chuỗi tên từ user space. Device node mục tiêu là:

```bash
/dev/string_norm
```

### 4.2. Cách hoạt động

Luồng gọi rất đơn giản:

1. user space mở `/dev/string_norm`
2. ghi chuỗi gốc bằng `write()`
3. driver chuẩn hóa chuỗi trong kernel
4. đọc lại kết quả bằng `read()`

Driver dùng:

- `alloc_chrdev_region()`
- `cdev_add()`
- `class_create()`
- `device_create()`
- `copy_from_user()` và `copy_to_user()`
- `mutex` để bảo vệ buffer dùng chung

### 4.3. Quy tắc normalize

Theo implementation hiện tại trong `kernel_module/string_norm.c`, driver:

- bỏ khoảng trắng đầu chuỗi
- gộp nhiều khoảng trắng liên tiếp thành một dấu cách
- viết hoa ký tự đầu mỗi từ
- đưa các ký tự còn lại về lowercase
- bỏ khoảng trắng cuối chuỗi nếu có

Ví dụ:

```text
"  NGUYEN   VAN   AN  " -> "Nguyen Van An"
"le van c"             -> "Le Van C"
"   "                  -> ""
```

### 4.4. Giới hạn hiện tại

- buffer tĩnh dùng chung:
  - `input_buf[1024]`
  - `processed_buf[1024]`
- dữ liệu hữu ích mỗi lượt chỉ tối đa khoảng `1023` byte
- chỉ xử lý kiểu ký tự `ctype.h`, không có logic Unicode hay tiếng Việt có dấu
- state của driver là dùng chung toàn cục, không tách riêng theo session mở file
- header `string_norm.h` khai báo `normalize_string(...)`, nhưng hàm thực thi trong `.c` là `normalize_string_kernel(...)` dạng `static`

## 5. Kernel driver `usb_bridge`

### 5.1. Vai trò

`usb_bridge` là character device thứ hai của dự án, cung cấp API `ioctl()` để:

- mount USB
- unmount USB
- đọc file `.txt` hoặc `.csv`
- ghi file `.txt` hoặc `.csv`
- copy file host <-> USB
- kiểm tra mount point có phải do driver quản lý hay không

Device node mục tiêu:

```bash
/dev/usb_bridge
```

### 5.2. Các `ioctl` chính

Header chung cho kernel và user space là `kernel_module/usb_bridge_ioctl.h`.

Các operation hiện có:

- `USB_OP_MOUNT`
- `USB_OP_UNMOUNT`
- `USB_OP_READ_TEXT`
- `USB_OP_WRITE_TEXT`
- `USB_OP_COPY_HOST_TO_USB`
- `USB_OP_COPY_USB_TO_HOST`
- `USB_OP_CHECK_MOUNT_OWNERSHIP`

### 5.3. Hành vi bảo vệ cơ bản

Driver áp một số kiểm tra đầu vào trước khi đọc/ghi:

- path phải là đường dẫn tuyệt đối
- từ chối path chứa `..`
- giới hạn độ dài path
- chỉ chấp nhận extension `.txt` và `.csv`

Ngoài ra driver có bảng `tracked_mounts` để chỉ cho phép unmount các mount point do chính nó đã mount trước đó.

### 5.4. Cách mount/unmount

Driver không tự mount trực tiếp bằng VFS nội bộ mà gọi helper ở user mode:

- mount qua `/bin/mount`
- unmount qua `/bin/umount`

Sau khi mount thành công, driver ghi nhận mount point vào danh sách theo dõi. Nếu việc track mount thất bại thì driver cố rollback bằng cách unmount lại ngay.

### 5.5. Đọc/ghi/copy file

Phần file I/O trong driver dùng:

- `filp_open()`
- `kernel_read()`
- `kernel_write()`

Copy file dùng buffer từng khối `4096` byte.

### 5.6. Giới hạn quan trọng

- nội dung tối đa cho read/write qua struct hiện là `65536` byte
- extension bị giới hạn chặt ở `.txt` và `.csv`
- mount tracking tối đa `16` mount point
- mọi operation `ioctl()` đi qua một `mutex` chung, không phải multi-session parallel design

## 6. Ứng dụng userspace GTK3

### 6.1. Mục tiêu

Ứng dụng desktop đóng vai trò giao diện chính cho toàn hệ thống. Khi chạy, GUI:

- mở màn hình đăng nhập
- nạp `students.txt` lúc khởi động
- hiển thị danh sách sinh viên trong `GtkTreeView`
- lưu lại `students.txt` khi thoát chương trình

### 6.2. Trạng thái trung tâm

`gui_app.c` dùng một `AppState` để giữ:

- mảng sinh viên đang thao tác trong RAM
- mảng kết quả tìm kiếm
- username và role đang đăng nhập
- các widget GTK quan trọng
- bộ đếm login sai và thời gian lockout

### 6.3. Giao diện chính

Dashboard hiện có các vùng sau:

- sidebar điều hướng
- top bar hiển thị tiêu đề và thông tin user
- bảng danh sách sinh viên
- ô tìm kiếm inline
- form thêm/sửa/xóa
- cụm export
- status bar
- nút mở `USB Storage Manager`
- nút `Quản lý tài khoản`
- nút `Đổi mật khẩu`
- nút `Đăng xuất`

## 7. Đăng nhập, xác thực và phân quyền

### 7.1. File cấu hình tài khoản

Ứng dụng đọc tài khoản từ:

```text
userspace_app/config.txt
```

Schema hiện tại:

```text
username:sha256_hash:role
```

Code vẫn tương thích ngược với format cũ 2 cột:

```text
username:sha256_hash
```

Nếu thiếu cột role thì mặc định role được coi là `admin`.

### 7.2. Tài khoản mẫu đang có trong repo

Theo đúng file `userspace_app/config.txt` hiện tại:

- `admin` / mật khẩu `admin` / role `admin`
- `student` / mật khẩu `123` / role `viewer`

### 7.3. Băm mật khẩu

Module `auth.c` dùng OpenSSL `SHA256()` để băm mật khẩu và lưu dưới dạng chuỗi hex dài 64 ký tự.

Hệ thống không lưu plaintext password trong `config.txt`.

### 7.4. Lockout đăng nhập

Ở GUI:

- nếu nhập sai 3 lần liên tiếp, form login bị khóa `30` giây
- trong thời gian khóa:
  - ô username bị disable
  - ô password bị disable
  - nút login bị disable
- có đếm ngược theo giây
- hết thời gian khóa thì bộ đếm reset

### 7.5. Phân quyền `admin` và `viewer`

Sau khi đăng nhập thành công:

- `admin` dùng đầy đủ chức năng
- `viewer` vẫn được xem dữ liệu nhưng các widget sửa đổi dữ liệu chính bị ẩn

Theo code hiện tại:

- `admin` có thêm/sửa/xóa sinh viên
- `admin` có lưu dữ liệu xuống file
- `admin` thấy nút quản lý tài khoản
- `viewer` vẫn xem danh sách, tìm kiếm, tải file, export CSV/TXT, mở USB manager, đổi mật khẩu và đăng xuất

## 8. Nghiệp vụ sinh viên

### 8.1. Cấu trúc dữ liệu

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

Ý nghĩa:

- `student_code`: mã sinh viên sau chuẩn hóa
- `raw_name`: tên gốc người dùng nhập
- `normalized_name`: tên đã chuẩn hóa
- `student_class`: lớp sau chuẩn hóa
- `dob`: ngày sinh
- `gpa`: điểm hệ 4

### 8.2. Giới hạn dữ liệu

- tối đa `100` sinh viên trong RAM
- `MAX_NAME_LEN = 256`
- `MAX_CODE_LEN = 20`
- `MAX_CLASS_LEN = 20`
- `MAX_DOB_LEN = 15`

### 8.3. Chuẩn hóa dữ liệu

#### Mã sinh viên

- chỉ cho phép chữ và số
- bị loại bỏ khoảng trắng
- được đưa về uppercase

Ví dụ:

```text
" ct07 0310 " -> "CT070310"
```

#### Họ tên

`student.c` làm theo 2 lớp:

1. thử gọi `normalize_via_driver()` qua `/dev/string_norm`
2. nếu driver không sẵn sàng thì fallback sang chuẩn hóa ở user space bằng `sanitize_name_alpha_space()`

Điểm quan trọng:

- fallback thật sự có tồn tại trong code hiện tại
- sau normalize, tên bị lọc chỉ còn chữ cái và khoảng trắng
- chữ số và ký tự đặc biệt bị loại bỏ ở bước sanitize

Ví dụ:

```text
"  @#$NGUYEN123   VAN456   AN!@#  " -> "Nguyen Van An"
```

#### Lớp

- cho phép chữ, số và khoảng trắng ở đầu vào
- khi lưu vào struct sẽ bỏ khoảng trắng và uppercase

Ví dụ:

```text
" ct 07 c " -> "CT07C"
```

#### Ngày sinh

Code chỉ kiểm tra mức cơ bản:

- độ dài phải là `10`
- bắt buộc có dấu `/` tại vị trí `2` và `5`
- parse được dạng `dd/mm/yyyy`
- ngày `1..31`
- tháng `1..12`
- năm `1900..2100`

Chưa có kiểm tra lịch thực như:

- `31/02`
- năm nhuận

#### GPA

- parse bằng `strtof`
- không được có ký tự rác phía sau
- phải nằm trong khoảng `0.0..4.0`

### 8.4. CRUD

`student.c` hiện hỗ trợ:

- `add_student()`
- `delete_student()`
- `edit_student()` cho bản CLI
- `search_student()`
- `sort_by_name()`
- `sort_by_gpa()`

GUI dùng `add_student()` và `delete_student()` trực tiếp, còn logic sửa được xử lý trong `gui_app.c`.

### 8.5. Tìm kiếm

Hàm `search_student()` tìm theo substring trên 3 trường:

- `normalized_name`
- `raw_name`
- `student_code`

Lưu ý:

- tìm kiếm hiện là `strstr()`
- có phân biệt hoa thường
- không phải exact match

### 8.6. Sắp xếp

GUI hiện hỗ trợ:

- tên A-Z
- GPA tăng dần
- GPA giảm dần

## 9. File dữ liệu sinh viên

### 9.1. File mặc định

GUI dùng file mặc định:

```text
userspace_app/students.txt
```

Nội dung mẫu hiện có trong repo:

```text
# code|name|class|dob|gpa
CT07|Tran Quoc D A Yamate|CT07C|27/02/2004|4.00
CT070310|Tran Quoc Toan|CT07C|27/02/2004|4.00
```

### 9.2. Định dạng save nội bộ

`save_to_file()` ghi theo format:

```text
# code|name|class|dob|gpa
MASV|Ten Da Chuan Hoa|LOP|dd/mm/yyyy|x.xx
```

Đặc điểm:

- dùng delimiter `|`
- có dòng comment/header
- lưu `normalized_name`, không lưu `raw_name`

### 9.3. Nạp dữ liệu

`load_from_file()` chấp nhận:

- file nội bộ dùng `|`
- file CSV dùng `,`

Khi đọc file, code sẽ:

- bỏ dòng trống
- bỏ dòng comment bắt đầu bằng `#`
- strip UTF-8 BOM nếu có
- trim khoảng trắng đầu/cuối từng field
- bỏ qua header CSV nếu cột cuối là `GPA`

### 9.4. Chế độ import

Import hiện là strict:

- chỉ cần một dòng sai là từ chối toàn bộ file
- dữ liệu không được nạp từng phần
- mã sinh viên trùng trong file cũng bị từ chối
- mọi field phải qua validate nghiệp vụ

Giá trị trả về:

- `0`: thành công
- `-1`: không mở được file
- `-2`: file sai format hoặc vi phạm rule nghiệp vụ

### 9.5. Export CSV

`export_to_csv()`:

- ghi BOM UTF-8
- dùng header tiếng Việt
- ghi các cột: `Mã SV,Họ và Tên,Lớp,Ngày Sinh,GPA`
- dùng `raw_name` khi export

Điểm này khác với `save_to_file()`, nơi tên được lưu là `normalized_name`.

## 10. USB manager và file I/O USB

### 10.1. Lớp user space

`userspace_app/usb_file.c` là lớp trung gian để thao tác file trên USB. Nó không hoàn toàn phụ thuộc cứng vào driver trong mọi trường hợp.

### 10.2. Hành vi fallback

Theo code hiện tại:

- `usb_write_text_file()`:
  - ưu tiên driver
  - nếu driver ghi lỗi thì fallback sang `libc`
- `usb_read_text_file()`:
  - ưu tiên driver
  - nếu driver đọc lỗi thì fallback sang `libc`
- `usb_mount()` và `usb_mount_detect()`:
  - ưu tiên driver
  - nếu driver không dùng được thì fallback sang `udisksctl`
- `usb_unmount()`:
  - ưu tiên driver
  - rồi thử `udisksctl`
  - cuối cùng thử `umount2()`
- `usb_copy_to_device()` và `usb_copy_from_device()`:
  - chỉ chạy được khi driver `usb_bridge` sẵn sàng

### 10.3. USB Storage Manager trong GUI

Hộp thoại USB Manager hỗ trợ:

- detect USB đã mount
- mount phân vùng USB chưa mount
- unmount
- liệt kê file/thư mục trong mount point
- đọc file text/CSV vào `GtkTextView`
- ghi nội dung text ra file
- copy file `.txt` hoặc `.csv` từ host lên USB
- copy file từ USB về host
- hiển thị log thao tác và bộ đếm read/write

### 10.4. Một nuance quan trọng

Trong GUI, nút `Mount` kiểm tra `usb_driver_available()` ngay từ đầu. Nghĩa là:

- lớp `usb_file.c` có fallback `udisksctl`
- nhưng nút mount trong GUI hiện vẫn yêu cầu `/dev/usb_bridge` tồn tại trước khi bắt đầu luồng mount

Các thao tác detect hoặc đọc/ghi file trên mount point đã có sẵn thì mềm dẻo hơn nhờ fallback ở `usb_file.c`.

## 11. Quản lý tài khoản

GUI admin có dialog quản lý user với các chức năng:

- xem danh sách tài khoản
- thêm user
- xóa user được chọn
- reset mật khẩu user được chọn

Rule hiện có:

- role chỉ có `viewer` hoặc `admin`
- không cho xóa chính tài khoản đang đăng nhập
- reset password là thao tác admin-side, không cần biết mật khẩu cũ

Lưu ý thực tế:

- dialog này không hiển thị lỗi chi tiết cho mọi nhánh thất bại của `add_user()` hoặc `delete_user()`
- nó thiên về thao tác nhanh hơn là UX đầy đủ cho lỗi

## 12. Đổi mật khẩu và audit log

### 12.1. Đổi mật khẩu

User đang đăng nhập có thể đổi mật khẩu từ GUI. Hệ thống kiểm tra:

- không để trống field
- mật khẩu mới và xác nhận phải khớp
- mật khẩu mới phải khác mật khẩu cũ
- mật khẩu cũ phải đúng

### 12.2. Audit log

File log mặc định:

```text
userspace_app/audit.log
```

Format mỗi dòng:

```text
[YYYY-MM-DD HH:MM:SS] [username] action...
```

Các thao tác thường được log:

- đăng nhập sai
- thêm/sửa/xóa sinh viên
- lưu/tải file
- export CSV/TXT
- mount/unmount USB
- read/write/copy USB
- tạo/xóa user
- reset password
- đổi mật khẩu

## 13. Yêu cầu môi trường

Đây là project Linux. Để build và chạy đầy đủ, bạn cần:

- `gcc`
- `make`
- `pkg-config`
- GTK3 development package
- OpenSSL development package
- kernel headers đúng với kernel đang chạy
- quyền `sudo` để nạp/gỡ module và chạy integration test

### 13.1. Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-3-dev libssl-dev linux-headers-$(uname -r)
```

### 13.2. Fedora

```bash
sudo dnf install -y gcc make pkg-config gtk3-devel openssl-devel kernel-devel kernel-headers
```

## 14. Build dự án

### 14.1. Build kernel modules

```bash
cd kernel_module
make
```

Target đang có trong `kernel_module/Makefile`:

- `make`
- `make clean`
- `make load`
- `make load-usb`
- `make load-all`
- `make unload`
- `make unload-usb`
- `make unload-all`
- `make status`

`make load` tạo và cấp quyền `/dev/string_norm`.

`make load-usb` tạo và cấp quyền `/dev/usb_bridge`.

### 14.2. Build ứng dụng GUI

```bash
cd userspace_app
make
```

Binary tạo ra:

```bash
./student_manager_gui
```

### 14.3. Build test

```bash
cd tests
make all
```

## 15. Quy trình chạy khuyến nghị

### Bước 1. Build kernel modules

```bash
cd kernel_module
make
```

### Bước 2. Load cả hai driver

```bash
make load-all
```

### Bước 3. Kiểm tra device node

```bash
make status
```

### Bước 4. Build GUI

```bash
cd ../userspace_app
make
```

### Bước 5. Chạy GUI từ đúng thư mục `userspace_app`

```bash
cd ../userspace_app
./student_manager_gui
```

Điểm này rất quan trọng vì ứng dụng dùng đường dẫn tương đối cho:

- `config.txt`
- `students.txt`
- `style.css`
- `audit.log`

Nếu chạy binary từ thư mục khác, chương trình có thể không tìm thấy dữ liệu hoặc CSS.

## 16. Hướng dẫn sử dụng nhanh

### 16.1. Đăng nhập

Tài khoản mẫu hiện có:

- `admin` / `admin`
- `student` / `123`

### 16.2. Thêm sinh viên

Nhập:

- mã sinh viên
- họ tên
- lớp
- ngày sinh `dd/mm/yyyy`
- GPA trong khoảng `0.0..4.0`

Sau đó bấm `Thêm`.

### 16.3. Sửa sinh viên

- chọn một dòng trong bảng để điền form tự động
- sửa field cần đổi
- bấm `Sửa`

### 16.4. Xóa sinh viên

- chọn dòng trong bảng hoặc nhập mã sinh viên
- bấm `Xóa`
- xác nhận thao tác

### 16.5. Lưu và tải file

- nếu để trống ô file, app dùng `students.txt`
- `Lưu file` ghi snapshot hiện tại xuống file
- `Tải file` thay thế toàn bộ danh sách đang nằm trong RAM

### 16.6. Export

- `Xuất Text (.txt)`: xuất file text theo format nội bộ
- `Xuất Excel (.csv)`: export CSV để mở bằng Excel

### 16.7. USB Storage Manager

Trong sidebar có nút `USB Storage Manager`. Tại đây bạn có thể:

- detect USB
- mount/unmount
- đọc và ghi file text
- copy `.txt` và `.csv` giữa host với USB

## 17. Test

### 17.1. Theo `tests/Makefile`

Các lệnh chính:

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

- `run_mock`: test normalize userspace không cần driver
- `run_auth`: test hash, login, role, add/delete user, change password
- `run_student`: test CRUD, validate, sort, load/save, export CSV
- `run_integration`: test `/dev/string_norm`
- `run_usb_bridge`: test `/dev/usb_bridge`
- `run_all`: gọi script tổng hợp

### 17.2. Trạng thái kiểm tra tôi đã chạy trong workspace

Tôi đã chạy thực tế 3 unit test userspace:

- `make run_mock`: pass `40/40`
- `make run_auth`: fail `1` assertion trên tổng `49`
- `make run_student`: fail `2` assertion trên tổng `78`

Các failure này đến từ test expectation không khớp hoàn toàn với behavior hiện tại của code:

- `test_auth.c` đang mong giá trị SHA-256 sai cho chuỗi `"password"`
- `test_student.c` đang kỳ vọng `"   "` là tên invalid ở `is_valid_name()`, trong khi code hiện tại chỉ kiểm tra ký tự, còn việc chặn blank name được làm ở lớp khác
- `test_student.c` đang kỳ vọng tìm `"an"` ra `2` kết quả, nhưng `search_student()` dùng `strstr()` trên cả `raw_name` lẫn `normalized_name`, nên số match thực tế rộng hơn

Tôi chưa chạy integration test vì các lệnh đó cần `sudo` và driver thật được load trong máy chạy.

### 17.3. Lưu ý về `run_all_tests.sh`

Repo có script `tests/integration_tests/run_all_tests.sh`, nhưng script này hiện không khớp hoàn toàn với `tests/Makefile`:

- một số lệnh compile trong script chưa link đủ source userspace
- vì vậy nên ưu tiên chạy test qua `make` trong `tests/` trước

## 18. Các hạn chế hiện tại của dự án

- xử lý tên chưa hỗ trợ Unicode tiếng Việt đầy đủ
- driver `string_norm` dùng buffer toàn cục dùng chung
- dữ liệu sinh viên dùng mảng tĩnh tối đa `100` bản ghi
- validate ngày sinh chỉ ở mức cơ bản
- import file là strict, sai một dòng là fail cả file
- search đang là substring search có phân biệt hoa thường
- GUI phụ thuộc đường dẫn tương đối
- dialog quản lý tài khoản chưa hiển thị đầy đủ lỗi nghiệp vụ
- USB copy chỉ chạy được khi `usb_bridge` hoạt động
- repo hiện chứa cả source và artifact build, không phải cây mã sạch tối giản

## 19. Khắc phục sự cố nhanh

### Không thấy `/dev/string_norm` hoặc `/dev/usb_bridge`

```bash
cd kernel_module
make load-all
make status
```

### GUI chạy nhưng không có CSS hoặc không đọc được dữ liệu

Hãy chắc chắn bạn chạy từ đúng thư mục:

```bash
cd userspace_app
./student_manager_gui
```

### Không đăng nhập được

Kiểm tra `userspace_app/config.txt` theo format:

```text
username:sha256_hash:role
```

### Import file bị từ chối

Nguyên nhân thường gặp:

- sai delimiter
- sai số cột
- tên chứa ký tự không hợp lệ
- mã sinh viên hoặc lớp chứa ký tự cấm
- DOB sai format
- GPA ngoài `0.0..4.0`
- có dòng trùng mã sinh viên

### USB Manager mount không chạy

Kiểm tra:

- đã load `usb_bridge`
- có `/dev/usb_bridge`
- mount point trong `/tmp` tạo được
- hệ thống có `udisksctl` nếu cần fallback

## 20. Kết luận

Đây là một project C/Linux khá đầy đủ ở mức đồ án hệ thống:

- có kernel module thật
- có GUI thật
- có phân quyền
- có file-based persistence
- có test
- có audit
- có luồng USB riêng

Điểm mạnh của repo là ghép được nhiều lớp từ kernel đến desktop app trong cùng một bài toán. Điểm cần lưu ý là một số test và script phụ hiện chưa đồng bộ hoàn toàn với behavior mới của mã nguồn, nên khi dùng làm tài liệu hoặc demo nên bám vào source và `Makefile` hiện tại.
