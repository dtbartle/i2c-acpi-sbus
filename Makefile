TARGET		:= $(shell uname -r)
KERNEL_BUILD	:= /usr/src/linux-headers-$(TARGET)
KERNEL_MODULES	:= /lib/modules/$(TARGET)
SYSTEM_MAP      := /boot/System.map-$(TARGET)

DRIVER  := i2c-acpi-sbus

obj-m	:= $(DRIVER).o

.PHONY: all install modules modules_install clean

all: modules

modules clean:
	@$(MAKE) -C $(KERNEL_BUILD) M=$(CURDIR) $@ EXTRA_CFLAGS=-g

install: modules_install

modules_install:
	cp $(DRIVER).ko $(KERNEL_MODULES)/kernel/drivers/i2c/busses
	depmod -a -F $(SYSTEM_MAP) $(TARGET)
