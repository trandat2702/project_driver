obj-m += string_norm.o
KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order

load:
	@echo "=== Loading string_norm driver ==="
	sudo insmod string_norm.ko || (dmesg | tail -5 && exit 1)
	@MAJOR=$$(awk '/string_norm/{print $$1}' /proc/devices); \
	if [ -z "$$MAJOR" ]; then \
		echo "ERROR: string_norm not found in /proc/devices"; exit 1; \
	fi; \
	sudo mknod /dev/string_norm c $$MAJOR 0; \
	sudo chmod 666 /dev/string_norm; \
	echo "=== Loaded: /dev/string_norm (major=$$MAJOR) ==="

unload:
	@echo "=== Unloading string_norm driver ==="
	sudo rmmod string_norm 2>/dev/null || true
	sudo rm -f /dev/string_norm
	@echo "=== Unloaded ==="

status:
	@echo "--- lsmod ---"
	@lsmod | grep string_norm || echo "Not loaded"
	@echo "--- /dev ---"
	@ls -la /dev/string_norm 2>/dev/null || echo "Device node not found"
	@echo "--- dmesg ---"
	@dmesg | grep string_norm | tail -5
