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
	{ 0x1548f2da, "class_create" },
	{ 0x5c2b7174, "cdev_del" },
	{ 0xdb01e8cf, "device_create" },
	{ 0xc28d6dc6, "class_destroy" },
	{ 0x1e6d26a8, "strstr" },
	{ 0x754d539c, "strlen" },
	{ 0x656e4a6e, "snprintf" },
	{ 0xa7eedcc4, "call_usermodehelper" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x9f984513, "strrchr" },
	{ 0xdcd72c5e, "device_destroy" },
	{ 0x89940875, "mutex_lock_interruptible" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x441d0de9, "__kmalloc_large_noprof" },
	{ 0x37a0cba, "kfree" },
	{ 0xb8b6d67f, "filp_open" },
	{ 0x792c38cd, "kernel_write" },
	{ 0x34a24001, "filp_close" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x6072848a, "kernel_read" },
	{ 0xe9830982, "kmalloc_caches" },
	{ 0x7cbe85f5, "__kmalloc_cache_noprof" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xa916b694, "strnlen" },
	{ 0x75ca79b5, "__fortify_panic" },
	{ 0x476b165a, "sized_strscpy" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x92997ed8, "_printk" },
	{ 0x8f0f1a3b, "cdev_init" },
	{ 0xa8966080, "cdev_add" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x993c2ebf, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "2BEF121D0D6A8F6CACE467F");
MODULE_INFO(rhelversion, "10.3");
