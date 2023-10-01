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
#include <asm/mach/map.h> 
#include <asm/uaccess.h> 
#include <asm/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#define BEEP_CNT    1           /* 设备号个数 */
#define BEEP_NAME   "beep"   /* 名字 */

#define BEEPOFF  0
#define BEEPON   1

/* beep设备结构体 */ 
struct beep_dev{
	dev_t   devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int     major;
	int     minor;
	struct device_node *nd;
	int beep_gpio;
};

struct beep_dev beep;          /* beep设备 */

static int beep_open(struct inode *inode, struct file *filp) 
{ 
	filp->private_data = &beep; /* 设置私有数据 */ 
	return 0; 
}

static int beep_release(struct inode *inode, struct file *filp)
{
	
	return 0;
}

static ssize_t beep_write(struct file *filp, const char __user *buf, size_t count, loff_t *offt)
{
	int ret;
	unsigned char databuf[1];
	struct beep_dev *dev = filp->private_data;
	ret = copy_from_user(databuf, buf, count);
	if(ret < 0) {
		return -EINVAL;
	}
	if(databuf[0] == BEEPON) {
		gpio_set_value(dev->beep_gpio, 0);
	} else if(databuf[0] == BEEPOFF) {
		gpio_set_value(dev->beep_gpio, 1);
	}
	return 0;
}

/* 字符设备操作集合 */
static struct file_operations beep_fops = { 
	.owner = THIS_MODULE, 
	.open = beep_open,
	.write = beep_write, 
	.release = beep_release, 
};

/* 模块入口 */
static int __init beep_init(void)
{
 	int ret = 0;

	/* 注册字符设备 */
	/* 1.申请设备号 */
	beep.major = 0;    /* 设备号由内核分配 */
	if(beep.major) {
		beep.devid = MKDEV(beep.major, 0);
		ret = register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);
	} else {
		ret = alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
		beep.major = MAJOR(beep.devid);
		beep.minor = MINOR(beep.devid);
	}
	printk("beep major = %d, minor = %d\r\n", beep.major, beep.minor);
	if(ret < 0) {
		goto fail_devid;
	}

	/* 2.添加字符设备 */
	beep.cdev.owner = THIS_MODULE;
	cdev_init(&beep.cdev, &beep_fops);
	ret = cdev_add(&beep.cdev, beep.devid, BEEP_CNT);
	if(ret < 0)
		goto fail_cdev;

	/* 3.自动创建设备节点 */
	beep.class = class_create(THIS_MODULE, BEEP_NAME);
	if (IS_ERR(beep.class)) {
		ret = PTR_ERR(beep.class);
		goto fail_class;
	}
	
	/* 4.创建设备 */
	beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
	if (IS_ERR(beep.device)) {
		ret = PTR_ERR(beep.device);
		goto fail_device;
	}

	/* 初始化BEEP */
	/* 1.获取设备树属性节点内容 */
	beep.nd = of_find_node_by_path("/beep");
	if(beep.nd == NULL) {
		ret = -EINVAL;
		goto fail_nd;
	}

 	/* 2.获取BEEP对应的GPIO属性 */
	beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpios", 0);
	if(beep.beep_gpio < 0) {
		printk("can't find beep gpio\r\n");
		ret = -EINVAL;
		goto fail_nd;
	}
	printk("beep gpio num = %d\r\n", beep.beep_gpio);

	/* 3.申请IO */
	ret = gpio_request(beep.beep_gpio, "beep-gpio");
	if (ret) {
		printk("Failed to request the beep gpio\r\n");
		ret = -EINVAL;
		goto fail_nd;
	}

	/* 4.使用IO,设置为输出 */
	ret = gpio_direction_output(beep.beep_gpio, 0);
	if (ret) {
		ret = -EINVAL;
		goto fail_setoutput;
	}

	/* 5.输出低电平, 开启锋鸣器*/
	gpio_set_value(beep.beep_gpio, 1);

 	return 0;

fail_setoutput:
	gpio_free(beep.beep_gpio);
fail_nd:
	device_destroy(beep.class, beep.devid);
fail_device:
	class_destroy(beep.class);
fail_class:
	cdev_del(&beep.cdev);
fail_cdev:
	unregister_chrdev_region(beep.devid, BEEP_CNT);
fail_devid:
	return ret;
}

/* 模块出口 */
static void __exit beep_exit(void)
{
	/* 关闭锋鸣器 */
	gpio_set_value(beep.beep_gpio, 1);
	/* 删除设备号 */
	cdev_del(&beep.cdev);
	/* 释放设备号 */
	unregister_chrdev_region(beep.devid, BEEP_CNT);
	/* 摧毁设备 */
	device_destroy(beep.class, beep.devid);
	/* 摧毁类 */
	class_destroy(beep.class);
	gpio_free(beep.beep_gpio);
}

/* 注册模块入口和出口 */
module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
