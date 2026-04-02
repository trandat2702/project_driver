---
description: chạy toàn bộ test suite của dự án (unit + integration)
---

// turbo-all

1. Build tất cả test binaries
```bash
cd /home/dat/linux-driver-project/tests && make all 2>&1
```

2. Chạy mock normalize test (không cần driver)
```bash
cd /home/dat/linux-driver-project/tests && ./userspace_tests/test_normalize_mock
```

3. Chạy SHA-256 auth test
```bash
cd /home/dat/linux-driver-project/tests && ./userspace_tests/test_auth
```

4. Chạy student CRUD test
```bash
cd /home/dat/linux-driver-project/tests && ./userspace_tests/test_student
```

5. Chạy integration test (cần driver đã load)
```bash
sudo /home/dat/linux-driver-project/tests/integration_tests/test_driver_io
```
