#ifndef USB_DRIVER_CLIENT_H
#define USB_DRIVER_CLIENT_H

#include <stddef.h>

#define USB_CLIENT_MAX_PATH     256
#define USB_CLIENT_MAX_CONTENT  65536

int usb_driver_mount(const char *device, const char *mount_point,
                     const char *fs_type, const char *options);
int usb_driver_unmount(const char *mount_point);
int usb_driver_read_text(const char *file_path, char *content, size_t max_len);
int usb_driver_write_text(const char *file_path, const char *content, size_t len);
int usb_driver_copy_to_usb(const char *src_path, const char *dst_path, unsigned int *bytes_copied);
int usb_driver_copy_from_usb(const char *src_path, const char *dst_path, unsigned int *bytes_copied);
int usb_driver_available(void);
int usb_driver_is_managed_mount(const char *mount_point, int *is_managed);

#endif
