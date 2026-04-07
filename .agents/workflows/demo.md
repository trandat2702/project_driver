---
description: Quy trình hoàn chỉnh - Build, Load, Test, Run GUI, Cleanup
---

# DỰ ÁN QUẢN LÝ SINH VIÊN VỚI LINUX DRIVER

## 0. CẤU HÌNH UDEV (Chỉ cần làm 1 lần)

Tạo udev rule để tự động cấp quyền cho user `dat`:

```bash
sudo tee /etc/udev/rules.d/99-student-drivers.rules << 'EOF'
KERNEL=="string_norm", MODE="0666", OWNER="dat", GROUP="dat"
KERNEL=="usb_bridge", MODE="0666", OWNER="dat", GROUP="dat"
EOF

sudo udevadm control --reload-rules
```

## 1. BUILD TẤT CẢ

```bash
cd /home/dat/project_driver/kernel_module && make clean && make && \
cd /home/dat/project_driver/userspace_app && make clean && make && \
cd /home/dat/project_driver/tests && make clean && make all && \
echo "=== BUILD DONE ==="
```

## 2. LOAD DRIVERS

```bash
cd /home/dat/project_driver/kernel_module && \
sudo rmmod usb_bridge string_norm 2>/dev/null; \
sudo rm -f /dev/string_norm /dev/usb_bridge; \
sudo insmod string_norm.ko && sudo insmod usb_bridge.ko && \
M1=$(awk '/string_norm/{print $1}' /proc/devices) && \
M2=$(awk '/usb_bridge/{print $1}' /proc/devices) && \
sudo mknod /dev/string_norm c $M1 0 && sudo mknod /dev/usb_bridge c $M2 0 && \
echo "=== DRIVERS: string_norm=$M1, usb_bridge=$M2 ==="
```

### Kiểm tra drivers đã load:
```bash
lsmod | grep -E "string_norm|usb_bridge"
ls -la /dev/string_norm /dev/usb_bridge
```

### Kiểm tra USB đã mount:
```bash
lsblk
```

## 3. CHẠY TESTS

```bash
cd /home/dat/project_driver/tests && \
./userspace_tests/test_normalize_mock && \
./userspace_tests/test_auth && \
./userspace_tests/test_student && \
./integration_tests/test_driver_io && \
./integration_tests/test_usb_bridge_io && \
echo "=== ALL TESTS PASSED ==="
```

## 4. CHẠY GUI

```bash
cd /home/dat/project_driver/userspace_app && ./student_manager_gui
```

**Đăng nhập:** `admin` / `123456`

**Chức năng chính:**
- Thêm/Sửa/Xóa sinh viên (tên tự động chuẩn hóa qua driver)
- Lưu/Tải danh sách từ file
- Xuất Text (.txt) / Excel (.csv)

**USB Storage Manager** (click nút "💾 USB Storage Manager" trong sidebar):
- Detect USB: Tìm USB đã mount trên hệ thống
- Read/Write: Đọc/ghi file text trên USB
- Copy To/From USB: Sao chép file txt/csv
- Log Messages: Theo dõi mọi thao tác

## 5. DỌN DẸP

```bash
sudo rmmod usb_bridge string_norm 2>/dev/null; \
sudo rm -f /dev/string_norm /dev/usb_bridge; \
cd /home/dat/project_driver && \
make -C kernel_module clean 2>/dev/null; \
make -C userspace_app clean 2>/dev/null; \
make -C tests clean 2>/dev/null; \
echo "=== CLEANED ==="
```
