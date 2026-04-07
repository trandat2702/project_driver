#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "usb_bridge_ioctl.h"

#define DEVICE USB_BRIDGE_DEVICE_PATH
#define TEST_DIR "/tmp/usb_bridge_test"

static int g_total = 0, g_passed = 0, g_skipped = 0;

static int driver_available(void) {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static void setup_test_dir(void) {
    mkdir(TEST_DIR, 0755);
}

static void cleanup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    system(cmd);
}

static void test_result(const char *name, int pass, int skip) {
    g_total++;
    if (skip) {
        printf("[SKIP] %s\n", name);
        g_skipped++;
    } else if (pass) {
        printf("[PASS] %s\n", name);
        g_passed++;
    } else {
        printf("[FAIL] %s\n", name);
    }
}

/* ========== WRITE TESTS ========== */

static void test_write_txt(void) {
    const char *name = "Write .txt file";
    struct usb_write_args *args;
    char path[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(path, sizeof(path), "%s/test.txt", TEST_DIR);
    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, path, USB_MAX_PATH_LEN - 1);
    const char *content = "Hello World!\nLine 2\nLine 3";
    memcpy(args->content, content, strlen(content));
    args->content_len = strlen(content);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    struct stat st;
    test_result(name, ret >= 0 && stat(path, &st) == 0 && st.st_size > 0, 0);
}

static void test_write_csv(void) {
    const char *name = "Write .csv file";
    struct usb_write_args *args;
    char path[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(path, sizeof(path), "%s/data.csv", TEST_DIR);
    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, path, USB_MAX_PATH_LEN - 1);
    const char *content = "ID,Name,Score\n1,Alice,95\n2,Bob,87\n3,Charlie,92";
    memcpy(args->content, content, strlen(content));
    args->content_len = strlen(content);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    struct stat st;
    test_result(name, ret >= 0 && stat(path, &st) == 0, 0);
}

static void test_write_empty_content(void) {
    const char *name = "Write empty content";
    struct usb_write_args *args;
    char path[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(path, sizeof(path), "%s/empty.txt", TEST_DIR);
    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, path, USB_MAX_PATH_LEN - 1);
    args->content_len = 0;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    struct stat st;
    test_result(name, ret >= 0 && stat(path, &st) == 0, 0);
}

static void test_write_overwrite(void) {
    const char *name = "Overwrite existing file";
    struct usb_write_args *args;
    char path[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(path, sizeof(path), "%s/overwrite.txt", TEST_DIR);
    
    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, path, USB_MAX_PATH_LEN - 1);
    memcpy(args->content, "First content", 13);
    args->content_len = 13;
    fd = open(DEVICE, O_RDWR);
    ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);

    memcpy(args->content, "New content", 11);
    args->content_len = 11;
    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret >= 0, 0);
}

/* ========== READ TESTS ========== */

static void test_read_txt(void) {
    const char *name = "Read .txt file";
    struct usb_read_args *args;
    char path[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(path, sizeof(path), "%s/test.txt", TEST_DIR);
    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, path, USB_MAX_PATH_LEN - 1);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_READ_TEXT, args);
    close(fd);

    int pass = (ret >= 0 && strstr(args->content, "Hello World") != NULL);
    if (!pass) printf("  Content: '%s'\n", args->content);
    free(args);
    test_result(name, pass, 0);
}

static void test_read_csv(void) {
    const char *name = "Read .csv file";
    struct usb_read_args *args;
    char path[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(path, sizeof(path), "%s/data.csv", TEST_DIR);
    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, path, USB_MAX_PATH_LEN - 1);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_READ_TEXT, args);
    close(fd);

    int pass = (ret >= 0 && strstr(args->content, "ID,Name,Score") != NULL);
    free(args);
    test_result(name, pass, 0);
}

static void test_read_nonexist(void) {
    const char *name = "Read nonexistent file fails";
    struct usb_read_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, "/tmp/nonexistent_file_12345.txt", USB_MAX_PATH_LEN - 1);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_READ_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

/* ========== SECURITY TESTS ========== */

static void test_reject_pdf(void) {
    const char *name = "Reject .pdf extension";
    struct usb_write_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    snprintf(args->file_path, USB_MAX_PATH_LEN, "%s/doc.pdf", TEST_DIR);
    memcpy(args->content, "test", 4);
    args->content_len = 4;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

static void test_reject_exe(void) {
    const char *name = "Reject .exe extension";
    struct usb_write_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    snprintf(args->file_path, USB_MAX_PATH_LEN, "%s/virus.exe", TEST_DIR);
    memcpy(args->content, "malware", 7);
    args->content_len = 7;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

static void test_reject_no_extension(void) {
    const char *name = "Reject file without extension";
    struct usb_write_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    snprintf(args->file_path, USB_MAX_PATH_LEN, "%s/noextension", TEST_DIR);
    memcpy(args->content, "test", 4);
    args->content_len = 4;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

static void test_reject_relative_path(void) {
    const char *name = "Reject relative path";
    struct usb_write_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, "relative/path.txt", USB_MAX_PATH_LEN - 1);
    memcpy(args->content, "test", 4);
    args->content_len = 4;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

static void test_reject_path_traversal(void) {
    const char *name = "Reject path traversal (..)";
    struct usb_write_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    strncpy(args->file_path, "/tmp/../etc/test.txt", USB_MAX_PATH_LEN - 1);
    memcpy(args->content, "hack", 4);
    args->content_len = 4;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

static void test_reject_empty_path(void) {
    const char *name = "Reject empty path";
    struct usb_write_args *args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    args = calloc(1, sizeof(*args));
    args->file_path[0] = '\0';
    memcpy(args->content, "test", 4);
    args->content_len = 4;

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_WRITE_TEXT, args);
    close(fd);
    free(args);

    test_result(name, ret < 0, 0);
}

/* ========== COPY TESTS ========== */

static void test_copy_txt(void) {
    const char *name = "Copy .txt file";
    struct usb_copy_args args;
    char src[USB_MAX_PATH_LEN], dst[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(src, sizeof(src), "%s/test.txt", TEST_DIR);
    snprintf(dst, sizeof(dst), "%s/test_copy.txt", TEST_DIR);

    memset(&args, 0, sizeof(args));
    strncpy(args.src_path, src, USB_MAX_PATH_LEN - 1);
    strncpy(args.dst_path, dst, USB_MAX_PATH_LEN - 1);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_COPY_HOST_TO_USB, &args);
    close(fd);

    struct stat st;
    int pass = (ret >= 0 && stat(dst, &st) == 0 && args.bytes_copied > 0);
    if (pass) printf("  Copied %u bytes\n", args.bytes_copied);
    test_result(name, pass, 0);
}

static void test_copy_csv(void) {
    const char *name = "Copy .csv file";
    struct usb_copy_args args;
    char src[USB_MAX_PATH_LEN], dst[USB_MAX_PATH_LEN];
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    snprintf(src, sizeof(src), "%s/data.csv", TEST_DIR);
    snprintf(dst, sizeof(dst), "%s/data_backup.csv", TEST_DIR);

    memset(&args, 0, sizeof(args));
    strncpy(args.src_path, src, USB_MAX_PATH_LEN - 1);
    strncpy(args.dst_path, dst, USB_MAX_PATH_LEN - 1);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_COPY_USB_TO_HOST, &args);
    close(fd);

    struct stat st;
    test_result(name, ret >= 0 && stat(dst, &st) == 0, 0);
}

static void test_copy_nonexist_src(void) {
    const char *name = "Copy nonexistent source fails";
    struct usb_copy_args args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    memset(&args, 0, sizeof(args));
    strncpy(args.src_path, "/tmp/nonexist12345.txt", USB_MAX_PATH_LEN - 1);
    strncpy(args.dst_path, "/tmp/dst.txt", USB_MAX_PATH_LEN - 1);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_COPY_HOST_TO_USB, &args);
    close(fd);

    test_result(name, ret < 0, 0);
}

static void test_copy_reject_pdf(void) {
    const char *name = "Copy rejects .pdf extension";
    struct usb_copy_args args;
    int fd, ret;

    if (!driver_available()) { test_result(name, 0, 1); return; }

    memset(&args, 0, sizeof(args));
    snprintf(args.src_path, USB_MAX_PATH_LEN, "%s/test.txt", TEST_DIR);
    snprintf(args.dst_path, USB_MAX_PATH_LEN, "%s/output.pdf", TEST_DIR);

    fd = open(DEVICE, O_RDWR);
    ret = ioctl(fd, USB_OP_COPY_HOST_TO_USB, &args);
    close(fd);

    test_result(name, ret < 0, 0);
}

/* ========== MAIN ========== */

int main(void) {
    printf("=== USB_BRIDGE DRIVER INTEGRATION TESTS ===\n");
    printf("Device: %s\n\n", DEVICE);

    if (!driver_available()) {
        printf("WARNING: USB bridge driver not loaded.\n");
        printf("Run: sudo insmod usb_bridge.ko\n\n");
    }

    setup_test_dir();

    printf("--- Write Operations ---\n");
    test_write_txt();
    test_write_csv();
    test_write_empty_content();
    test_write_overwrite();

    printf("\n--- Read Operations ---\n");
    test_read_txt();
    test_read_csv();
    test_read_nonexist();

    printf("\n--- Security: Extension Validation ---\n");
    test_reject_pdf();
    test_reject_exe();
    test_reject_no_extension();

    printf("\n--- Security: Path Validation ---\n");
    test_reject_relative_path();
    test_reject_path_traversal();
    test_reject_empty_path();

    printf("\n--- Copy Operations ---\n");
    test_copy_txt();
    test_copy_csv();
    test_copy_nonexist_src();
    test_copy_reject_pdf();

    cleanup_test_dir();

    printf("\n=== Results: %d passed, %d failed, %d skipped (of %d) ===\n",
           g_passed, g_total - g_passed - g_skipped, g_skipped, g_total);

    return (g_passed + g_skipped == g_total) ? 0 : 1;
}
