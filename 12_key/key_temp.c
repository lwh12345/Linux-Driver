#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h> 
#include <linux/gpio.h> 
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <asm/mach/map.h> 
#include <asm/uaccess.h> 
#include <asm/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#define KEY_CNT    1           /* 设备号个数 */
#define KEY_NAME   "key"   /* 名字 */

#define LEDOFF  0
#define LEDON   1

/* key设备结构体 */
struct key_dev{
	dev_t   devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int     major;
	int     minor;
	struct device_node *nd;
	int key_gpio;

};

struct key_dev key;          /* led设备 */

static int key_open(struct inode *inode, struct file *filp) 
{ 
	filp->private_data = &key; /* 设置私有数据 */ 
	

	return 0; 
}

static int key_release(struct inode *inode, struct file *filp)
{
	struct key_dev *dev = filp->private_data;

	
	return 0;
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t count, loff_t *offt)
{
	// int ret;
	// unsigned char databuf[1];
	// struct key_dev *dev = filp->private_data;
	// ret = copy_from_user(databuf, buf, count);
	// if(ret < 0) {
	// 	return -EINVAL;
	// }
	// if(databuf[0] == LEDON) {
	// 	gpio_set_value(dev->key_gpio, 0);
	// } else if(databuf[0] == LEDOFF) {
	// 	gpio_set_value(key.key_gpio, 1);
	// }
	return 0;
}

/* 字符设备操作集合 */
static struct file_operations key_fops = { 
	.owner = THIS_MODULE, 
	.open = key_open,
	.write = key_write, 
	.release = key_release, 
};

/* 模块入口 */
static int __init mykey_init(void)
{
 	int ret = 0;

	/* 注册字符设备 */
	/* 1.申请设备号 */
	key.major = 0;    /* 设备号由内核分配 */
	if(key.major) {
		key.devid = MKDEV(key.major, 0);
		ret = register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
	} else {
		ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
		key.major = MAJOR(key.devid);
		key.minor = MINOR(key.devid);
	}
	printk("key major = %d, minor = %d\r\n", key.major, key.minor);
	if(ret < 0) {
		goto fail_devid;
	}

	/* 2.添加字符设备 */
	key.cdev.owner = THIS_MODULE;
	cdev_init(&key.cdev, &key_fops);
	ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
	if(ret < 0)
		goto fail_cdev;

	/* 3.自动创建设备节点 */
	key.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(key.class)) {
		ret = PTR_ERR(key.class);
		goto fail_class;
	}
	
	/* 4.创建设备 */
	key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
	if (IS_ERR(key.device)) {
		ret = PTR_ERR(key.device);
		goto fail_device;
	}

 	return 0;

fail_setoutput:
	gpio_free(key.key_gpio);
fail_findnode:
	return ret;
fail_device:
	class_destroy(key.class);
fail_class:
	cdev_del(&key.cdev);
fail_cdev:
	unregister_chrdev_region(key.devid, KEY_CNT);
fail_devid:
	return ret;
}

/* 模块出口 */
static void __exit mykey_exit(void)
{

	/* 删除设备号 */
	cdev_del(&key.cdev);
	/* 释放设备号 */
	unregister_chrdev_region(key.devid, KEY_CNT);
	/* 摧毁设备 */
	device_destroy(key.class, key.devid);
	/* 摧毁类 */
	class_destroy(key.class);
	/* 释放IO */
	gpio_free(key.key_gpio);
}

/* 注册模块入口和出口 */
module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
