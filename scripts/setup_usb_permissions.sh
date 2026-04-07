#!/bin/bash
# Fix USB mount/unmount permissions for dat user
# Run with: sudo bash scripts/setup_usb_permissions.sh

set -e

echo "=== [1/4] Reload usb_bridge kernel module with new printk logs ==="
rmmod usb_bridge 2>/dev/null || true
insmod /home/dat/project_driver/kernel_module/usb_bridge.ko
echo "    usb_bridge loaded OK"

echo ""
echo "=== [2/4] Apply udev rules for device file permissions ==="
cat > /etc/udev/rules.d/99-student-drivers.rules << 'EOF'
KERNEL=="string_norm", SUBSYSTEM=="string_norm_class", MODE="0666", OWNER="dat", GROUP="dat"
KERNEL=="usb_bridge",  SUBSYSTEM=="usb_bridge_class",  MODE="0666", OWNER="dat", GROUP="dat"
EOF
udevadm control --reload-rules
udevadm trigger
echo "    udev rules applied"

echo ""
echo "=== [3/4] Create polkit rule: dat user can mount/unmount without password ==="
cat > /etc/polkit-1/rules.d/50-udisks2-dat.rules << 'EOF'
polkit.addRule(function(action, subject) {
    var mount_actions = [
        "org.freedesktop.udisks2.filesystem-mount",
        "org.freedesktop.udisks2.filesystem-mount-system",
        "org.freedesktop.udisks2.filesystem-unmount-others",
        "org.freedesktop.udisks2.filesystem-unmount",
    ];
    for (var i = 0; i < mount_actions.length; i++) {
        if (action.id == mount_actions[i] && subject.user == "dat") {
            return polkit.Result.YES;
        }
    }
});
EOF
echo "    polkit rule created: /etc/polkit-1/rules.d/50-udisks2-dat.rules"

echo ""
echo "=== [4/4] Generate SELinux policy to allow usb_bridge to use usermodehelper ==="
if command -v ausearch &>/dev/null && command -v audit2allow &>/dev/null; then
    # Grab recent AVC denials for usb_bridge
    ausearch -c '(usb_bridge)' -m avc --raw 2>/dev/null | \
        audit2allow -M usb_bridge_mount 2>/dev/null && \
        semodule -i usb_bridge_mount.pp 2>/dev/null && \
        echo "    SELinux policy module installed: usb_bridge_mount" || \
        echo "    (no new AVC denials found or audit2allow not available — skipping)"
else
    echo "    audit2allow not found — writing policy manually..."
    cat > /tmp/usb_bridge_mount.te << 'EOF'
module usb_bridge_mount 1.0;

require {
    type kernel_t;
    type mount_exec_t;
    class file { execute execute_no_trans open read };
}

allow kernel_t mount_exec_t:file { execute execute_no_trans open read };
EOF
    checkmodule -M -m -o /tmp/usb_bridge_mount.mod /tmp/usb_bridge_mount.te 2>/dev/null && \
        semodule_package -o /tmp/usb_bridge_mount.pp -m /tmp/usb_bridge_mount.mod 2>/dev/null && \
        semodule -i /tmp/usb_bridge_mount.pp 2>/dev/null && \
        echo "    SELinux policy module installed manually" || \
        echo "    SELinux policy install skipped (may need selinux-policy-devel)"
fi

echo ""
echo "=== Done! ==="
echo ""
echo "Verify:"
echo "  dmesg | grep usb_bridge          # see driver logs"
echo "  ls -la /dev/usb_bridge           # should be owned by dat"
echo "  cat /etc/polkit-1/rules.d/50-udisks2-dat.rules"
