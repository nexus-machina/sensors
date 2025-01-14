# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := hello.o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
endif

# Device tree overlay source
DTS_SOURCE := apds9600.dts
DTBO_TARGET := apds9600.dtbo

all: modules dtbo

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

dtbo: $(DTBO_TARGET)

$(DTBO_TARGET): $(DTS_SOURCE)
	dtc -@ -I dts -O dtb -o $@ $<
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f $(DTBO_TARGET)
