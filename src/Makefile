ifneq ($(KERNELRELEASE),)
	obj-m := main.o
else

KDIR = /lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS := -std=c99

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

disclean: clean
	@rm *.ko *.symvers

endif
