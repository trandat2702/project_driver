---
description: khởi tạo toàn bộ dự án từ đầu - build driver, app, GUI, load module, tạo config
---

// turbo-all

1. Kiểm tra dependencies cần thiết
```bash
echo "=== Checking dependencies ===" && \
gcc --version | head -1 && \
pkg-config --modversion gtk+-3.0 && \
openssl version && \
uname -r && \
echo "=== All OK ==="
```

2. Tạo file config.txt nếu chưa có (tài khoản mặc định: admin / admin123)
```bash
cd /home/dat/linux-driver-project/userspace_app && \
if [ ! -f config.txt ]; then \
  HASH=$(echo -n "admin123" | sha256sum | awk '{print $1}'); \
  echo "admin:$HASH" > config.txt; \
  echo "Created config.txt with default account admin/admin123"; \
else \
  echo "config.txt already exists"; \
fi
```

3. Build kernel module string_norm
```bash
cd /home/dat/linux-driver-project/kernel_module && make clean && make 2>&1
```

4. Unload module cũ nếu đang chạy
```bash
sudo rmmod string_norm 2>/dev/null; sudo rm -f /dev/string_norm; echo "Old module cleared"
```

5. Load module mới và tạo device node
```bash
sudo insmod /home/dat/linux-driver-project/kernel_module/string_norm.ko && \
MAJOR=$(awk '/string_norm/{print $1}' /proc/devices) && \
if [ ! -e /dev/string_norm ]; then sudo mknod /dev/string_norm c "$MAJOR" 0; fi && \
sudo chmod 666 /dev/string_norm && \
echo "=== Driver loaded: /dev/string_norm (major=$MAJOR) ==="
```

6. Build userspace GUI app
```bash
cd /home/dat/linux-driver-project/userspace_app && make clean && make 2>&1
```

7. Build test suite
```bash
cd /home/dat/linux-driver-project/tests && make clean && make all 2>&1
```

8. Chạy unit tests nhanh để xác nhận
```bash
cd /home/dat/linux-driver-project/tests && \
./userspace_tests/test_normalize_mock && \
./userspace_tests/test_auth && \
./userspace_tests/test_student && \
echo "=== All unit tests passed ==="
```

9. Kiểm tra trạng thái tổng thể
```bash
echo "=== Final status ===" && \
echo "Driver:" && lsmod | grep string_norm && \
echo "Device:" && ls -la /dev/string_norm && \
echo "GUI app:" && ls -lh /home/dat/linux-driver-project/userspace_app/student_manager_gui && \
echo "=== PROJECT READY ==="
```
