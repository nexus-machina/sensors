# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := nex-apds9960.o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
endif

SRC ?= $(shell pwd)

# Device tree overlay source
DTS_SOURCE := apds9960.dts
DTBO_TARGET := apds9960.dtbo

# Define the path to the dtc executable within the kernel build artifacts
DTC_EXECUTABLE := dtc

# Build the dtc utility, for now dead recipe
# $(DTC_EXECUTABLE): $(DTC_DIR)/Makefile
# 	@echo "Building dtc in $(DTC_DIR) to $(DTC_EXECUTABLE)"
# 	$(MAKE) -C $(KERNEL_SRC) scripts/dtc/dtc
# 	@echo "Finished building dtc"
# 

all: modules

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

dtbo: $(DTBO_TARGET) $(DTC_DIR)/dtc

$(DTBO_TARGET): $(DTS_SOURCE)
	@echo "Compiling DT overlay: apds9960.dts -> apds9960.dtb -> apds9960.dtbo"
	$(DTC_EXECUTABLE) -@ -I dts -O dtb -o $@ $<

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
	rm -f $(DTBO_TARGET)
