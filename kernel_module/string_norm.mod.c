#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xec7bd9af, "class_create" },
	{ 0xde2bc62d, "cdev_del" },
	{ 0xc03df407, "device_create" },
	{ 0xbd29f104, "class_destroy" },
	{ 0x89940875, "mutex_lock_interruptible" },
	{ 0xa916b694, "strnlen" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x75ca79b5, "__fortify_panic" },
	{ 0x4d9d6b12, "device_destroy" },
	{ 0x11089ac7, "_ctype" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x92997ed8, "_printk" },
	{ 0x554740dc, "cdev_init" },
	{ 0xf566b27f, "cdev_add" },
	{ 0x5fb309e8, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "FE7CFE53CE9ACAC22C10474");
MODULE_INFO(rhelversion, "10.2");
