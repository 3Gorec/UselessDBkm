obj-m += uselesskm.o
INSTALL_DIR=/lib/modules/$(shell uname -r)/kernel/lib
MODULE=uselesskm.ko

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	install ./$(MODULE) $(INSTALL_DIR)
	depmod $(INSTALL_DIR)/$(MODULE)

uninstall:
	rm -rf $(INSTALL_DIR)/$(MODULE)
	depmod -a