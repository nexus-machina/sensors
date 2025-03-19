# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := apds9960.o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
	SRC ?= $(shell pwd)
endif

# Device tree overlay source
DTS_SOURCE := apds9960.dts
DTBO_TARGET := apds9960.dtbo

all: modules dtbo

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

dtbo: $(DTBO_TARGET)

$(DTBO_TARGET): $(DTS_SOURCE)
	dtc -@ -I dts -O dtb -o $@ $<
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
	rm -f $(DTBO_TARGET)
