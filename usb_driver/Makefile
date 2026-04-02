obj-m += usb_driver.o
KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)
CC    := gcc
CFLAGS := -Wall -Wextra -g
DEMO_TARGET := usb_file_demo

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f $(DEMO_TARGET)

load:
	sudo insmod usb_driver.ko || (dmesg | tail -5 && exit 1)
	@echo "=== USB driver loaded. Plug in a USB drive and run: dmesg | grep USB_DRIVER ==="

unload:
	sudo rmmod usb_driver 2>/dev/null || true

demo: $(DEMO_TARGET)

$(DEMO_TARGET): usb_file_demo.c
	$(CC) $(CFLAGS) -o $(DEMO_TARGET) usb_file_demo.c

run_demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

clean_demo:
	rm -f $(DEMO_TARGET)
