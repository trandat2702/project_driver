# 🚀 Student Management System (Linux Kernel & Userspace Project)

> **Mô tả:** Hệ thống Quản lý Sinh viên toàn diện trên Linux nhân hệ điều hành CentOS 64-bit. Dự án kết hợp liền mạch giữa **Kernel Space** (Device Driver xử lý logic) và **Userspace** (Ứng dụng giao diện GTK3). 
> **Tiêu chí nổi bật:** Thiết kế chuẩn bảo mật phân quyền (RBAC), chống tấn công nạp dữ liệu (Data poisoning), tự động kết nối thiết bị ngoại vi lưu trữ (USB), hỗ trợ Xuất báo cáo Excel và Ghi nhật ký hệ thống (Audit Log).
> **Hệ điều hành:** CentOS 64-bit.

---

## 🛠️ 1. Môi trường & Ngôn ngữ
- **Kernel:** Linux Kernel 6.x+ (CentOS / RHEL 9+)
- **Ngôn ngữ:** Lập trình C (GCC 11+)
- **Giao diện (GUI):** GTK+ 3.0
- **Bảo mật:** Thư viện OpenSSL 3.x (Mã hóa SHA-256)
- **Kernel Driver:** Module Character Device `string_norm` (`/dev/string_norm`)

**Lệnh cài đặt môi trường cần thiết:**
```bash
sudo dnf install -y kernel-devel kernel-headers gcc make pkg-config gtk3-devel openssl-devel
```

---

## 📂 2. Cấu trúc Dự Án

*   `kernel_module/`: Chứa file mã nguồn `string_norm.c` xây dựng Linux Kernel Driver chuyên biệt làm nhiệm vụ chuẩn hóa xâu (Normalizer) - Xóa khoảng trắng thừa và viết hoa chữ cái.
*   `userspace_app/`: Toàn bộ lõi ứng dụng không gian người dùng.
    *   `gui_app.c`: Khung giao diện cửa sổ GTK3.
    *   `auth.c` / `auth.h`: Xử lý bảo mật băm SHA-256 và cấp quyền đăng nhập.
    *   `audit.c` / `audit.h`: Hệ thống Ghi nhật ký (Audit Log) theo dõi toàn bộ hoạt động.
    *   `student.c` / `student.h`: Cấu trúc dữ liệu sinh viên, Thêm/Sửa/Xóa và xuất Excel.
*   `tests/`: Bộ Unit/Integration Test tự động Python+C (90+ Test Cases).
*   `.agents/workflows/`: Kho lệnh tự động phục vụ demo.

---

## ⚡ 3. Thao tác Nhanh (Slash Commands)

Dự án được kết hợp các Shell Workflows độc đáo để quá trình Biên dịch - Khởi động - Tải Driver chỉ nằm trong 1 cú click (Thay vì gõ thủ công từng lệnh make và insmod):

- **`/setup-project`**: Lệnh khởi tạo A-Z (Gỡ mọi phiên bản cũ, Build driver, Tải Kernel module mới, Build app). Dùng 1 lần duy nhất khi vừa tải code về.
- **`/run-gui`**: Biên dịch phần mềm Userspace và hiển thị màn hình phần mềm Quản lý.
- **`/unload-driver`**: Gỡ module Kernel (`rmmod`) và xóa nút `/dev/string_norm`.
- **`/run-tests`**: Thực thi toàn bộ hộp kiểm thử để đánh giá.

---

## 🌟 4. Các Tính Năng Đột Phá

### 🔐 1. Bảo Mật Đăng Nhập & Phân Quyền (RBAC)
*   **SHA-256 Hashing:** Tuyệt đối không lưu mật khẩu thô. Ngăn chặn Brute-force (khóa 30 giây khi sai 3 lần).
*   **Role-Based Access Control:**
    *   **Admin (`admin` / `admin`)**: Đầy đủ mọi quyền (Thêm, Sửa, Xóa, Lưu vào cấu hình, Tạo User mới, Xóa User khác).
    *   **Viewer (`student` / `123`)**: Chỉ có quyền "Xem dữ liệu" và tải danh sách hiển thị, tải file từ USB. Các phím tương tác phá hoại bị khóa cưỡng chế.

### 🛡️ 2. Lõi Lọc Dữ Liệu Chống Ô Nhiễm (Anti-Data Poisoning)
*   Mọi thông tin nạp từ USB hay File Text ngoài không được nạp trực tiếp! Lõi sẽ quét từng dòng. Nếu phát hiện điền sai cấu trúc ngày sinh (`dd/mm/yyyy`), điểm GPA vô lý (`> 4.0`), hoặc bị trùng lặp **Mã Sinh Viên**, phần mềm sẽ chủ động chặn dòng dữ liệu đó, bảo vệ tuổi thọ của CSDL.

### 🖥️ 3. Giao diện Đồ Họa Cửa Sổ Hấp Dẫn (GTK3)
*   Sử dụng CSS Native với hệ lưới linh hoạt. Màu chủ đạo **Aurora Dark Mode**.
*   Thanh Sidebar trực quan có điều hướng rõ ràng, thanh Topbar hiển thị tên người dùng và vai trò.

### 📊 4. Xuất Báo Cáo Excel (CSV UTF-8)
*   Hỗ trợ xuất danh sách toàn trường ra một file `.csv` tích hợp sẵn mã định dạng **UTF-8 BOM**. Mở bằng Excel trên Windows, MacOS sẽ không bao giờ bị lỗi font tiếng Việt.

### 👁️ 5. Mắt Thần Hệ Thống (Audit Log)
*   File `audit.log` ngầm định ghi nhận từng mili-giây đối với các hành vi của người dùng: *Ai là người thêm sinh viên A, ai vừa thay đổi mật khẩu, đăng nhập sai lúc mấy giờ?*

---

## 🧪 5. Kịch Bản Demo (Cho Chấm Điểm)

### Kịch bản 1: Phô diễn sức mạnh Kernel Driver
1. Mở app, thêm thử 1 sinh viên có tên gõ sai định dạng: `  nGuyen     VAn     a  `.
2. Lưu. Danh sách lập tức xuất hiện dòng chữ gọn gàng `Nguyen Van A`. Dữ liệu này thực chất đã chạy một vòng từ Userspace "chọc" thẳng xuống Kernel Mode qua `/dev/string_norm` rồi mới quay lại hiển thị bảng.

### Kịch bản 2: Bảo vệ bằng Phân Quyền (RBAC)
1. Dùng quyền Admin, bấm **Quản lý Tài Khoản**, tạo tài khoản `khach` mật khẩu `khach`, Role = `Viewer`.
2. Log out ra.
3. Đăng nhập lại bằng tài khoản `khach`. Các nút Xóa / Lưu báo cáo đều không thể bấm được. Tính bảo mật cực cao.

### Kịch bản 3: Sức mạnh I/O Ngoại Vi & Xuất Excel
1. Cắm USB vào máy CentOS.
2. Tại phần mềm, chọn thư mục Mount đường dẫn USB (VD: `/run/media/dat/USB`).
3. Ấn **Xuất Excel (.csv)**. Rút USB ra, cắm vào máy tính Windows của Giảng viên mở lên xem thành quả bảng xếp điểm tự động chia cột rõ ràng.

---
*Phát triển và Hoàn thiện bởi Đạt & Nhóm trợ lý Deepmind (Antigravity).*
