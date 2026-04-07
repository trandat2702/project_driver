obj-m += string_norm.o usb_bridge.o
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
	sudo mknod /dev/string_norm c $$MAJOR 0 2>/dev/null || true; \
	sudo chmod 666 /dev/string_norm; \
	echo "=== Loaded: /dev/string_norm (major=$$MAJOR) ==="

load-usb:
	@echo "=== Loading usb_bridge driver ==="
	sudo insmod usb_bridge.ko || (dmesg | tail -5 && exit 1)
	@MAJOR=$$(awk '/usb_bridge/{print $$1}' /proc/devices); \
	if [ -z "$$MAJOR" ]; then \
		echo "ERROR: usb_bridge not found in /proc/devices"; exit 1; \
	fi; \
	sudo mknod /dev/usb_bridge c $$MAJOR 0 2>/dev/null || true; \
	sudo chmod 666 /dev/usb_bridge; \
	echo "=== Loaded: /dev/usb_bridge (major=$$MAJOR) ==="

load-all: load load-usb

unload:
	@echo "=== Unloading string_norm driver ==="
	sudo rmmod string_norm 2>/dev/null || true
	sudo rm -f /dev/string_norm
	@echo "=== Unloaded ==="

unload-usb:
	@echo "=== Unloading usb_bridge driver ==="
	sudo rmmod usb_bridge 2>/dev/null || true
	sudo rm -f /dev/usb_bridge
	@echo "=== Unloaded ==="

unload-all: unload-usb unload

status:
	@echo "--- lsmod ---"
	@lsmod | grep -E 'string_norm|usb_bridge' || echo "No modules loaded"
	@echo "--- /dev ---"
	@ls -la /dev/string_norm 2>/dev/null || echo "string_norm not found"
	@ls -la /dev/usb_bridge 2>/dev/null || echo "usb_bridge not found"
	@echo "--- dmesg ---"
	@dmesg | grep -E 'string_norm|usb_bridge' | tail -10
