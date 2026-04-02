#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>

static struct usb_device_id usb_table[] = {
    { USB_DEVICE_INFO(USB_CLASS_MASS_STORAGE, 0, 0) },
    { }
};
MODULE_DEVICE_TABLE(usb, usb_table);

static const char *usb_speed_to_text(enum usb_device_speed speed)
{
    switch (speed) {
    case USB_SPEED_LOW:
        return "Low (1.5 Mbps - USB 1.0)";
    case USB_SPEED_FULL:
        return "Full (12 Mbps - USB 1.1)";
    case USB_SPEED_HIGH:
        return "High (480 Mbps - USB 2.0)";
    case USB_SPEED_SUPER:
        return "Super (5 Gbps - USB 3.0)";
#ifdef USB_SPEED_SUPER_PLUS
    case USB_SPEED_SUPER_PLUS:
        return "Super+ (10+ Gbps - USB 3.x)";
#endif
    default:
        return "Unknown";
    }
}

static int usb_probe(struct usb_interface *interface,
                     const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(interface);

    printk(KERN_INFO "USB_DRIVER: ========== Device Connected ==========\n");
    printk(KERN_INFO "USB_DRIVER: Vendor  ID    = 0x%04X\n",
           udev->descriptor.idVendor);
    printk(KERN_INFO "USB_DRIVER: Product ID    = 0x%04X\n",
           udev->descriptor.idProduct);

    if (udev->manufacturer)
        printk(KERN_INFO "USB_DRIVER: Manufacturer = %s\n", udev->manufacturer);
    if (udev->product)
        printk(KERN_INFO "USB_DRIVER: Product Name = %s\n", udev->product);
    if (udev->serial)
        printk(KERN_INFO "USB_DRIVER: Serial Number= %s\n", udev->serial);

    printk(KERN_INFO "USB_DRIVER: Bus Number    = %d\n",
           udev->bus->busnum);
    printk(KERN_INFO "USB_DRIVER: Device Addr   = %d\n",
           udev->devnum);
    printk(KERN_INFO "USB_DRIVER: Speed         = %s\n",
           usb_speed_to_text(udev->speed));
    printk(KERN_INFO "USB_DRIVER: ======================================\n");
    return 0;
}

static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "USB_DRIVER: ===== Device Disconnected =====\n");
}

static struct usb_driver my_usb_driver = {
    .name       = "my_usb_driver",
    .probe      = usb_probe,
    .disconnect = usb_disconnect,
    .id_table   = usb_table,
};

static int __init usb_driver_init(void) {
    int ret = usb_register(&my_usb_driver);
    if (ret)
        printk(KERN_ERR "USB_DRIVER: Registration failed (%d)\n", ret);
    else
        printk(KERN_INFO "USB_DRIVER: Registered successfully\n");
    return ret;
}

static void __exit usb_driver_exit(void) {
    usb_deregister(&my_usb_driver);
    printk(KERN_INFO "USB_DRIVER: Unregistered\n");
}

module_init(usb_driver_init);
module_exit(usb_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("USB Mass Storage Driver Demo");
