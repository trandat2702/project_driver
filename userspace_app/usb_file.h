#ifndef USB_FILE_H
#define USB_FILE_H

#include <stddef.h>

int usb_write_text_file(const char *mount_path,
                        const char *file_name,
                        const char *content);
int usb_read_text_file(const char *mount_path,
                       const char *file_name,
                       char *output,
                       size_t output_size);

#endif
