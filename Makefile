obj-m += aaron.o
aaron-objs := aaron_main.o aaron_sock.o aaron_http.o aaron_route.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod aaron.ko

unload:
	sudo rmmod aaron

reload: unload load

.PHONY: all clean load unload reload
