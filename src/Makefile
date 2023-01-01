ifneq ($(KERNELRELEASE),)
	obj-m := netpmod.o
	netpmod-objs := netp.o netp_mod.o
else

KDIR = /lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS := -std=c99

.PHONY: default
default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

.PHONY: load
load:
	sudo insmod netpmod.ko

.PHONY: unload
unload:
	sudo rmmod netpmod.ko

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: disclean
disclean: clean
	@rm *.ko *.symvers

endif
