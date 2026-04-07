---
description: Quy trình hoàn chỉnh - Build, Load, Test, Run GUI, Cleanup
---

# DỰ ÁN QUẢN LÝ SINH VIÊN VỚI LINUX DRIVER

## 0. SETUP PERMISSIONS (Chỉ cần làm 1 lần)

Chạy script tự động cấu hình toàn bộ:

```bash
sudo bash /home/dat/project_driver/scripts/setup_usb_permissions.sh
```

Script này thực hiện:
- **udev rule**: tự động cấp quyền `dat:dat` cho `/dev/string_norm` và `/dev/usb_bridge`
- **polkit rule**: cho phép user `dat` mount/unmount USB không cần nhập mật khẩu
- **SELinux policy**: cho phép kernel module gọi usermode helper (mount)
- **Reload module**: nạp lại `usb_bridge.ko` mới nhất

Hoặc cấu hình thủ công từng bước:

```bash
# udev rule
sudo tee /etc/udev/rules.d/99-student-drivers.rules << 'EOF'
KERNEL=="string_norm", MODE="0666", OWNER="dat", GROUP="dat"
KERNEL=="usb_bridge", MODE="0666", OWNER="dat", GROUP="dat"
EOF
sudo udevadm control --reload-rules

# polkit rule (mount không cần password)
sudo tee /etc/polkit-1/rules.d/50-udisks2-dat.rules << 'EOF'
polkit.addRule(function(action, subject) {
    var allowed = [
        "org.freedesktop.udisks2.filesystem-mount",
        "org.freedesktop.udisks2.filesystem-mount-system",
        "org.freedesktop.udisks2.filesystem-unmount-others",
        "org.freedesktop.udisks2.filesystem-unmount",
    ];
    for (var i = 0; i < allowed.length; i++) {
        if (action.id == allowed[i] && subject.user == "dat") {
            return polkit.Result.YES;
        }
    }
});
EOF
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

### Xem kernel log theo dõi driver:
```bash
sudo dmesg -w | grep -E "usb_bridge|string_norm"
```

### Kiểm tra USB đã cắm:
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
- Thêm/Sửa/Xóa sinh viên (tên tự động chuẩn hóa qua driver `string_norm`)
- Lưu/Tải danh sách từ file
- Xuất Text (.txt) / Excel (.csv)

**USB Storage Manager** (click nút "💾 USB Storage Manager" trong sidebar):
- **Mount/Unmount**: Driver `usb_bridge` gửi ioctl → nếu SELinux chặn → fallback `udisksctl` (không cần password sau khi setup polkit)
- **Detect USB**: Tìm USB đã mount trên hệ thống
- **Read/Write**: Đọc/ghi file `.txt`/`.csv` qua driver (kernel VFS)
- **Copy To/From USB**: Sao chép file qua driver kernel
- **Log Messages**: Theo dõi mọi thao tác

### Theo dõi driver trong dmesg khi dùng USB:

```
usb_bridge: mount request /dev/sdb2 -> /tmp/usb_bridge_sdb2   ← driver được gọi
usb_bridge: mount failed, ret=-13                              ← SELinux block
(fallback udisksctl không cần password nhờ polkit rule)
usb_bridge: read_text OK /run/media/dat/USB/file.txt (512 bytes)  ← read qua driver
usb_bridge: write_text OK /run/media/dat/USB/file.txt (256 bytes) ← write qua driver
usb_bridge: copy OK /home/dat/a.csv -> /run/media/dat/USB/a.csv  ← copy qua driver
```

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
