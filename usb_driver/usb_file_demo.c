#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define TEST_FILENAME "driver_test_file.txt"
#define MAX_PATH 512

static void print_banner(const char *mount_path)
{
    printf("==============================================\n");
    printf(" USB File Read/Write Demo\n");
    printf(" Mount path: %s\n", mount_path);
    printf("==============================================\n\n");
}

static int check_usb_mounted(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        fprintf(stderr, "ERROR: path '%s' does not exist.\n", path);
        fprintf(stderr, "Hint: check with lsblk or df -h\n");
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: '%s' is not a directory.\n", path);
        return 0;
    }

    return 1;
}

static int write_test_file(const char *mount_path)
{
    char filepath[MAX_PATH];
    FILE *fp;
    time_t now;

    snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, TEST_FILENAME);
    printf("[WRITE] %s\n", filepath);

    fp = fopen(filepath, "w");
    if (!fp) {
        perror("ERROR: fopen write");
        return -1;
    }

    now = time(NULL);
    fprintf(fp, "=== Linux Driver Project USB Demo ===\n");
    fprintf(fp, "Write time: %s", ctime(&now));
    fprintf(fp, "1|Nguyen Van An|8.50\n");
    fprintf(fp, "2|Tran Thi Binh|9.00\n");
    fprintf(fp, "3|Le Van C|7.00\n");
    fprintf(fp, "=== END ===\n");

    fclose(fp);
    return 0;
}

static int read_test_file(const char *mount_path)
{
    char filepath[MAX_PATH];
    char line[256];
    FILE *fp;

    snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, TEST_FILENAME);
    printf("[READ] %s\n", filepath);

    fp = fopen(filepath, "r");
    if (!fp) {
        perror("ERROR: fopen read");
        return -1;
    }

    while (fgets(line, sizeof(line), fp))
        printf("%s", line);

    fclose(fp);
    printf("\n");
    return 0;
}

static int list_usb_files(const char *mount_path)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    printf("[LIST] %s\n", mount_path);
    dir = opendir(mount_path);
    if (!dir) {
        perror("ERROR: opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char fullpath[MAX_PATH];
        struct stat st;

        if (entry->d_name[0] == '.')
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", mount_path, entry->d_name);
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode))
                printf("[DIR ] %s\n", entry->d_name);
            else
                printf("[FILE] %s (%ld bytes)\n", entry->d_name, (long)st.st_size);
            count++;
        }
    }

    closedir(dir);
    printf("Total entries: %d\n\n", count);
    return 0;
}

static int verify_file(const char *mount_path)
{
    char filepath[MAX_PATH];
    char line[256];
    FILE *fp;
    int found_header = 0;
    int found_data = 0;
    int found_footer = 0;

    snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, TEST_FILENAME);
    fp = fopen(filepath, "r");
    if (!fp) {
        printf("[FAIL] Cannot open file for verify\n");
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "USB Demo"))
            found_header = 1;
        if (strstr(line, "Nguyen Van An"))
            found_data = 1;
        if (strstr(line, "END"))
            found_footer = 1;
    }

    fclose(fp);

    if (found_header && found_data && found_footer) {
        printf("[PASS] File round-trip verified\n\n");
        return 0;
    }

    printf("[FAIL] File integrity check failed\n\n");
    return -1;
}

int main(int argc, char *argv[])
{
    const char *mount_path;

    if (argc < 2) {
        printf("Usage: %s <usb_mount_path>\n", argv[0]);
        printf("Example: %s /run/media/$USER/USBDISK\n", argv[0]);
        return 1;
    }

    mount_path = argv[1];
    print_banner(mount_path);

    if (!check_usb_mounted(mount_path))
        return 1;

    if (list_usb_files(mount_path) != 0)
        return 1;
    if (write_test_file(mount_path) != 0)
        return 1;
    if (read_test_file(mount_path) != 0)
        return 1;
    if (verify_file(mount_path) != 0)
        return 1;
    if (list_usb_files(mount_path) != 0)
        return 1;

    printf("Demo completed successfully.\n");
    return 0;
}
