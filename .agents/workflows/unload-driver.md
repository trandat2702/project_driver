---
description: unload toàn bộ kernel modules của dự án và dọn device nodes
---

// turbo-all

1. Unload string_norm module
```bash
sudo rmmod string_norm 2>/dev/null && echo "string_norm unloaded" || echo "string_norm was not loaded"
```

2. Unload usb_driver module
```bash
sudo rmmod usb_driver 2>/dev/null && echo "usb_driver unloaded" || echo "usb_driver was not loaded"
```

3. Dọn device nodes
```bash
sudo rm -f /dev/string_norm && echo "Device node removed" || echo "Device node not found"
```

4. Xác nhận trạng thái
```bash
lsmod | grep -E "string_norm|usb_driver" || echo "All modules unloaded OK"
```
