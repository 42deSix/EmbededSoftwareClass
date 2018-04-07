obj-m := desk.o

KERNELDIR := ~/linux-rpi-3.18.7/
ARM := ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabi-
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(ARM) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(ARM) clean
