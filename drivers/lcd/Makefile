# Makefile

obj-m += i2c_lcd1602_driver.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod i2c_lcd1602_driver.ko

uninstall:
	sudo rmmod i2c_lcd1602_driver

reload: uninstall install

test:
	echo "Hello LCD!" | sudo tee /dev/lcd1602

.PHONY: all clean install uninstall reload test
