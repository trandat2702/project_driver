#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "usb_file.h"
#include "usb_driver_client.h"

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

static int write_via_libc(const char *full_path, const char *content) {
    FILE *fp = fopen(full_path, "w");
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

static int read_via_libc(const char *full_path, char *output, size_t output_size) {
    FILE *fp;
    size_t n;

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

static int run_command_wait(char *const argv[]) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0)
        return -errno;

    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0)
        return -errno;

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 0;

    return -EIO;
}

static int find_device_for_mount_point(const char *mount_point,
                                       char *device_out,
                                       size_t device_out_size) {
    FILE *fp;
    char line[1024];

    if (!mount_point || !device_out || device_out_size == 0)
        return -EINVAL;

    fp = fopen("/proc/mounts", "r");
    if (!fp)
        return -errno;

    while (fgets(line, sizeof(line), fp)) {
        char device[MAX_MOUNT_PATH];
        char path[MAX_MOUNT_PATH];

        if (sscanf(line, "%255s %255s", device, path) < 2)
            continue;

        if (strcmp(path, mount_point) == 0) {
            strncpy(device_out, device, device_out_size - 1);
            device_out[device_out_size - 1] = '\0';
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -ENOENT;
}

int usb_write_text_file(const char *mount_path,
                        const char *file_name,
                        const char *content) {
    char full_path[PATH_MAX];
    int ret;

    if (!content)
        return -1;

    if (validate_mount_path(mount_path) != 0)
        return -1;

    if (build_usb_file_path(mount_path, file_name,
                            full_path, sizeof(full_path)) != 0)
        return -1;

    if (usb_driver_available()) {
        ret = usb_driver_write_text(full_path, content, strlen(content));
        if (ret >= 0)
            return 0;
        fprintf(stderr, "Driver write failed, trying libc fallback...\n");
    }

    return write_via_libc(full_path, content);
}

int usb_read_text_file(const char *mount_path,
                       const char *file_name,
                       char *output,
                       size_t output_size) {
    char full_path[PATH_MAX];
    int ret;

    if (!output || output_size == 0)
        return -1;

    output[0] = '\0';

    if (validate_mount_path(mount_path) != 0)
        return -1;

    if (build_usb_file_path(mount_path, file_name,
                            full_path, sizeof(full_path)) != 0)
        return -1;

    if (usb_driver_available()) {
        ret = usb_driver_read_text(full_path, output, output_size);
        if (ret >= 0)
            return 0;
        fprintf(stderr, "Driver read failed, trying libc fallback...\n");
    }

    return read_via_libc(full_path, output, output_size);
}

static int udisks_mount_fallback(const char *device,
                                 char *actual_mp_out,
                                 size_t actual_mp_sz) {
    char cmd[512];
    char out[512] = {0};
    FILE *fp;
    int status;

    snprintf(cmd, sizeof(cmd),
             "/usr/bin/udisksctl mount -b %.400s 2>&1", device);
    fp = popen(cmd, "r");
    if (!fp)
        return -errno;

    if (fgets(out, sizeof(out), fp) == NULL)
        out[0] = '\0';

    status = pclose(fp);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "udisksctl mount failed: %s\n", out);
        return -EIO;
    }

    if (actual_mp_out && actual_mp_sz > 0) {
        const char *at = strstr(out, " at ");
        if (at) {
            strncpy(actual_mp_out, at + 4, actual_mp_sz - 1);
            actual_mp_out[actual_mp_sz - 1] = '\0';
            while (actual_mp_out[0] != '\0') {
                size_t len = strlen(actual_mp_out);
                char last = actual_mp_out[len - 1];
                if (last != '.' && last != '\n' && last != '\r')
                    break;
                actual_mp_out[len - 1] = '\0';
            }
        }
    }

    fprintf(stderr, "udisksctl: %s\n", out);
    return 0;
}

static int udisks_unmount_fallback(const char *mount_point) {
    char device[MAX_MOUNT_PATH] = {0};
    char *const argv_device[] = {
        "/usr/bin/udisksctl", "unmount", "-b", device, NULL
    };
    char *const argv_path[] = {
        "/usr/bin/udisksctl", "unmount", "-b", (char *)mount_point, NULL
    };

    if (find_device_for_mount_point(mount_point, device, sizeof(device)) == 0)
        return run_command_wait(argv_device);

    return run_command_wait(argv_path);
}

int usb_mount(const char *device, const char *mount_point,
              const char *fs_type, const char *options) {
    if (usb_driver_available()) {
        int ret = usb_driver_mount(device, mount_point, fs_type, options);
        if (ret == 0) {
            fprintf(stderr, "Mount via driver OK: %s at %s\n",
                    device, mount_point);
            return 0;
        }
        fprintf(stderr, "Driver mount failed (%d), trying udisksctl...\n", ret);
    } else {
        fprintf(stderr, "Driver not available, trying udisksctl...\n");
    }

    return udisks_mount_fallback(device, NULL, 0);
}

int usb_mount_detect(const char *device, const char *mount_point,
                     const char *fs_type, const char *options,
                     char *actual_mp_out, size_t actual_mp_sz) {
    if (usb_driver_available()) {
        int ret = usb_driver_mount(device, mount_point, fs_type, options);
        if (ret == 0) {
            if (actual_mp_out && actual_mp_sz > 0) {
                strncpy(actual_mp_out, mount_point, actual_mp_sz - 1);
                actual_mp_out[actual_mp_sz - 1] = '\0';
            }
            fprintf(stderr, "Mount via driver OK: %s at %s\n",
                    device, mount_point);
            return 0;
        }
        fprintf(stderr, "Driver mount failed (%d), trying udisksctl...\n", ret);
    } else {
        fprintf(stderr, "Driver not available, trying udisksctl...\n");
    }

    return udisks_mount_fallback(device, actual_mp_out, actual_mp_sz);
}

int usb_unmount(const char *mount_point) {
    if (!mount_point || mount_point[0] == '\0')
        return -EINVAL;

    if (usb_driver_available()) {
        int ret = usb_driver_unmount(mount_point);
        if (ret == 0) {
            fprintf(stderr, "Unmount via driver OK: %s\n", mount_point);
            return 0;
        }
        fprintf(stderr, "Driver unmount failed (%d), trying udisksctl...\n", ret);
    } else {
        fprintf(stderr, "Driver not available, trying udisksctl...\n");
    }

    if (udisks_unmount_fallback(mount_point) == 0) {
        fprintf(stderr, "Unmount via udisksctl OK: %s\n", mount_point);
        return 0;
    }

    return usb_unmount_system(mount_point);
}

int usb_unmount_system(const char *mount_point) {
    int ret;
    int saved_errno;
    char device[MAX_MOUNT_PATH];

    if (!mount_point || mount_point[0] == '\0')
        return -EINVAL;

    ret = umount2(mount_point, 0);
    if (ret == 0)
        return 0;

    saved_errno = errno;
    if (saved_errno != EPERM && saved_errno != EACCES)
        return -saved_errno;

    ret = find_device_for_mount_point(mount_point, device, sizeof(device));
    if (ret < 0)
        return ret;

    {
        char *const argv[] = { "/usr/bin/udisksctl", "unmount", "-b", device, NULL };
        ret = run_command_wait(argv);
    }

    if (ret == 0)
        return 0;

    return -saved_errno;
}

int usb_is_driver_managed_mount(const char *mount_point, int *is_managed) {
    if (!mount_point || !is_managed)
        return -EINVAL;

    if (!usb_driver_available()) {
        *is_managed = 0;
        return -1;
    }

    return usb_driver_is_managed_mount(mount_point, is_managed);
}

int usb_copy_to_device(const char *src_path, const char *dst_path, unsigned int *bytes_copied) {
    if (!usb_driver_available()) {
        fprintf(stderr, "ERROR: USB driver not available for copy\n");
        return -1;
    }
    return usb_driver_copy_to_usb(src_path, dst_path, bytes_copied);
}

int usb_copy_from_device(const char *src_path, const char *dst_path, unsigned int *bytes_copied) {
    if (!usb_driver_available()) {
        fprintf(stderr, "ERROR: USB driver not available for copy\n");
        return -1;
    }
    return usb_driver_copy_from_usb(src_path, dst_path, bytes_copied);
}

int detect_usb_mounts(UsbMount *mounts, int max_count) {
    FILE *fp;
    char line[1024];
    int found = 0;

    if (!mounts || max_count <= 0) return 0;

    fp = fopen("/proc/mounts", "r");
    if (!fp) return 0;

    while (fgets(line, sizeof(line), fp) && found < max_count) {
        char device[MAX_MOUNT_PATH];
        char path[MAX_MOUNT_PATH];
        char fstype[64];

        if (sscanf(line, "%255s %255s %63s", device, path, fstype) < 3)
            continue;

        int is_usb_device = (strncmp(device, "/dev/sd", 7) == 0);
        int is_media_path = (strncmp(path, "/media/", 7) == 0 ||
                             strncmp(path, "/run/media/", 11) == 0);
        int is_real_fs = (strcmp(fstype, "vfat") == 0 ||
                          strcmp(fstype, "exfat") == 0 ||
                          strcmp(fstype, "ntfs") == 0 ||
                          strcmp(fstype, "ntfs3") == 0 ||
                          strcmp(fstype, "fuseblk") == 0 ||
                          strcmp(fstype, "ext4") == 0 ||
                          strcmp(fstype, "ext3") == 0 ||
                          strcmp(fstype, "ext2") == 0);

        if ((is_usb_device || is_media_path) && is_real_fs) {
            strncpy(mounts[found].device, device, MAX_MOUNT_PATH - 1);
            mounts[found].device[MAX_MOUNT_PATH - 1] = '\0';
            strncpy(mounts[found].path, path, MAX_MOUNT_PATH - 1);
            mounts[found].path[MAX_MOUNT_PATH - 1] = '\0';
            found++;
        }
    }

    fclose(fp);
    return found;
}

int detect_usb_devices(UsbMount *devices, int max_count) {
    FILE *fp;
    char line[1024];
    int found = 0;
    char cmd[] = "lsblk -rno NAME,SIZE,TYPE,MOUNTPOINT 2>/dev/null";

    if (!devices || max_count <= 0) return 0;

    fp = popen(cmd, "r");
    if (!fp) return 0;

    while (fgets(line, sizeof(line), fp) && found < max_count) {
        char name[64], size[32], type[32], mountpoint[256];
        int n = sscanf(line, "%63s %31s %31s %255s", name, size, type, mountpoint);
        if (n < 3) continue;
        if (n < 4) mountpoint[0] = '\0';

        if (strcmp(type, "part") == 0 && strncmp(name, "sd", 2) == 0) {
            snprintf(devices[found].device, MAX_MOUNT_PATH, "/dev/%s", name);
            if (mountpoint[0])
                strncpy(devices[found].path, mountpoint, MAX_MOUNT_PATH - 1);
            else
                devices[found].path[0] = '\0';
            devices[found].path[MAX_MOUNT_PATH - 1] = '\0';
            found++;
        }
    }

    pclose(fp);
    return found;
}
