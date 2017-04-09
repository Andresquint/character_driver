//Gaurav Anil Yeole
//UFID 54473949
//EEL5934 Advanced System Programming
//Assignement - 5
	
	
#include<linux/slab.h>
#include<linux/kdev_t.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/device.h>
#include<asm/uaccess.h>
#include<linux/semaphore.h>
#include<linux/moduleparam.h>
#include <linux/ioctl.h>




#define MYDEV_NAME "mycdrv"


#define ramdisk_size (size_t)(16*PAGE_SIZE)
#define MYCLASS_NAME cdd_class
//#define MAGIC_NUM 'k'
#define CDRV_IOC_MAGIC 'k'
#define ASP_CLEAR_BUF _IO(CDRV_IOC_MAGIC,1)

static int NUM_DEVICES = 3;
static unsigned int count = 1;
struct class *my_class;
//static int N;
static dev_t first;

module_param(NUM_DEVICES,  int, S_IRUGO);

static long mycdrv_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static loff_t mycdrv_lseek(struct file *file, loff_t offset, int orig);
static void __exit my_exit(void);
static int mycdrv_open(struct inode *inode, struct file *file);
static int mycdrv_release(struct inode *inode, struct file *file);
static ssize_t mycdrv_read(struct file *file, char __user * buf, size_t lbuf, loff_t * ppos);
static ssize_t mycdrv_write(struct file *file, const char __user * buf, size_t lbuf, loff_t * ppos);



struct asp_mycdev{
	struct cdev dev;
	char* ramdisk;
	struct semaphore sem; 
	dev_t devNo;
};

struct asp_mycdev *aspcdev;
//aspcdev = kmalloc(sizeof(struct asp_mycdev)*N,GFP_KERNEL);

static const struct file_operations mycdrv_fops = {
	.owner = THIS_MODULE,
	.read = mycdrv_read,
	.write = mycdrv_write,
	.open = mycdrv_open,
	.release = mycdrv_release,
	.llseek = mycdrv_lseek,
	.unlocked_ioctl = mycdrv_unlocked_ioctl
};



static int __init my_init(void){
	int add_status,i=0;
	aspcdev = kmalloc(sizeof(struct asp_mycdev)*NUM_DEVICES,GFP_KERNEL);
	my_class = class_create(THIS_MODULE,"cdd_class");
	if(alloc_chrdev_region(&first,0,NUM_DEVICES,MYDEV_NAME)<0){
			pr_err("Failed to allocate character device region and number\n");
			return -1;
	}
	
	for (i=0;i<NUM_DEVICES;i++){
		aspcdev[i].ramdisk = kzalloc(ramdisk_size,GFP_KERNEL);
		aspcdev[i].devNo = MKDEV(MAJOR(first),MINOR(first) +i); 
		
		sema_init(&aspcdev[i].sem,1);
		cdev_init(&aspcdev[i].dev, &mycdrv_fops);
		add_status = cdev_add(&aspcdev[i].dev,aspcdev[i].devNo,count);
		if(add_status<0){
			printk(KERN_ERR "Adding Failed \n");
			unregister_chrdev_region(first, NUM_DEVICES);
			return 1;
		}
		
		device_create(my_class,NULL,aspcdev[i].devNo,NULL,"mycdrv%d",i);
		
		pr_info("\nSucceeded in registering %dth character device %s\n",i+1,MYDEV_NAME);
		pr_info("Major no is %d, Minor no is %d\n",MAJOR(aspcdev[i].devNo),MINOR(aspcdev[i].devNo));
					
	}	
			
	return 0;
}
static void __exit my_exit(void){
	int i=0;
	for (i=0;i<NUM_DEVICES;i++){
		cdev_del(&aspcdev[i].dev);
		unregister_chrdev_region(aspcdev[i].devNo,count);
		pr_info("\n%dth device unregistered\n",i+1);
		kfree(aspcdev[i].ramdisk);
	}
	kfree(aspcdev);
	class_destroy(my_class);
}


static int mycdrv_open(struct inode *inode, struct file *file){
	struct asp_mycdev *device;
	pr_info("OPENING device:%s:\n\n",MYDEV_NAME);
	
	device = container_of(inode->i_cdev, struct asp_mycdev, dev);
	file->private_data = device;
	
	if((file -> f_flags & O_ACCMODE)==O_WRONLY){
		if(down_interruptible(&device->sem))
			return -ERESTARTSYS;
		
		up(&device -> sem);	
	}
	
	return 0;
}

static int mycdrv_release(struct inode *inode, struct file *file){
	pr_info("CLOSING device:%s\n\n",MYDEV_NAME);
	return 0;
}

static ssize_t mycdrv_read(struct file *file, char __user * buf, size_t lbuf, loff_t * ppos)
{	
	int nbytes;
	struct asp_mycdev *dev;
	dev = file -> private_data;
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_to_user(buf, dev->ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	pr_info("\n READING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	
	up(&dev->sem);
	return nbytes;
}

static ssize_t mycdrv_write(struct file *file, const char __user * buf, size_t lbuf,
	     loff_t * ppos)
{
	int nbytes;
	struct asp_mycdev *dev;
	dev = file -> private_data;
	
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	if ((lbuf + *ppos) > ramdisk_size) {
		pr_info("trying to read past end of device,"
			"aborting because this is just a stub!\n");
		return 0;
	}
	nbytes = lbuf - copy_from_user(dev->ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
	pr_info("\n WRITING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	
	up(&dev->sem);
	return nbytes;
}



static loff_t mycdrv_lseek(struct file *file, loff_t offset, int orig){
	loff_t testpos;
	struct asp_mycdev *dev;
	dev = file -> private_data;
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	switch(orig){
		case SEEK_SET: 
			testpos = offset;
			break;
		case SEEK_CUR:
			testpos = file->f_pos + offset;
			break;
		case SEEK_END:
			testpos = ramdisk_size + offset;
			break;
		default:
			return -EINVAL;
	}
	//testpos = testpos<ramdisk_size?testpos:ramdisk_size;
	
	if(testpos > ramdisk_size){
		dev->ramdisk = krealloc(dev->ramdisk,testpos,GFP_KERNEL);
		memset(dev->ramdisk+ramdisk_size,0,testpos-ramdisk_size);
	}

	testpos = testpos>=0?testpos:0;	
	file->f_pos=testpos;
	pr_info("Seeking to position = %ld\n",(long)testpos);
	
	up(&dev->sem);
	return testpos;
}

static long mycdrv_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	
	struct asp_mycdev *dev;
	dev = file -> private_data;
	
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	if(_IOC_TYPE(cmd) != CDRV_IOC_MAGIC){
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) != 1){
		return -ENOTTY;
	}
	
	if (cmd == ASP_CLEAR_BUF){
		memset(dev->ramdisk,0,ramdisk_size);
		file->f_pos = 0;
	}
	
	up(&dev->sem);
	return 0;
}

module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("GAURAV YEOLE");


























