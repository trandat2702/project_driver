#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/kmod.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include "usb_bridge_ioctl.h"

#define DEVICE_NAME "usb_bridge"
#define CLASS_NAME  "usb_bridge_class"
#define CHUNK_SIZE  4096
#define MAX_TRACKED_MOUNTS 16

static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;
static DEFINE_MUTEX(bridge_mutex);

struct tracked_mount_entry {
    int in_use;
    char device[USB_MAX_PATH_LEN];
    char mount_point[USB_MAX_PATH_LEN];
};

static struct tracked_mount_entry tracked_mounts[MAX_TRACKED_MOUNTS];

static int find_tracked_mount_index(const char *mount_point) {
    int i;

    for (i = 0; i < MAX_TRACKED_MOUNTS; i++) {
        if (tracked_mounts[i].in_use &&
            strcmp(tracked_mounts[i].mount_point, mount_point) == 0)
            return i;
    }
    return -1;
}

static int track_mount(const char *device, const char *mount_point) {
    int i;
    int idx = find_tracked_mount_index(mount_point);

    if (idx >= 0) {
        strscpy(tracked_mounts[idx].device, device, USB_MAX_PATH_LEN);
        return 0;
    }

    for (i = 0; i < MAX_TRACKED_MOUNTS; i++) {
        if (!tracked_mounts[i].in_use) {
            tracked_mounts[i].in_use = 1;
            strscpy(tracked_mounts[i].device, device, USB_MAX_PATH_LEN);
            strscpy(tracked_mounts[i].mount_point, mount_point, USB_MAX_PATH_LEN);
            return 0;
        }
    }

    return -ENOSPC;
}

static void untrack_mount(const char *mount_point) {
    int idx = find_tracked_mount_index(mount_point);

    if (idx < 0)
        return;

    memset(&tracked_mounts[idx], 0, sizeof(tracked_mounts[idx]));
}

static int is_mount_tracked(const char *mount_point) {
    return find_tracked_mount_index(mount_point) >= 0;
}

static int validate_path(const char *path) {
    if (!path || path[0] == '\0')
        return -EINVAL;
    if (path[0] != '/')
        return -EINVAL;
    if (strstr(path, ".."))
        return -EINVAL;
    if (strlen(path) >= USB_MAX_PATH_LEN)
        return -ENAMETOOLONG;
    return 0;
}

static int validate_text_extension(const char *path) {
    const char *ext;
    ext = strrchr(path, '.');
    if (!ext)
        return -EINVAL;
    if (strcmp(ext, ".txt") != 0 && strcmp(ext, ".csv") != 0)
        return -EINVAL;
    return 0;
}

static int run_mount_helper(const char *device, const char *mount_point,
                            const char *fs_type, const char *options) {
    char *argv[8];
    char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
    int ret;
    char fs_opt[USB_MAX_FSTYPE_LEN + 4];
    char mount_opt[USB_MAX_OPTIONS_LEN + 4];

    if (validate_path(device) < 0 || validate_path(mount_point) < 0)
        return -EINVAL;

    argv[0] = "/bin/mount";
    argv[1] = "-t";
    snprintf(fs_opt, sizeof(fs_opt), "%s", fs_type[0] ? fs_type : "auto");
    argv[2] = fs_opt;

    if (options[0]) {
        argv[3] = "-o";
        snprintf(mount_opt, sizeof(mount_opt), "%s", options);
        argv[4] = mount_opt;
        argv[5] = (char *)device;
        argv[6] = (char *)mount_point;
        argv[7] = NULL;
    } else {
        argv[3] = (char *)device;
        argv[4] = (char *)mount_point;
        argv[5] = NULL;
    }

    ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
    if (ret < 0) {
        printk(KERN_ERR "usb_bridge: mount failed, ret=%d\n", ret);
        return ret;
    }
    if (ret > 0) {
        printk(KERN_ERR "usb_bridge: mount exit code=%d\n", ret);
        return -EIO;
    }

    printk(KERN_INFO "usb_bridge: mounted %s on %s\n", device, mount_point);
    return 0;
}

static int run_umount_helper(const char *mount_point) {
    char *argv[3];
    char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
    int ret;

    if (validate_path(mount_point) < 0)
        return -EINVAL;

    argv[0] = "/bin/umount";
    argv[1] = (char *)mount_point;
    argv[2] = NULL;

    ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
    if (ret < 0) {
        printk(KERN_ERR "usb_bridge: umount failed, ret=%d\n", ret);
        return ret;
    }
    if (ret > 0) {
        printk(KERN_ERR "usb_bridge: umount exit code=%d\n", ret);
        return -EIO;
    }

    printk(KERN_INFO "usb_bridge: unmounted %s\n", mount_point);
    return 0;
}

static ssize_t do_kernel_read(const char *path, char *buf, size_t max_len) {
    struct file *f;
    ssize_t ret;
    loff_t pos = 0;

    f = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(f)) {
        printk(KERN_ERR "usb_bridge: cannot open %s for read\n", path);
        return PTR_ERR(f);
    }

    ret = kernel_read(f, buf, max_len - 1, &pos);
    if (ret >= 0)
        buf[ret] = '\0';

    filp_close(f, NULL);
    return ret;
}

static ssize_t do_kernel_write(const char *path, const char *buf, size_t len) {
    struct file *f;
    ssize_t ret;
    loff_t pos = 0;

    f = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(f)) {
        printk(KERN_ERR "usb_bridge: cannot open %s for write\n", path);
        return PTR_ERR(f);
    }

    ret = kernel_write(f, buf, len, &pos);
    filp_close(f, NULL);
    return ret;
}

static int do_kernel_copy(const char *src, const char *dst, unsigned int *bytes_copied) {
    struct file *fsrc, *fdst;
    char *buf;
    ssize_t nread, nwrite;
    loff_t src_pos = 0, dst_pos = 0;
    unsigned int total = 0;
    int ret = 0;

    buf = kmalloc(CHUNK_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    fsrc = filp_open(src, O_RDONLY, 0);
    if (IS_ERR(fsrc)) {
        kfree(buf);
        return PTR_ERR(fsrc);
    }

    fdst = filp_open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(fdst)) {
        filp_close(fsrc, NULL);
        kfree(buf);
        return PTR_ERR(fdst);
    }

    while ((nread = kernel_read(fsrc, buf, CHUNK_SIZE, &src_pos)) > 0) {
        nwrite = kernel_write(fdst, buf, nread, &dst_pos);
        if (nwrite < 0) {
            ret = nwrite;
            break;
        }
        total += nwrite;
    }

    if (nread < 0)
        ret = nread;

    *bytes_copied = total;

    filp_close(fdst, NULL);
    filp_close(fsrc, NULL);
    kfree(buf);

    return ret;
}

static long usb_bridge_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    int ret = 0;

    if (mutex_lock_interruptible(&bridge_mutex))
        return -ERESTARTSYS;

    switch (cmd) {
    case USB_OP_MOUNT: {
        struct usb_mount_args margs;
        if (copy_from_user(&margs, (void __user *)arg, sizeof(margs))) {
            ret = -EFAULT;
            break;
        }
        margs.device[USB_MAX_PATH_LEN - 1] = '\0';
        margs.mount_point[USB_MAX_PATH_LEN - 1] = '\0';
        margs.fs_type[USB_MAX_FSTYPE_LEN - 1] = '\0';
        margs.options[USB_MAX_OPTIONS_LEN - 1] = '\0';
        ret = run_mount_helper(margs.device, margs.mount_point,
                               margs.fs_type, margs.options);
        if (ret == 0) {
            int track_ret = track_mount(margs.device, margs.mount_point);
            if (track_ret < 0) {
                int rollback_ret;

                printk(KERN_ERR "usb_bridge: cannot track mount %s (ret=%d), rollback\n",
                       margs.mount_point, track_ret);
                rollback_ret = run_umount_helper(margs.mount_point);
                if (rollback_ret < 0)
                    printk(KERN_ERR "usb_bridge: rollback umount failed for %s (ret=%d)\n",
                           margs.mount_point, rollback_ret);
                ret = track_ret;
            }
        }
        break;
    }

    case USB_OP_UNMOUNT: {
        struct usb_umount_args uargs;
        if (copy_from_user(&uargs, (void __user *)arg, sizeof(uargs))) {
            ret = -EFAULT;
            break;
        }
        uargs.mount_point[USB_MAX_PATH_LEN - 1] = '\0';
        if (!is_mount_tracked(uargs.mount_point)) {
            ret = -EPERM;
            printk(KERN_WARNING "usb_bridge: reject unmount of unmanaged mount %s\n",
                   uargs.mount_point);
            break;
        }
        ret = run_umount_helper(uargs.mount_point);
        if (ret == 0)
            untrack_mount(uargs.mount_point);
        break;
    }

    case USB_OP_CHECK_MOUNT_OWNERSHIP: {
        struct usb_mount_owner_args oargs;

        if (copy_from_user(&oargs, (void __user *)arg, sizeof(oargs))) {
            ret = -EFAULT;
            break;
        }
        oargs.mount_point[USB_MAX_PATH_LEN - 1] = '\0';
        ret = validate_path(oargs.mount_point);
        if (ret < 0)
            break;

        oargs.managed = is_mount_tracked(oargs.mount_point) ? 1 : 0;
        if (copy_to_user((void __user *)arg, &oargs, sizeof(oargs))) {
            ret = -EFAULT;
            break;
        }
        ret = 0;
        break;
    }

    case USB_OP_READ_TEXT: {
        struct usb_read_args *rargs;
        ssize_t n;

        rargs = kmalloc(sizeof(*rargs), GFP_KERNEL);
        if (!rargs) {
            ret = -ENOMEM;
            break;
        }

        if (copy_from_user(rargs, (void __user *)arg, sizeof(*rargs))) {
            kfree(rargs);
            ret = -EFAULT;
            break;
        }
        rargs->file_path[USB_MAX_PATH_LEN - 1] = '\0';

        ret = validate_path(rargs->file_path);
        if (ret < 0) {
            kfree(rargs);
            break;
        }
        ret = validate_text_extension(rargs->file_path);
        if (ret < 0) {
            kfree(rargs);
            break;
        }

        n = do_kernel_read(rargs->file_path, rargs->content, USB_MAX_CONTENT_LEN);
        if (n < 0) {
            ret = n;
            kfree(rargs);
            break;
        }

        rargs->content_len = n;
        if (copy_to_user((void __user *)arg, rargs, sizeof(*rargs))) {
            ret = -EFAULT;
        }
        kfree(rargs);
        break;
    }

    case USB_OP_WRITE_TEXT: {
        struct usb_write_args *wargs;
        ssize_t n;

        wargs = kmalloc(sizeof(*wargs), GFP_KERNEL);
        if (!wargs) {
            ret = -ENOMEM;
            break;
        }

        if (copy_from_user(wargs, (void __user *)arg, sizeof(*wargs))) {
            kfree(wargs);
            ret = -EFAULT;
            break;
        }
        wargs->file_path[USB_MAX_PATH_LEN - 1] = '\0';

        ret = validate_path(wargs->file_path);
        if (ret < 0) {
            kfree(wargs);
            break;
        }
        ret = validate_text_extension(wargs->file_path);
        if (ret < 0) {
            kfree(wargs);
            break;
        }

        if (wargs->content_len > USB_MAX_CONTENT_LEN)
            wargs->content_len = USB_MAX_CONTENT_LEN;

        n = do_kernel_write(wargs->file_path, wargs->content, wargs->content_len);
        if (n < 0) {
            ret = n;
        } else {
            ret = 0;
        }
        kfree(wargs);
        break;
    }

    case USB_OP_COPY_HOST_TO_USB:
    case USB_OP_COPY_USB_TO_HOST: {
        struct usb_copy_args cargs;
        unsigned int copied = 0;

        if (copy_from_user(&cargs, (void __user *)arg, sizeof(cargs))) {
            ret = -EFAULT;
            break;
        }
        cargs.src_path[USB_MAX_PATH_LEN - 1] = '\0';
        cargs.dst_path[USB_MAX_PATH_LEN - 1] = '\0';

        ret = validate_path(cargs.src_path);
        if (ret < 0) break;
        ret = validate_path(cargs.dst_path);
        if (ret < 0) break;
        ret = validate_text_extension(cargs.src_path);
        if (ret < 0) break;
        ret = validate_text_extension(cargs.dst_path);
        if (ret < 0) break;

        ret = do_kernel_copy(cargs.src_path, cargs.dst_path, &copied);
        cargs.bytes_copied = copied;

        if (copy_to_user((void __user *)arg, &cargs, sizeof(cargs))) {
            ret = -EFAULT;
        }
        break;
    }

    default:
        ret = -ENOTTY;
    }

    mutex_unlock(&bridge_mutex);
    return ret;
}

static int usb_bridge_open(struct inode *inode, struct file *f) {
    return 0;
}

static int usb_bridge_release(struct inode *inode, struct file *f) {
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = usb_bridge_open,
    .release        = usb_bridge_release,
    .unlocked_ioctl = usb_bridge_ioctl,
};

static int __init usb_bridge_init(void) {
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ERR "usb_bridge: alloc_chrdev_region failed\n");
        return -1;
    }

    cdev_init(&c_dev, &fops);
    if (cdev_add(&c_dev, dev_num, 1) < 0) {
        printk(KERN_ERR "usb_bridge: cdev_add failed\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    cl = class_create(CLASS_NAME);
    if (IS_ERR(cl)) {
        printk(KERN_ERR "usb_bridge: class_create failed\n");
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(cl);
    }

    if (device_create(cl, NULL, dev_num, NULL, DEVICE_NAME) == NULL) {
        printk(KERN_ERR "usb_bridge: device_create failed\n");
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    printk(KERN_INFO "usb_bridge: registered, major=%d\n", MAJOR(dev_num));
    return 0;
}

static void __exit usb_bridge_exit(void) {
    device_destroy(cl, dev_num);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "usb_bridge: unregistered\n");
}

module_init(usb_bridge_init);
module_exit(usb_bridge_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("USB Bridge Driver for mount/unmount/read/write/copy operations");
MODULE_VERSION("1.0");
