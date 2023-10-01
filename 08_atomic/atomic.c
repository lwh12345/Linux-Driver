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

#define GPIOLED_CNT    1           /* 设备号个数 */
#define GPIOLED_NAME   "gpioled"   /* 名字 */

#define LEDOFF  0
#define LEDON   1

/* gpioled设备结构体 */
struct gpioled_dev{
	dev_t   devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int     major;
	int     minor;
	struct device_node *nd;
	int led_gpio;
	atomic_t lock;   /* 原子操作 */
};

struct gpioled_dev gpioled;          /* led设备 */

static int led_open(struct inode *inode, struct file *filp) 
{ 
	filp->private_data = &gpioled; /* 设置私有数据 */ 
	/* 判断原子变量 */
	if(atomic_read(&gpioled.lock) <= 0) {
		return -EBUSY;
	} else {
		atomic_dec(&gpioled.lock);
	}

	return 0; 
}

static int led_release(struct inode *inode, struct file *filp)
{
	struct gpioled_dev *dev = filp->private_data;
	/* 关闭驱动文件的时候释放原子变量 */ 
	atomic_inc(&dev->lock);
	return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t count, loff_t *offt)
{
	int ret;
	unsigned char databuf[1];
	struct gpioled_dev *dev = filp->private_data;
	ret = copy_from_user(databuf, buf, count);
	if(ret < 0) {
		return -EINVAL;
	}
	if(databuf[0] == LEDON) {
		gpio_set_value(dev->led_gpio, 0);
	} else if(databuf[0] == LEDOFF) {
		gpio_set_value(gpioled.led_gpio, 1);
	}
	return 0;
}

/* 字符设备操作集合 */
static struct file_operations led_fops = { 
	.owner = THIS_MODULE, 
	.open = led_open,
	.write = led_write, 
	.release = led_release, 
};

/* 模块入口 */
static int __init led_init(void)
{
 	int ret = 0;

	/* 初始化原子变量 */ 
	atomic_set(&gpioled.lock, 1); /* 原子变量初始值为1 */

	/* 注册字符设备 */
	/* 1.申请设备号 */
	gpioled.major = 0;    /* 设备号由内核分配 */
	if(gpioled.major) {
		gpioled.devid = MKDEV(gpioled.major, 0);
		ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
	} else {
		ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
		gpioled.major = MAJOR(gpioled.devid);
		gpioled.minor = MINOR(gpioled.devid);
	}
	printk("gpioled major = %d, minor = %d\r\n", gpioled.major, gpioled.minor);
	if(ret < 0) {
		goto fail_devid;
	}

	/* 2.添加字符设备 */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &led_fops);
	ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
	if(ret < 0)
		goto fail_cdev;

	/* 3.自动创建设备节点 */
	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class)) {
		ret = PTR_ERR(gpioled.class);
		goto fail_class;
	}
	
	/* 4.创建设备 */
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
	if (IS_ERR(gpioled.device)) {
		ret = PTR_ERR(gpioled.device);
		goto fail_device;
	}

	/* 1.获取设备树属性节点内容 */
	gpioled.nd = of_find_node_by_path("/gpioled");
	if(gpioled.nd == NULL) {
		ret = -EINVAL;
		goto fail_findnode;
	}

 	/* 2.获取LED对应的GPIO属性 */
	gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpios", 0);
	if(gpioled.led_gpio < 0) {
		printk("can't find led gpio\r\n");
		ret = -EINVAL;
		goto fail_findnode;
	}
	printk("led gpio num = %d\r\n", gpioled.led_gpio);

	/* 3.申请IO */
	ret = gpio_request(gpioled.led_gpio, "led-gpios");
	if (ret) {
		printk("Failed to request the led gpio\r\n");
		ret = -EINVAL;
		goto fail_findnode;
	}

	/* 4.使用IO,设置为输出 */
	ret = gpio_direction_output(gpioled.led_gpio, 1);
	if (ret) {
		ret = -EINVAL;
		goto fail_setoutput;
	}

	/* 5.输出低电平, 点亮LED灯*/
	gpio_set_value(gpioled.led_gpio, 0);

 	return 0;

fail_setoutput:
	gpio_free(gpioled.led_gpio);
fail_findnode:
	return ret;
fail_device:
	class_destroy(gpioled.class);
fail_class:
	cdev_del(&gpioled.cdev);
fail_cdev:
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
fail_devid:
	return ret;
}

/* 模块出口 */
static void __exit led_exit(void)
{
	gpio_set_value(gpioled.led_gpio, 1);
	/* 删除设备号 */
	cdev_del(&gpioled.cdev);
	/* 释放设备号 */
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
	/* 摧毁设备 */
	device_destroy(gpioled.class, gpioled.devid);
	/* 摧毁类 */
	class_destroy(gpioled.class);
	/* 释放IO */
	gpio_free(gpioled.led_gpio);
}

/* 注册模块入口和出口 */
module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
