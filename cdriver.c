#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>	// for atomic counter "limit"
#include <linux/cred.h>		// credentials of the current process/task_struct

#define DEVICE_NAME "my_device"
#define BUF_SIZE 1024

static char device_buffer[BUF_SIZE];


static dev_t dev;
static struct cdev my_cdev;
static struct class* my_class;

static struct device* device;

static atomic_t user_ref_count = ATOMIC_INIT(0);
static atomic_t current_users  = ATOMIC_INIT(0);
static kuid_t current_user_id  = INVALID_UID;



static atomic_t limit = ATOMIC_INIT(1);
// will be using this as allow boolean, if 1 thread is allowed to access the operation


static int my_open(struct inode* inode, struct file* file){
	static int counter = 0;
	
	
	// atomic_cmpxchg(ptr, old, new)
	// Atomically:
	//   if (*ptr == old)
	//       *ptr = new
	//       return old
	//   else
	//       return *ptr (no change made)
	
	if(atomic_cmpxchg(&current_users, 0, 1) == 0 || uid_eq(current_user_id, current_uid()) ){
		// first ever user
		atomic_inc(&user_ref_count);
		if(atomic_read(&user_ref_count)==1)current_user_id = current_uid();	// first ever access by this user
	}
	else{
		// a user is already in
		pr_info("access denied as a user is already using this driver\n");
		return -EBUSY;
	}
	
	
	
	
	if(atomic_cmpxchg(&limit, 1, 0) != 1){
		// cannot allow concurrent access
		atomic_dec(&user_ref_count);
		if(atomic_read(&user_ref_count)==0){
			atomic_set(&current_users, 0);
			current_user_id=INVALID_UID;
		}
		pr_info("cannot allow concurrent access\n");
		return -EBUSY;
	}
	// now allow count is 0 using cmpxchg
	
	
	counter++;
	pr_info("device opened %dth time\n", counter);
	return 0;

}


static int my_release(struct inode* inode, struct file* file){
	
	atomic_dec(&user_ref_count);
	if(atomic_read(&user_ref_count)==0){
		atomic_set(&current_users, 0);
		current_user_id=INVALID_UID;
	}
	
	atomic_set(&limit, 1);  // Reset to available
	
	pr_info("device closed/released\n");
	return 0;

}


static int data_size=0;

static ssize_t my_read(struct file* file, char* __user buf, size_t count, loff_t* offset){
	
	ssize_t bytes_to_read;
	if(*offset >= data_size)return 0; // EOF
	
	bytes_to_read = min(count, data_size - (size_t)(*offset));
	
	int ret=copy_to_user(buf, device_buffer+(*offset), bytes_to_read);
	if(ret!=0){
		printk(KERN_INFO "error reading in my_read\n");
		return -1;
	}
	
	*offset+=bytes_to_read;
	return bytes_to_read;
}

static ssize_t my_write(struct file* file, const char* __user buf, size_t count, loff_t* offset){
	
	ssize_t bytes_to_write = min(count, (size_t)(BUF_SIZE-1));
	
	int ret = copy_from_user(device_buffer, buf, bytes_to_write);
	if(ret!=0){
		printk(KERN_INFO "error writing in my_write\n");
		return -1;
	}	
	device_buffer[bytes_to_write] = '\0';
	printk(KERN_INFO "%s\n", device_buffer);
	
	data_size=bytes_to_write;
	return bytes_to_write;
	
}



static struct file_operations fops={

	.open=my_open,
	.owner = THIS_MODULE,
	.read=my_read,
	.write=my_write,
	.release=my_release,

};


static int __init my_init(void){
	
	int ret=alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if(ret!=0){
		printk(KERN_INFO "failed to register device major and minor number\n");
		return 1;
	}
	
	
	cdev_init(&my_cdev, &fops);
	
	cdev_add(&my_cdev, dev, 1);
	
	
	my_class = class_create("my_class");
	
	device=device_create(my_class, NULL, dev, NULL, DEVICE_NAME);
	if (IS_ERR(device)) {
		pr_err("could not create device\n");
		class_destroy(my_class);
		unregister_chrdev_region(dev, 1);
		return PTR_ERR(device);
	}
	
	pr_info("module mounted\n");
	
	

	return 0;
}


static void __exit my_exit(void){
	
	pr_info("cleanup started\n");
	
	device_destroy(my_class, dev);
	class_destroy(my_class);
	cdev_del(&my_cdev);
	
	unregister_chrdev_region(dev, 1);
	
	pr_info("cleanup done\n");

	return;
}



MODULE_AUTHOR("asit");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("character driver");


module_init(my_init);
module_exit(my_exit);






