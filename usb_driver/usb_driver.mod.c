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
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xe684ccf6, "usb_deregister" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xe6fd774f, "usb_register_driver" },
	{ 0x92997ed8, "_printk" },
	{ 0x5fb309e8, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v*p*d*dc08dsc00dp00ic*isc*ip*in*");

MODULE_INFO(srcversion, "EE4CF59EDAFA9A549410A53");
MODULE_INFO(rhelversion, "10.2");
