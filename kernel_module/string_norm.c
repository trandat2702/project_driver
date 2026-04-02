#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mutex.h>
#include "string_norm.h"

#define DEVICE_NAME "string_norm"
#define CLASS_NAME  "string_norm_class"
#define BUF_SIZE    1024

static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;
static char input_buf[BUF_SIZE];
static char processed_buf[BUF_SIZE];
static DEFINE_MUTEX(dev_mutex);

static ssize_t dev_read(struct file *f, char __user *buf,
                         size_t len, loff_t *off) {
    size_t data_len, bytes_to_copy;
    ssize_t ret;

    if (mutex_lock_interruptible(&dev_mutex))
        return -ERESTARTSYS;

    if (processed_buf[0] == '\0') {
        mutex_unlock(&dev_mutex);
        return 0;
    }

    data_len = strlen(processed_buf);
    if (*off >= data_len) {
        mutex_unlock(&dev_mutex);
        return 0;
    }

    bytes_to_copy = data_len - *off;
    if (bytes_to_copy > len)
        bytes_to_copy = len;

    if (copy_to_user(buf, processed_buf + *off, bytes_to_copy)) {
        printk(KERN_ERR "string_norm: copy_to_user failed\n");
        mutex_unlock(&dev_mutex);
        return -EFAULT;
    }

    *off += bytes_to_copy;
    ret = bytes_to_copy;
    mutex_unlock(&dev_mutex);
    return ret;
}

static void normalize_string_kernel(const char *input, char *output, int buf_size) {
    int i = 0, j = 0;
    int in_word = 0, new_word = 1;

    if (!input || !output || buf_size <= 0) return;
    memset(output, 0, buf_size);

    while (input[i] && isspace((unsigned char)input[i])) i++;

    while (input[i] && j < buf_size - 1) {
        if (isspace((unsigned char)input[i])) {
            if (in_word) {
                output[j++] = ' ';
                in_word = 0;
                new_word = 1;
            }
        } else {
            in_word = 1;
            if (new_word) {
                output[j++] = toupper((unsigned char)input[i]);
                new_word = 0;
            } else {
                output[j++] = tolower((unsigned char)input[i]);
            }
        }
        i++;
    }
    if (j > 0 && output[j-1] == ' ')
        output[--j] = '\0';
    else
        output[j] = '\0';
}

static ssize_t dev_write(struct file *f, const char __user *buf,
                          size_t len, loff_t *off) {
    size_t copy_len = (len >= BUF_SIZE) ? BUF_SIZE - 1 : len;

    if (mutex_lock_interruptible(&dev_mutex))
        return -ERESTARTSYS;

    memset(input_buf, 0, BUF_SIZE);
    memset(processed_buf, 0, BUF_SIZE);

    if (copy_from_user(input_buf, buf, copy_len)) {
        printk(KERN_ERR "string_norm: copy_from_user failed\n");
        mutex_unlock(&dev_mutex);
        return -EFAULT;
    }
    input_buf[copy_len] = '\0';

    normalize_string_kernel(input_buf, processed_buf, BUF_SIZE);

    printk(KERN_INFO "string_norm: input=[%s]\n", input_buf);
    printk(KERN_INFO "string_norm: output=[%s]\n", processed_buf);

    mutex_unlock(&dev_mutex);
    return copy_len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
    .write = dev_write,
};

static int __init string_norm_init(void) {
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ERR "string_norm: alloc_chrdev_region failed\n");
        return -1;
    }

    cdev_init(&c_dev, &fops);
    if (cdev_add(&c_dev, dev_num, 1) < 0) {
        printk(KERN_ERR "string_norm: cdev_add failed\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    cl = class_create(CLASS_NAME);
    if (IS_ERR(cl)) {
        printk(KERN_ERR "string_norm: class_create failed\n");
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(cl);
    }

    if (device_create(cl, NULL, dev_num, NULL, DEVICE_NAME) == NULL) {
        printk(KERN_ERR "string_norm: device_create failed\n");
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    printk(KERN_INFO "string_norm: registered, major=%d\n", MAJOR(dev_num));
    return 0;
}

static void __exit string_norm_exit(void) {
    device_destroy(cl, dev_num);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "string_norm: unregistered\n");
}

module_init(string_norm_init);
module_exit(string_norm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("String Normalization Character Device Driver");
MODULE_VERSION("1.0");
