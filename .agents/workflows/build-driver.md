---
description: build và load kernel module string_norm
---

// turbo-all

1. Build kernel module string_norm
```bash
cd /home/dat/linux-driver-project/kernel_module && make 2>&1
```

2. Unload module cũ nếu đang chạy
```bash
sudo rmmod string_norm 2>/dev/null; sudo rm -f /dev/string_norm; echo "Old module cleared"
```

3. Load module mới và tạo device node
```bash
sudo insmod /home/dat/linux-driver-project/kernel_module/string_norm.ko && \
MAJOR=$(awk '/string_norm/{print $1}' /proc/devices) && \

sudo chmod 666 /dev/string_norm && \
echo "=== Loaded OK: /dev/string_norm (major=$MAJOR) ==="
```

4. Kiểm tra kết quả
```bash
lsmod | grep string_norm && dmesg | grep string_norm | tail -3
```
