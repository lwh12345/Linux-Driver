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
#include <linux/of_irq.h>

#define IMX6UIRQ_CNT    1           /* 设备号个数 */
#define IMX6UIRQ_NAME   "imx6uirq"   /* 名字 */

/* imx6uirq设备结构体 */
struct imx6uirq_dev{
	dev_t   devid;
	int     major;
	int     minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;

};

struct imx6uirq_dev imx6uirq;          /* led设备 */

static int imx6uirq_open(struct inode *inode, struct file *filp) 
{ 
	filp->private_data = &imx6uirq; /* 设置私有数据 */ 
	

	return 0; 
}

static int imx6uirq_release(struct inode *inode, struct file *filp)
{

	
	return 0;
}

static ssize_t imx6uirq_write(struct file *filp, const char __user *buf, size_t count, loff_t *offt)
{

	return 0;
}

/* 字符设备操作集合 */
static struct file_operations imx6uirq_fops = { 
	.owner = THIS_MODULE, 
	.open = imx6uirq_open,
	.write = imx6uirq_write, 
	.release = imx6uirq_release, 
};

/* 模块入口 */
static int __init imx6uirq_init(void)
{
 	int ret = 0;

	/* 注册字符设备 */
	/* 1.申请设备号 */
	imx6uirq.major = 0;    /* 设备号由内核分配 */
	if(imx6uirq.major) {
		imx6uirq.devid = MKDEV(imx6uirq.major, 0);
		ret = register_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
	} else {
		ret = alloc_chrdev_region(&imx6uirq.devid, 0, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
		imx6uirq.major = MAJOR(imx6uirq.devid);
		imx6uirq.minor = MINOR(imx6uirq.devid);
	}
	printk("imx6uirq major = %d, minor = %d\r\n", imx6uirq.major, imx6uirq.minor);
	if(ret < 0) {
		goto fail_devid;
	}

	/* 2.添加字符设备 */
	imx6uirq.cdev.owner = THIS_MODULE;
	cdev_init(&imx6uirq.cdev, &imx6uirq_fops);
	ret = cdev_add(&imx6uirq.cdev, imx6uirq.devid, IMX6UIRQ_CNT);
	if(ret < 0)
		goto fail_cdev;

	/* 3.自动创建设备节点 */
	imx6uirq.class = class_create(THIS_MODULE, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.class)) {
		ret = PTR_ERR(imx6uirq.class);
		goto fail_class;
	}
	
	/* 4.创建设备 */
	imx6uirq.device = device_create(imx6uirq.class, NULL, imx6uirq.devid, NULL, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.device)) {
		ret = PTR_ERR(imx6uirq.device);
		goto fail_device;
	}

 	return 0;

fail_device:
	class_destroy(imx6uirq.class);
fail_class:
	cdev_del(&imx6uirq.cdev);
fail_cdev:
	unregister_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT);
fail_devid:
	return ret;
}

/* 模块出口 */
static void __exit imx6uirq_exit(void)
{

	/* 删除设备号 */
	cdev_del(&imx6uirq.cdev);
	/* 释放设备号 */
	unregister_chrdev_region(imx6uirq.devid, IMX6UIRQ_CNT);
	/* 摧毁设备 */
	device_destroy(imx6uirq.class, imx6uirq.devid);
	/* 摧毁类 */
	class_destroy(imx6uirq.class);

}

/* 注册模块入口和出口 */
module_init(imx6uirq_init);
module_exit(imx6uirq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
