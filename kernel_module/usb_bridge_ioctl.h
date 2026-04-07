#ifndef USB_BRIDGE_IOCTL_H
#define USB_BRIDGE_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define USB_BRIDGE_MAGIC 'U'

#define USB_MAX_PATH_LEN      256
#define USB_MAX_CONTENT_LEN   65536
#define USB_MAX_FSTYPE_LEN    32
#define USB_MAX_OPTIONS_LEN   128

struct usb_mount_args {
    char device[USB_MAX_PATH_LEN];
    char mount_point[USB_MAX_PATH_LEN];
    char fs_type[USB_MAX_FSTYPE_LEN];
    char options[USB_MAX_OPTIONS_LEN];
};

struct usb_umount_args {
    char mount_point[USB_MAX_PATH_LEN];
};

struct usb_mount_owner_args {
    char mount_point[USB_MAX_PATH_LEN];
    int managed;
};

struct usb_read_args {
    char file_path[USB_MAX_PATH_LEN];
    char content[USB_MAX_CONTENT_LEN];
    unsigned int content_len;
};

struct usb_write_args {
    char file_path[USB_MAX_PATH_LEN];
    char content[USB_MAX_CONTENT_LEN];
    unsigned int content_len;
};

struct usb_copy_args {
    char src_path[USB_MAX_PATH_LEN];
    char dst_path[USB_MAX_PATH_LEN];
    unsigned int bytes_copied;
};

struct usb_status_args {
    int result;
    char message[256];
};

#define USB_OP_MOUNT            _IOW(USB_BRIDGE_MAGIC, 1, int)
#define USB_OP_UNMOUNT          _IOW(USB_BRIDGE_MAGIC, 2, int)
#define USB_OP_READ_TEXT        _IOW(USB_BRIDGE_MAGIC, 3, int)
#define USB_OP_WRITE_TEXT       _IOW(USB_BRIDGE_MAGIC, 4, int)
#define USB_OP_COPY_HOST_TO_USB _IOW(USB_BRIDGE_MAGIC, 5, int)
#define USB_OP_COPY_USB_TO_HOST _IOW(USB_BRIDGE_MAGIC, 6, int)
#define USB_OP_CHECK_MOUNT_OWNERSHIP _IOWR(USB_BRIDGE_MAGIC, 7, int)

#define USB_BRIDGE_DEVICE_PATH "/dev/usb_bridge"

#endif
