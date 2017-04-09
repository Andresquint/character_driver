#Gaurav Anil Yeole
#UFID 54473949
#EEL5934 Advanced System Programming
#Assignement - 5

obj-m := char_driver.o

KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	
clean:
	rm -f *.o.cmd *.symvers *.order *~ *.mod.* *.ko *.o