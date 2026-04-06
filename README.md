# Student Management System với Linux Character Driver và GTK3

Đây là dự án quản lý sinh viên viết bằng C, kết hợp giữa:

- một Linux character device driver để chuẩn hóa chuỗi trong kernel space;
- một ứng dụng GTK3 chạy ở user space để đăng nhập, quản lý sinh viên, import/export dữ liệu và thao tác file text trên USB;
- một bộ test tách riêng cho phần logic userspace và cho luồng I/O với driver.

README này được viết lại theo đúng hiện trạng mã nguồn trong repository tại thời điểm hiện tại, không giả định thêm những tính năng chưa có trong code.

## 1. Mục tiêu dự án

Dự án minh họa cách kết hợp nhiều lớp trong một ứng dụng hệ thống hoàn chỉnh:

- `kernel_module/`: xây dựng driver ký tự `/dev/string_norm`;
- `userspace_app/`: xây dựng ứng dụng desktop GTK3 dùng driver đó để chuẩn hóa tên sinh viên;
- `tests/`: kiểm thử các hàm xử lý nghiệp vụ và kiểm tra giao tiếp thật với device node.

Ý tưởng trung tâm của project là:

1. người dùng nhập dữ liệu sinh viên ở GUI;
2. tên sinh viên được đưa qua driver `/dev/string_norm` để chuẩn hóa;
3. dữ liệu sau khi hợp lệ được lưu trong RAM, có thể ghi ra file text, xuất CSV hoặc thao tác với file text trên USB;
4. các hành động quan trọng được ghi vào `audit.log`.

## 2. Kiến trúc tổng thể

### 2.1. Sơ đồ luồng

```text
GTK3 GUI
   |
   |-- auth.c       -> xác thực tài khoản, phân quyền admin/viewer
   |-- student.c    -> CRUD, validate, save/load, export CSV
   |                    |
   |                    -> /dev/string_norm (kernel module)
   |
   |-- usb_file.c   -> đọc/ghi file text tại thư mục mount USB
   |
   |-- audit.c      -> ghi nhật ký thao tác
```

### 2.2. Ba khối chính

#### `kernel_module`

Driver ký tự `string_norm` nhận chuỗi từ user space qua `write()`, chuẩn hóa chuỗi trong kernel rồi trả kết quả lại bằng `read()`.

#### `userspace_app`

Ứng dụng chính là `student_manager_gui`, được build từ `gui_app.c` cùng các module phụ:

- `auth.c`: xác thực tài khoản, băm SHA-256, phân quyền, đổi mật khẩu, quản lý user;
- `student.c`: thêm/sửa/xóa/tìm/sắp xếp sinh viên, load/save file, export CSV;
- `usb_file.c`: đọc/ghi file text trong một thư mục mount do người dùng cung cấp;
- `audit.c`: ghi audit log.

#### `tests`

Gồm 2 nhóm:

- `userspace_tests/`: unit test cho logic userspace;
- `integration_tests/`: test giao tiếp thật với `/dev/string_norm`.

## 3. Cấu trúc thư mục

```text
.
├─ kernel_module/
│  ├─ Makefile
│  ├─ string_norm.c
│  ├─ string_norm.h
│  ├─ string_norm.ko
│  └─ các artifact build kernel khác
├─ userspace_app/
│  ├─ Makefile
│  ├─ gui_app.c
│  ├─ student.c
│  ├─ student.h
│  ├─ auth.c
│  ├─ auth.h
│  ├─ usb_file.c
│  ├─ usb_file.h
│  ├─ audit.c
│  ├─ audit.h
│  ├─ style.css
│  ├─ config.txt
│  ├─ students.txt
│  ├─ audit.log
│  └─ student_manager_gui
└─ tests/
   ├─ Makefile
   ├─ userspace_tests/
   │  ├─ test_auth.c
   │  ├─ test_student.c
   │  └─ test_normalize_mock.c
   └─ integration_tests/
      ├─ test_driver_io.c
      └─ run_all_tests.sh
```

Lưu ý:

- repository hiện đang chứa sẵn cả source lẫn một số file build (`.ko`, binary test, binary GUI, object/module files);
- đó là trạng thái hiện tại của repo, không phải toàn bộ các file này đều là source gốc.

## 4. Kernel module: `string_norm`

### 4.1. Vai trò

Driver chịu trách nhiệm chuẩn hóa chuỗi nhập từ user space. Chuỗi sau xử lý được dùng để tạo `normalized_name` cho sinh viên.

### 4.2. Device node

Sau khi nạp module đúng cách, thiết bị được truy cập qua:

```bash
/dev/string_norm
```

### 4.3. Hành vi chuẩn hóa

Theo code hiện tại trong `kernel_module/string_norm.c`, driver:

- bỏ khoảng trắng đầu chuỗi;
- gộp nhiều khoảng trắng liên tiếp thành một khoảng trắng;
- viết hoa ký tự đầu mỗi từ;
- đưa các ký tự còn lại về lowercase;
- bỏ khoảng trắng cuối chuỗi nếu có.

Ví dụ:

```text
"  NGUYEN   VAN   AN  " -> "Nguyen Van An"
"le van c"             -> "Le Van C"
"   "                  -> ""
```

### 4.4. Cơ chế hoạt động

Driver dùng:

- `alloc_chrdev_region()` để cấp major/minor động;
- `cdev_add()` để đăng ký character device;
- `class_create()` và `device_create()` để tạo thiết bị trong `/dev`;
- `copy_from_user()` và `copy_to_user()` để trao đổi dữ liệu;
- `mutex` để bảo vệ vùng đệm dùng chung.

### 4.5. Bộ đệm nội bộ

Trong driver có 2 buffer tĩnh:

- `input_buf[1024]`
- `processed_buf[1024]`

Điều này có nghĩa:

- mỗi lần `write()` chỉ lưu được tối đa `1023` byte dữ liệu hữu ích;
- kết quả `read()` cũng bị giới hạn bởi kích thước buffer này.

### 4.6. Giới hạn hiện tại của driver

Theo code hiện tại:

- driver chỉ xử lý theo kiểu ký tự C cơ bản với `ctype.h`;
- không có logic Unicode/Vietnamese-aware;
- không hỗ trợ nhiều request song song tách biệt theo từng session, vì dùng buffer dùng chung toàn cục;
- `string_norm.h` khai báo `normalize_string(...)`, nhưng implementation thực tế trong `.c` là `normalize_string_kernel(...)` dạng `static`; header này hiện gần như không được dùng đúng theo tên hàm công khai.

## 5. Userspace application

### 5.1. Mục tiêu

Ứng dụng GTK3 cung cấp giao diện quản lý sinh viên có đăng nhập và phân quyền. GUI không chỉ hiển thị dữ liệu mà còn đóng vai trò orchestration giữa các module nghiệp vụ.

### 5.2. Thành phần chính

#### `gui_app.c`

Đây là file điều phối chính. Nó quản lý:

- đăng nhập;
- danh sách sinh viên đang nằm trong RAM;
- bảng hiển thị dữ liệu;
- form thêm/sửa;
- file import/export;
- thao tác USB;
- quản lý user;
- trạng thái theo role.

#### `student.c`

Chứa phần nghiệp vụ lõi cho sinh viên:

- validate tên;
- chuẩn hóa mã sinh viên và lớp;
- gọi driver để chuẩn hóa tên;
- thêm, xóa, sửa, tìm kiếm, sắp xếp;
- save/load file;
- export CSV.

#### `auth.c`

Chứa logic xác thực:

- băm SHA-256 bằng OpenSSL;
- đăng nhập theo `config.txt`;
- đọc role;
- đổi mật khẩu;
- reset mật khẩu bởi admin;
- thêm/xóa user;
- liệt kê danh sách user.

#### `usb_file.c`

Không giao tiếp với USB driver riêng. Module này chỉ thao tác file text trong một thư mục mount do người dùng nhập.

#### `audit.c`

Ghi log thao tác ra file `audit.log`.

## 6. Tính năng hiện có

### 6.1. Xác thực và phân quyền

Hệ thống hỗ trợ 2 role:

- `admin`
- `viewer`

Sau khi đăng nhập thành công:

- `admin` nhìn thấy và dùng được các chức năng thay đổi dữ liệu;
- `viewer` vẫn xem danh sách, tìm kiếm, tải dữ liệu, export dữ liệu, nhưng các thao tác quản trị bị ẩn hoặc vô hiệu hóa theo code GUI hiện tại.

### 6.2. Đăng nhập với lockout

Tại GUI:

- nếu nhập sai 3 lần liên tiếp, form đăng nhập bị khóa 30 giây;
- trong thời gian khóa, nút đăng nhập và ô nhập bị disable;
- có hiển thị đếm ngược;
- khi hết thời gian khóa, người dùng có thể thử lại.

### 6.3. Quản lý sinh viên

Các thao tác hiện có:

- thêm sinh viên;
- sửa sinh viên;
- xóa sinh viên;
- chọn dòng trong bảng để tự đổ dữ liệu vào form;
- tìm kiếm theo mã sinh viên, tên gốc hoặc tên chuẩn hóa;
- sắp xếp theo tên A-Z;
- sắp xếp GPA tăng dần;
- sắp xếp GPA giảm dần.

### 6.4. Lưu và tải dữ liệu

GUI cho phép:

- lưu danh sách hiện tại ra file text;
- tải danh sách từ file text hoặc file CSV đúng schema mà ứng dụng chấp nhận;
- nếu đang có dữ liệu trên RAM, thao tác tải sẽ hỏi xác nhận vì nó thay thế toàn bộ danh sách hiện có.

### 6.5. Export

Ứng dụng hỗ trợ:

- xuất text bằng `save_to_file(...)`;
- xuất CSV bằng `export_to_csv(...)`.

### 6.6. USB text I/O

GUI có vùng riêng để:

- nhập đường dẫn mount USB;
- nhập tên file;
- ghi nội dung từ `GtkTextView` ra file;
- đọc file text vào lại vùng text đó.

Đây là file I/O ở user space, không phải USB kernel driver.

### 6.7. Quản lý user

Trong dialog quản lý tài khoản, admin có thể:

- xem danh sách user và role;
- thêm tài khoản mới;
- xóa tài khoản đang chọn;
- reset mật khẩu cho tài khoản đang chọn.

Theo GUI hiện tại:

- admin không được xóa chính tài khoản đang đăng nhập;
- role được chọn từ combo box giữa `viewer` và `admin`.

### 6.8. Đổi mật khẩu

Người dùng đang đăng nhập có thể:

- nhập mật khẩu cũ;
- nhập mật khẩu mới;
- xác nhận mật khẩu mới.

Luồng đổi mật khẩu có kiểm tra:

- không được để trống;
- mật khẩu mới phải khớp xác nhận;
- mật khẩu mới phải khác mật khẩu cũ;
- mật khẩu cũ phải đúng.

### 6.9. Audit log

Các hành động quan trọng đều được ghi lại, ví dụ:

- đăng nhập thất bại;
- thêm/sửa/xóa sinh viên;
- lưu/tải file;
- đọc/ghi USB;
- export;
- tạo/xóa user;
- reset mật khẩu;
- đổi mật khẩu.

## 7. Mô hình dữ liệu sinh viên

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

Ý nghĩa các trường:

- `student_code`: mã sinh viên đã được chuẩn hóa về uppercase, bỏ khoảng trắng;
- `raw_name`: tên gốc người dùng nhập;
- `normalized_name`: tên sau khi đi qua driver rồi được lọc lại ở user space;
- `student_class`: lớp, được chuẩn hóa về uppercase và bỏ khoảng trắng;
- `dob`: ngày sinh định dạng `dd/mm/yyyy`;
- `gpa`: điểm hệ 4.

### 7.1. Giới hạn kích thước

Theo header hiện tại:

- tối đa `100` sinh viên trong RAM;
- mã sinh viên tối đa `20` ký tự;
- tên tối đa `256` ký tự;
- lớp tối đa `20` ký tự;
- ngày sinh tối đa `15` ký tự buffer, nhưng validate thực tế yêu cầu đúng chuỗi dài 10 ký tự dạng `dd/mm/yyyy`.

## 8. Quy tắc validate và chuẩn hóa dữ liệu

### 8.1. Mã sinh viên

Trong GUI và lúc load file:

- chỉ chấp nhận chữ và số;
- khi lưu vào struct sẽ được chuẩn hóa về uppercase;
- khoảng trắng bị loại bỏ.

Ví dụ:

```text
" ct07 0302 " -> "CT070302"
```

### 8.2. Họ tên

Theo logic hiện tại:

- `is_valid_name()` chỉ cho phép chữ cái và khoảng trắng;
- nếu tên chứa số hoặc ký tự đặc biệt thì bị từ chối;
- sau đó tên được gửi xuống driver để chuẩn hóa;
- kết quả từ driver lại được lọc lần nữa ở user space để chỉ giữ chữ cái và khoảng trắng.

Điểm quan trọng:

- hàm `normalize_name_best_effort(...)` trong code hiện tại thực tế không fallback sang user space nếu driver lỗi;
- nếu không mở được `/dev/string_norm`, hàm trả lỗi luôn;
- vì vậy thao tác thêm/sửa/load dữ liệu phụ thuộc vào việc driver sẵn sàng.

Tên gọi `best_effort` trong code không phản ánh đúng hành vi thực tế hiện tại.

### 8.3. Lớp

Lớp:

- được chấp nhận nếu chỉ gồm chữ, số, khoảng trắng;
- khi ghi vào struct sẽ bị bỏ khoảng trắng và đổi sang uppercase.

Ví dụ:

```text
" ct 07 c " -> "CT07C"
```

### 8.4. Ngày sinh

Ứng dụng chỉ kiểm tra ở mức cơ bản:

- đúng độ dài `10`;
- có dấu `/` tại vị trí `2` và `5`;
- parse được dạng `dd/mm/yyyy`;
- ngày trong khoảng `1..31`;
- tháng trong khoảng `1..12`;
- năm trong khoảng `1900..2100`.

Chưa có kiểm tra lịch thực tế sâu như:

- ngày 31 của tháng 2;
- năm nhuận.

### 8.5. GPA

GPA phải:

- parse được bằng `strtof`;
- không có rác phía sau;
- nằm trong khoảng `0.0` đến `4.0`.

## 9. File dữ liệu và định dạng

### 9.1. `students.txt`

File mặc định của app là:

```text
userspace_app/students.txt
```

Ví dụ hiện có trong repo:

```text
# code|name|class|dob|gpa
CT070302|Bui Duc Anh|CT07C|27/02/2004|3.40
CT070308|Tran Hai Dang|CT07C|27/12/2003|3.50
CT070310|Tran Quoc Dat|CT07C|27/02/2004|3.20
```

### 9.2. Định dạng save nội bộ

`save_to_file(...)` ghi theo schema:

```text
# code|name|class|dob|gpa
MASV|Ten Da Chuan Hoa|LOP|dd/mm/yyyy|x.xx
```

Đặc điểm:

- dùng dấu `|`;
- có dòng header comment đầu file;
- lưu `normalized_name`, không lưu `raw_name`.

### 9.3. Load file

`load_from_file(...)` hiện chấp nhận:

- file phân tách bằng `|`;
- hoặc file phân tách bằng `,`.

Nó cũng:

- bỏ qua dòng trống;
- bỏ qua dòng comment bắt đầu bằng `#`;
- bỏ BOM UTF-8 nếu có;
- trim khoảng trắng đầu/cuối từng field.

### 9.4. Chế độ strict khi import

`load_from_file(...)` đang chạy theo kiểu strict:

- chỉ cần một dòng sai format hoặc sai rule nghiệp vụ là từ chối toàn bộ file;
- mã sinh viên trùng trong file cũng làm import thất bại;
- nếu driver không sẵn sàng để chuẩn hóa tên, import cũng thất bại.

Khi lỗi định dạng/nghiệp vụ, hàm trả `-2`.

### 9.5. Export CSV

`export_to_csv(...)`:

- ghi BOM UTF-8 ở đầu file;
- dùng header tiếng Việt;
- ghi các cột: mã SV, họ và tên, lớp, ngày sinh, GPA;
- dùng `raw_name` khi export CSV.

Điểm này khác với `save_to_file(...)`, nơi tên được lưu dưới dạng `normalized_name`.

## 10. Xác thực và quản lý tài khoản

### 10.1. File cấu hình tài khoản

File tài khoản mặc định:

```text
userspace_app/config.txt
```

Schema:

```text
username:sha256_hash:role
```

Code hiện tại cũng tương thích ngược với format cũ 2 cột:

```text
username:sha256_hash
```

Trong trường hợp đó, role mặc định được coi là `admin`.

### 10.2. Tài khoản mẫu đang có trong repo

Theo file hiện tại:

- `admin` / mật khẩu `admin` / role `admin`
- `student` / mật khẩu `123` / role `viewer`

### 10.3. Băm mật khẩu

Mật khẩu được băm bằng SHA-256 qua OpenSSL và lưu dưới dạng chuỗi hex 64 ký tự.

Ứng dụng không lưu plaintext password trong `config.txt`.

## 11. Audit log

File log mặc định:

```text
userspace_app/audit.log
```

Mỗi dòng theo dạng:

```text
[YYYY-MM-DD HH:MM:SS] [username] action...
```

Ví dụ:

```text
[2026-04-07 12:34:56] [admin] Added student: CT070302
```

## 12. Yêu cầu môi trường

Đây là project Linux. Nếu đang dùng Windows, nên chạy qua:

- WSL2;
- máy ảo Linux;
- hoặc máy Linux thật.

### 12.1. Công cụ cần có

- `gcc`
- `make`
- `pkg-config`
- `libgtk-3-dev` hoặc gói tương đương
- `libssl-dev` hoặc gói tương đương
- Linux kernel headers khớp kernel đang chạy
- quyền `sudo` để nạp/gỡ module và chạy integration test

### 12.2. Gợi ý cài trên Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-3-dev libssl-dev linux-headers-$(uname -r)
```

### 12.3. Gợi ý cài trên Fedora

```bash
sudo dnf install -y gcc make pkg-config gtk3-devel openssl-devel kernel-devel kernel-headers
```

## 13. Build dự án

### 13.1. Build kernel module

```bash
cd kernel_module
make
```

Các target đang có:

- `make`: build module;
- `make load`: nạp module, tạo `/dev/string_norm`, chmod `666`;
- `make unload`: gỡ module và xóa device node;
- `make status`: xem trạng thái module/device/log;
- `make clean`: dọn artifact.

### 13.2. Build GUI userspace

```bash
cd userspace_app
make
```

Binary chính:

```bash
./student_manager_gui
```

hoặc:

```bash
make run-gui
```

### 13.3. Build test

```bash
cd tests
make all
```

## 14. Quy trình chạy khuyến nghị

Đây là thứ tự nên dùng để chạy project đúng cách:

### Bước 1. Build và nạp driver

```bash
cd kernel_module
make
sudo make load
```

### Bước 2. Kiểm tra device node

```bash
ls -l /dev/string_norm
```

hoặc:

```bash
cd kernel_module
make status
```

### Bước 3. Build GUI

```bash
cd ../userspace_app
make
```

### Bước 4. Chạy ứng dụng từ đúng thư mục `userspace_app`

```bash
cd ../userspace_app
./student_manager_gui
```

Điểm này quan trọng vì code đang mở các file tương đối:

- `config.txt`
- `students.txt`
- `style.css`
- `audit.log`

Nếu chạy binary từ thư mục khác, các file tương đối này có thể không được tìm thấy.

## 15. Hướng dẫn sử dụng nhanh

### 15.1. Đăng nhập

Đăng nhập bằng tài khoản mẫu:

- `admin` / `admin`
- hoặc `student` / `123`

### 15.2. Thêm sinh viên

Nhập:

- mã sinh viên;
- họ tên;
- lớp;
- ngày sinh `dd/mm/yyyy`;
- GPA.

Sau đó bấm `Thêm`.

### 15.3. Sửa sinh viên

Chọn một dòng trong bảng để đổ dữ liệu vào form, sửa các trường cần thiết rồi bấm `Sửa`.

### 15.4. Xóa sinh viên

Chọn dòng hoặc nhập mã sinh viên, sau đó bấm `Xóa` và xác nhận.

### 15.5. Tìm kiếm

Nhập từ khóa vào ô tìm kiếm. Code hiện tại tìm theo:

- `student_code`
- `raw_name`
- `normalized_name`

### 15.6. Lưu và tải file

- để trống ô file thì app mặc định dùng `students.txt`;
- nhập đường dẫn file khác nếu muốn save/load file riêng.

### 15.7. Export

- `Xuất Text (.txt)`: lưu danh sách ra file text theo format nội bộ;
- `Xuất Excel (.csv)`: xuất CSV có BOM để mở Excel dễ hơn.

### 15.8. USB

Nhập:

- thư mục mount;
- tên file;
- nội dung text.

Rồi chọn:

- `Ghi USB`
- hoặc `Đọc USB`

## 16. Test

### 16.1. Unit test userspace

Các target hiện có trong `tests/Makefile`:

```bash
cd tests
make run_mock
make run_auth
make run_student
```

Ý nghĩa:

- `run_mock`: test logic chuẩn hóa tên với mock driver;
- `run_auth`: test SHA-256, authenticate, role, add/delete user, đổi mật khẩu;
- `run_student`: test validate tên, add student, validate GPA, load file strict.

### 16.2. Integration test với driver thật

```bash
cd tests
make run_integration
```

Test này:

- mở `/dev/string_norm`;
- `write()` chuỗi vào driver;
- `read()` kết quả chuẩn hóa;
- so sánh với output mong đợi.

Nó cần:

- driver đã sẵn sàng;
- có quyền `sudo`.

### 16.3. Full test script

```bash
cd tests/integration_tests
chmod +x run_all_tests.sh
sudo ./run_all_tests.sh
```

Script sẽ:

1. build kernel module;
2. nạp driver;
3. compile và chạy mock/auth/student tests;
4. compile và chạy integration test;
5. in kernel log liên quan;
6. cleanup driver.

## 17. Các điểm cần lưu ý khi chạy thực tế

### 17.1. Driver là phụ thuộc bắt buộc cho các thao tác có chuẩn hóa tên

Theo code hiện tại:

- thêm sinh viên cần driver;
- sửa tên sinh viên cần driver;
- load file cần driver;
- nếu không mở được `/dev/string_norm`, các thao tác đó sẽ lỗi.

### 17.2. Hàm `normalize_name_best_effort(...)` không thật sự fallback

Tên hàm nghe như có fallback, nhưng logic hiện tại trả lỗi ngay khi driver không sẵn sàng.

### 17.3. Load file là thay thế toàn bộ danh sách trong RAM

Khi import thành công:

- danh sách cũ bị thay hoàn toàn;
- GUI có xác nhận trước nếu đang có dữ liệu.

### 17.4. Tìm kiếm đang là substring search

`search_student(...)` dùng `strstr(...)`, nên:

- không phải tìm kiếm chính xác tuyệt đối;
- nhập một phần tên hoặc một đoạn mã vẫn có thể match.

### 17.5. GUI dùng đường dẫn tương đối

Nếu chạy sai working directory, app có thể:

- không đọc được `config.txt`;
- không đọc được `students.txt`;
- không load được `style.css`;
- không ghi được `audit.log` đúng chỗ mong muốn.

## 18. Hạn chế hiện tại của dự án

README nên nói rõ các điểm này vì chúng là hành vi thật của code hiện nay:

- validate ngày sinh chưa kiểm tra lịch thật;
- xử lý tên chưa hỗ trợ Unicode tiếng Việt đầy đủ;
- dữ liệu sinh viên lưu bằng mảng tĩnh, tối đa 100 bản ghi;
- user management ở GUI không hiển thị lỗi chi tiết nếu `add_user(...)` thất bại;
- driver dùng buffer toàn cục dùng chung;
- project lưu cấu hình và dữ liệu dạng file text đơn giản, chưa có cơ chế transaction hay backup;
- `userspace_app/student.h` hiện có comment bị lỗi encoding trong file nguồn;
- repository đang chứa cả artifact build, dễ gây nhiễu nếu dùng như source tree sạch.

## 19. Khắc phục sự cố nhanh

### Không thêm/sửa/load được vì lỗi driver

Kiểm tra:

```bash
ls -l /dev/string_norm
```

Nếu chưa có:

```bash
cd kernel_module
sudo make load
```

### Không mở được GUI hoặc build lỗi GTK

Kiểm tra đã cài gói phát triển GTK3:

```bash
pkg-config --cflags gtk+-3.0
pkg-config --libs gtk+-3.0
```

### Không xác thực được tài khoản

Kiểm tra file:

```text
userspace_app/config.txt
```

Schema phải là:

```text
username:sha256_hash:role
```

### Import file bị từ chối

Nguyên nhân thường gặp:

- sai delimiter;
- sai số cột;
- mã sinh viên không hợp lệ;
- tên chứa ký tự không hợp lệ;
- lớp không hợp lệ;
- ngày sinh sai format;
- GPA ngoài khoảng `0.0..4.0`;
- mã sinh viên bị trùng;
- driver không sẵn sàng để chuẩn hóa tên.

## 20. Trạng thái kiểm tra trong môi trường hiện tại

Tôi đã rà mã nguồn để viết lại README này theo hiện trạng thật của project.

Riêng việc build/test trực tiếp trong môi trường hiện tại chưa thực hiện được vì shell đang báo không có `make` trong PATH. Do đó:

- nội dung README về build/test được đối chiếu từ `Makefile`, script test và source code;
- nếu bạn chạy trên Linux hoặc WSL có cài đủ toolchain, các lệnh trong phần trên là quy trình đúng theo repository hiện tại.
