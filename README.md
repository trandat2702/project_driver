# 🚀 Student Management System (Linux Driver Project)

> **Mô tả:** Hệ thống Quản lý Sinh viên tích hợp Driver Kernel để chuẩn hóa xâu, cơ chế đăng nhập bảo mật SHA-256, giao diện GUI hiện đại (GTK3) và hỗ trợ thao tác File I/O qua thiết bị USB. 
> **Hệ điều hành:** CentOS 64-bit.

---

## 🛠️ 1. Môi trường & Yêu cầu
- **Kernel:** Linux Kernel 6.x+ (CentOS Stream 10 / RHEL 9+)
- **Ngôn ngữ:** C (GCC 11+)
- **Thư viện GUI:** GTK+ 3.0
- **Bảo mật:** OpenSSL 3.x (SHA-256)
- **Driver:** Kernel Module `string_norm` (/dev/string_norm)

Lệnh cài đặt môi trường:
```bash
sudo dnf install -y kernel-devel kernel-headers gcc make pkg-config gtk3-devel openssl-devel
```

---

## 📂 2. Cấu trúc Dự Án
- `kernel_module/`: Driver chuẩn hóa chuỗi thực thi trong Kernel Space.
- `userspace_app/`: Ứng dụng chính (GUI & CLI), xử lý logic nghiệp vụ và Auth.
- `usb_driver/`: Module giám sát thiết bị USB.
- `tests/`: Bộ test suite tự động (Unit test & Integration test).
- `.agents/workflows/`: Các kịch bản tự động hóa (Slash Commands).

---

## ⚡ 3. Khởi động nhanh (Slash Commands)
Dự án được tối ưu hóa với các lệnh tắt (Workflows) để bạn không cần gõ lệnh thủ công:

- **`/setup-project`** (QUAN TRỌNG): Khởi tạo toàn bộ dự án từ A-Z (Build driver, load module, build app/gui/tests, tạo account admin mặc định).
- **`/run-gui`**: Biên dịch và khởi chạy giao diện GUI hiện đại.
- **`/run-tests`**: Chạy toàn bộ 90+ trường hợp kiểm thử tự động.
- **`/check-status`**: Kiểm tra trạng thái Driver và thiết bị hệ thống.
- **`/unload-driver`**: Gỡ bỏ Driver và dọn dẹp tài nguyên Kernel.

---

## 🖥️ 4. Hướng dẫn sử dụng Giao diện GUI
Sau khi chạy lệnh `/run-gui`, ứng dụng sẽ hiện ra với ngôn ngữ thiết kế **Aurora Dark**.

### **Đăng nhập:**
- **Tài khoản:** `admin`
- **Mật khẩu:** `admin`
*(Mật khẩu được băm SHA-256 và so khớp an toàn trong config.txt)*

### **Các tính năng chính:**
1. **Quản lý Sinh viên:** Thêm, Sửa, Xóa, Tìm kiếm sinh viên.
2. **Chuẩn hóa Tên (Kernel Driver):** Mọi tên sinh viên nhập vào sẽ được gửi xuống `/dev/string_norm` để xử lý khoảng trắng và chữ hoa/thường trước khi lưu.
3. **Sắp xếp:** Hỗ trợ sắp xếp theo Tên hoặc GPA ngay trên Sidebar.
4. **USB Export/Import:** Khi cắm USB, bạn có thể chọn đường dẫn mount để lưu file danh sách sinh viên.

---

## 🧪 5. Kịch bản Kiểm thử (Test Cases)

### **Test Case 1: Chuẩn hóa xâu qua Kernel Driver**
- **Thao tác:** Thêm sinh viên mới với tên: `   dAnG   vAn   tAnG   `
- **Kết quả mong đợi:** Tên hiển thị trong bảng là `Dang Van Tang`.
- **Giải thích:** Dữ liệu đã đi qua Device Driver và được xử lý bằng thuật toán trong Kernel Space.

### **Test Case 2: Bảo mật đăng nhập**
- **Thao tác:** Nhập sai mật khẩu 3 lần.
- **Kết quả mong đợi:** Ứng dụng hiển thị cảnh báo đỏ và khóa/ngắt phiên đăng nhập.
- **Kỹ thuật:** So sánh Hash SHA-256 của mật khẩu nhập vào với mã băm trong `config.txt`.

### **Test Case 3: Lưu trữ dữ liệu & USB**
- **Thao tác:** Bấm nút "Lưu File" trên Sidebar, sau đó cắm USB và dùng tính năng Export sang USB.
- **Kết quả mong đợi:** File `students.txt` được tạo ra tại thư mục gốc và thư mục USB với dữ liệu chính xác.

### **Test Case 4: Kiểm thử tự động**
- **Thao tác:** Chạy lệnh `/run-tests`.
- **Kết quả mong đợi:** Toàn bộ 90+ test cases báo `PASSED`, bao gồm cả Mock tests và Integration tests.

---

## 🛡️ 6. Ghi chú bảo mật
Dự án sử dụng cơ chế bảo mật tương tự môi trường Production:
- **Tắt Echo:** Khi nhập mật khẩu ở chế độ CLI, ký tự sẽ không hiện lên.
- **Mã hóa 1 chiều:** Tuyệt đối không lưu mật khẩu dạng Plaintext.
- **Phòng chống Brute-force:** Giới hạn số lần thử đăng nhập.

---
*Phát triển bởi Đạt & Đội ngũ AI Assistant.*
