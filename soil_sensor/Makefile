obj-m := stemma_driver.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

#current directory 
PWD := $(shell pwd)

#default target 
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
# clean target
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
