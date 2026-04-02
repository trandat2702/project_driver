#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include "usb_file.h"

static int build_usb_file_path(const char *mount_path,
                               const char *file_name,
                               char *full_path,
                               size_t full_path_size) {
    if (!mount_path || !file_name || !full_path || full_path_size == 0)
        return -1;

    if (mount_path[0] == '\0' || file_name[0] == '\0')
        return -1;

    if (strchr(file_name, '/')) {
        fprintf(stderr, "ERROR: file name must not contain '/'.\n");
        return -1;
    }

    if (snprintf(full_path, full_path_size, "%s/%s", mount_path, file_name)
        >= (int)full_path_size) {
        fprintf(stderr, "ERROR: USB file path is too long.\n");
        return -1;
    }

    return 0;
}

static int validate_mount_path(const char *mount_path) {
    struct stat st;

    if (stat(mount_path, &st) != 0) {
        fprintf(stderr, "ERROR: cannot access mount path '%s': %s\n",
                mount_path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: '%s' is not a directory.\n", mount_path);
        return -1;
    }

    return 0;
}

int usb_write_text_file(const char *mount_path,
                        const char *file_name,
                        const char *content) {
    char full_path[PATH_MAX];
    FILE *fp;

    if (!content)
        return -1;

    if (validate_mount_path(mount_path) != 0)
        return -1;

    if (build_usb_file_path(mount_path, file_name,
                            full_path, sizeof(full_path)) != 0)
        return -1;

    fp = fopen(full_path, "w");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open '%s' for write: %s\n",
                full_path, strerror(errno));
        return -1;
    }

    if (fputs(content, fp) == EOF) {
        fprintf(stderr, "ERROR: write failed for '%s'.\n", full_path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int usb_read_text_file(const char *mount_path,
                       const char *file_name,
                       char *output,
                       size_t output_size) {
    char full_path[PATH_MAX];
    FILE *fp;
    size_t n;

    if (!output || output_size == 0)
        return -1;

    output[0] = '\0';

    if (validate_mount_path(mount_path) != 0)
        return -1;

    if (build_usb_file_path(mount_path, file_name,
                            full_path, sizeof(full_path)) != 0)
        return -1;

    fp = fopen(full_path, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open '%s' for read: %s\n",
                full_path, strerror(errno));
        return -1;
    }

    n = fread(output, 1, output_size - 1, fp);
    output[n] = '\0';

    if (ferror(fp)) {
        fprintf(stderr, "ERROR: read failed for '%s'.\n", full_path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}
