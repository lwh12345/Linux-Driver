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
#define KEY_NAME   "key"       /* 名字 */

#define KEY0VALUE  0XF0
#define INVAKEY    0X00

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
	atomic_t keyvalue;
};

struct key_dev key;          /* led设备 */

static int key_open(struct inode *inode, struct file *filp) 
{ 
	filp->private_data = &key; /* 设置私有数据 */ 
	

	return 0; 
}

static int key_release(struct inode *inode, struct file *filp)
{
	// struct key_dev *dev = filp->private_data;

	
	return 0;
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t count, loff_t *offt)
{

	return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int value;
	struct key_dev *dev = filp->private_data;
	int ret = 0;

	/* 按下 */
	if(gpio_get_value(dev->key_gpio) == 0) {
		while (!gpio_get_value(dev->key_gpio));
		atomic_set(&dev->keyvalue, KEY0VALUE);
	} else {
		atomic_set(&dev->keyvalue, INVAKEY);
	}

	value = atomic_read(&dev->keyvalue);

	ret = copy_to_user(buf, &value, sizeof(value));

	return ret;
}

/* 字符设备操作集合 */
static struct file_operations key_fops = { 
	.owner = THIS_MODULE, 
	.open = key_open,
	.write = key_write, 
	.read = key_read,
	.release = key_release, 
};

/* key io初始化 */
static int keyio_init(struct key_dev *dev)
{
	int ret = 0;

	dev->nd = of_find_node_by_path("/key");
	if(dev->nd == NULL) {
		ret = -EINVAL;
		goto fail_nd;
	}

	dev->key_gpio = of_get_named_gpio(dev->nd, "key-gpios", 0);
	if (dev->key_gpio < 0)
	{
		/* code */
		ret = -EINVAL;
		goto fail_gpio;
	}

	ret = gpio_request(dev->key_gpio, "key0");
	if (ret)
	{
		/* code */
		ret = -EBUSY;
		printk("IO %d can't request!\r\n", dev->key_gpio);
		goto fail_request;
	}
	
	ret = gpio_direction_input(dev->key_gpio);
	if(ret < 0) {
		ret = -EINVAL;
		goto fail_input;
	}
	return 0;

fail_input:
	gpio_free(dev->key_gpio);
fail_request:
fail_gpio:
fail_nd:
	return ret;
}

/* 模块入口 */
static int __init mykey_init(void)
{
 	int ret = 0;

	/* 初始化atomic */
	atomic_set(&key.keyvalue, INVAKEY);

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

	ret = keyio_init(&key);
	if(ret < 0) {
		goto fail_device;
	}
 	return 0;

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
