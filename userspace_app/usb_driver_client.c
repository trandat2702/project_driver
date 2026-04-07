#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include "usb_driver_client.h"
#include "usb_bridge_ioctl.h"

static int open_driver(void) {
    int fd = open(USB_BRIDGE_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", USB_BRIDGE_DEVICE_PATH, strerror(errno));
    }
    return fd;
}

int usb_driver_available(void) {
    int fd = open(USB_BRIDGE_DEVICE_PATH, O_RDWR);
    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

int usb_driver_mount(const char *device, const char *mount_point,
                     const char *fs_type, const char *options) {
    struct usb_mount_args args;
    int fd, ret;

    if (!device || !mount_point)
        return -EINVAL;

    memset(&args, 0, sizeof(args));
    strncpy(args.device, device, USB_MAX_PATH_LEN - 1);
    strncpy(args.mount_point, mount_point, USB_MAX_PATH_LEN - 1);
    if (fs_type)
        strncpy(args.fs_type, fs_type, USB_MAX_FSTYPE_LEN - 1);
    if (options)
        strncpy(args.options, options, USB_MAX_OPTIONS_LEN - 1);

    fd = open_driver();
    if (fd < 0)
        return -errno;

    ret = ioctl(fd, USB_OP_MOUNT, &args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_MOUNT failed: %s\n", strerror(errno));
    }

    close(fd);
    return ret;
}

int usb_driver_unmount(const char *mount_point) {
    struct usb_umount_args args;
    int fd, ret;

    if (!mount_point)
        return -EINVAL;

    memset(&args, 0, sizeof(args));
    strncpy(args.mount_point, mount_point, USB_MAX_PATH_LEN - 1);

    fd = open_driver();
    if (fd < 0)
        return -errno;

    ret = ioctl(fd, USB_OP_UNMOUNT, &args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_UNMOUNT failed: %s\n", strerror(errno));
    }

    close(fd);
    return ret;
}

int usb_driver_read_text(const char *file_path, char *content, size_t max_len) {
    struct usb_read_args *args;
    int fd, ret;
    size_t copy_len;

    if (!file_path || !content || max_len == 0)
        return -EINVAL;

    args = calloc(1, sizeof(*args));
    if (!args)
        return -ENOMEM;

    strncpy(args->file_path, file_path, USB_MAX_PATH_LEN - 1);
    args->content_len = 0;

    fd = open_driver();
    if (fd < 0) {
        free(args);
        return -errno;
    }

    ret = ioctl(fd, USB_OP_READ_TEXT, args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_READ_TEXT failed: %s\n", strerror(errno));
    } else {
        copy_len = args->content_len;
        if (copy_len >= max_len)
            copy_len = max_len - 1;
        memcpy(content, args->content, copy_len);
        content[copy_len] = '\0';
        ret = (int)copy_len;
    }

    close(fd);
    free(args);
    return ret;
}

int usb_driver_write_text(const char *file_path, const char *content, size_t len) {
    struct usb_write_args *args;
    int fd, ret;

    if (!file_path || !content)
        return -EINVAL;

    if (len > USB_MAX_CONTENT_LEN)
        len = USB_MAX_CONTENT_LEN;

    args = calloc(1, sizeof(*args));
    if (!args)
        return -ENOMEM;

    strncpy(args->file_path, file_path, USB_MAX_PATH_LEN - 1);
    memcpy(args->content, content, len);
    args->content_len = len;

    fd = open_driver();
    if (fd < 0) {
        free(args);
        return -errno;
    }

    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_WRITE_TEXT failed: %s\n", strerror(errno));
    }

    close(fd);
    free(args);
    return ret;
}

int usb_driver_copy_to_usb(const char *src_path, const char *dst_path, unsigned int *bytes_copied) {
    struct usb_copy_args args;
    int fd, ret;

    if (!src_path || !dst_path)
        return -EINVAL;

    memset(&args, 0, sizeof(args));
    strncpy(args.src_path, src_path, USB_MAX_PATH_LEN - 1);
    strncpy(args.dst_path, dst_path, USB_MAX_PATH_LEN - 1);

    fd = open_driver();
    if (fd < 0)
        return -errno;

    ret = ioctl(fd, USB_OP_COPY_HOST_TO_USB, &args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_COPY_HOST_TO_USB failed: %s\n", strerror(errno));
    } else if (bytes_copied) {
        *bytes_copied = args.bytes_copied;
    }

    close(fd);
    return ret;
}

int usb_driver_copy_from_usb(const char *src_path, const char *dst_path, unsigned int *bytes_copied) {
    struct usb_copy_args args;
    int fd, ret;

    if (!src_path || !dst_path)
        return -EINVAL;

    memset(&args, 0, sizeof(args));
    strncpy(args.src_path, src_path, USB_MAX_PATH_LEN - 1);
    strncpy(args.dst_path, dst_path, USB_MAX_PATH_LEN - 1);

    fd = open_driver();
    if (fd < 0)
        return -errno;

    ret = ioctl(fd, USB_OP_COPY_USB_TO_HOST, &args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_COPY_USB_TO_HOST failed: %s\n", strerror(errno));
    } else if (bytes_copied) {
        *bytes_copied = args.bytes_copied;
    }

    close(fd);
    return ret;
}

int usb_driver_is_managed_mount(const char *mount_point, int *is_managed) {
    struct usb_mount_owner_args args;
    int fd, ret;

    if (!mount_point || !is_managed)
        return -EINVAL;

    memset(&args, 0, sizeof(args));
    strncpy(args.mount_point, mount_point, USB_MAX_PATH_LEN - 1);
    args.managed = 0;

    fd = open_driver();
    if (fd < 0)
        return -errno;

    ret = ioctl(fd, USB_OP_CHECK_MOUNT_OWNERSHIP, &args);
    if (ret < 0) {
        ret = -errno;
        fprintf(stderr, "USB_OP_CHECK_MOUNT_OWNERSHIP failed: %s\n", strerror(errno));
    } else {
        *is_managed = args.managed ? 1 : 0;
        ret = 0;
    }

    close(fd);
    return ret;
}
