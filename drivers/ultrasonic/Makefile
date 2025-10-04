# Makefile

obj-m += hc_sr04p_driver.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod hc_sr04p_driver.ko

uninstall:
	sudo rmmod hc_sr04p_driver

.PHONY: all clean install uninstall
