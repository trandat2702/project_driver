---
description: kiểm tra trạng thái toàn bộ hệ thống - modules có load không, device node, kernel log
---

// turbo-all

1. Kiểm tra kernel modules
```bash
echo "=== Loaded modules ===" && lsmod | grep -E "string_norm|usb_driver" || echo "(none)"
```

2. Kiểm tra device node
```bash
echo "=== Device nodes ===" && ls -la /dev/string_norm 2>/dev/null || echo "/dev/string_norm not found"
```

3. Kiểm tra kernel log gần nhất
```bash
echo "=== Kernel log (last 15 lines) ===" && dmesg | grep -E "(string_norm|USB_DRIVER)" | tail -15
```

4. Kiểm tra kernel và môi trường
```bash
echo "=== System ===" && uname -r && gcc --version | head -1 && openssl version
```
