#ifndef USB_FILE_H
#define USB_FILE_H

#include <stddef.h>

#define MAX_USB_MOUNTS 8
#define MAX_MOUNT_PATH 256

typedef struct {
    char path[MAX_MOUNT_PATH];
    char device[MAX_MOUNT_PATH];
} UsbMount;

int usb_write_text_file(const char *mount_path,
                        const char *file_name,
                        const char *content);
int usb_read_text_file(const char *mount_path,
                       const char *file_name,
                       char *output,
                       size_t output_size);

int usb_mount(const char *device, const char *mount_point,
              const char *fs_type, const char *options);
int usb_mount_detect(const char *device, const char *mount_point,
                     const char *fs_type, const char *options,
                     char *actual_mp_out, size_t actual_mp_sz);
int usb_unmount(const char *mount_point);
int usb_unmount_system(const char *mount_point);
int usb_is_driver_managed_mount(const char *mount_point, int *is_managed);

int usb_copy_to_device(const char *src_path, const char *dst_path, unsigned int *bytes_copied);
int usb_copy_from_device(const char *src_path, const char *dst_path, unsigned int *bytes_copied);

int detect_usb_mounts(UsbMount *mounts, int max_count);
int detect_usb_devices(UsbMount *devices, int max_count);

#endif
