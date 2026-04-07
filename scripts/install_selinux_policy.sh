#!/bin/bash
# Cài đặt SELinux policy cho usb_bridge kernel module
# Cho phép driver gọi mount/umount qua call_usermodehelper
# Chạy với: sudo bash scripts/install_selinux_policy.sh

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TE_FILE="$SCRIPT_DIR/usb_bridge_selinux.te"

echo "=== SELinux Policy Installer for usb_bridge ==="
echo ""

# Kiểm tra công cụ cần thiết
if ! command -v checkmodule &>/dev/null; then
    echo "[INFO] Cài policycoreutils-devel..."
    dnf install -y policycoreutils-devel selinux-policy-devel 2>/dev/null || \
    yum install -y policycoreutils-devel selinux-policy-devel 2>/dev/null || true
fi

echo "[1/4] Lấy AVC denials thực tế từ audit log..."
# Thử lấy từ audit log thực tế để tạo policy chính xác hơn
REAL_POLICY=$(ausearch -c '(usb_bridge)' -m avc --raw 2>/dev/null | \
              audit2allow 2>/dev/null)
if [ -n "$REAL_POLICY" ]; then
    echo "  Found real AVC denials, using them:"
    echo "$REAL_POLICY"
    ausearch -c '(usb_bridge)' -m avc --raw 2>/dev/null | \
        audit2allow -M usb_bridge_selinux
    echo "[2/4] Policy module compiled from real denials"
else
    echo "  No AVC log access, compiling from .te file..."
    echo "[2/4] Compiling $TE_FILE ..."
    cd "$SCRIPT_DIR"
    checkmodule -M -m -o usb_bridge_selinux.mod usb_bridge_selinux.te
    semodule_package -o usb_bridge_selinux.pp -m usb_bridge_selinux.mod
    echo "  Compiled: usb_bridge_selinux.pp"
fi

echo ""
echo "[3/4] Installing policy module..."
semodule -i usb_bridge_selinux.pp
echo "  Policy installed: usb_bridge_selinux"

echo ""
echo "[4/4] Reloading usb_bridge kernel module..."
rmmod usb_bridge 2>/dev/null || true
insmod /home/dat/project_driver/kernel_module/usb_bridge.ko
echo "  Module reloaded OK"

echo ""
echo "=== DONE ==="
echo ""
echo "Test: thử Mount/Unmount trong GUI rồi kiểm tra dmesg:"
echo "  sudo dmesg | grep usb_bridge | tail -10"
echo ""
echo "Nếu vẫn bị ret=-13, chạy lệnh này để xem AVC mới:"
echo "  sudo ausearch -c usb_bridge -m avc | tail -20"
