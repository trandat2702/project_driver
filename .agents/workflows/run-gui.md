---
description: build và chạy ứng dụng GUI student_manager_gui
---

// turbo-all

Lưu ý: với nghiệp vụ hiện tại (kernel-only normalize), hãy chạy /build-driver trước khi Add/Edit/Import.

1. Build GUI nếu cần
```bash
cd /home/dat/linux-driver-project/userspace_app && make 2>&1
```

2. Chạy GUI (chạy nền)
```bash
cd /home/dat/linux-driver-project/userspace_app && ./student_manager_gui &
```
