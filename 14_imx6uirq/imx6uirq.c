#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define IMX6UIRQ_CNT    1           /* 设备号个数 */
#define IMX6UIRQ_NAME   "imx6uirq"   /* 名字 */
#define KEY_NUM         1
#define KEY0VALUE       0X01
#define INVAKEY         0XFF

/* key结构体 */
struct irq_keydesc {
	int gpio;            					/* io编号 */
	int irqnum;          					/* 中断号 */
	unsigned char value; 					/* 键值 */
	char name[10];                          /* 名字 */
    irqreturn_t (*handler) (int, void*);     /* 中断处理函数 */
};

/* imx6uirq设备结构体 */
struct imx6uirq_dev{
	dev_t   devid;
	int     major;
	int     minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	struct irq_keydesc irqkey[KEY_NUM];
	struct timer_list timer;
	atomic_t keyvalue;
	atomic_t releasekey;
};

struct imx6uirq_dev imx6uirq;          /* led设备 */

static int imx6uirq_open(struct inode *inode, struct file *filp) 
{ 
	filp->private_data = &imx6uirq; /* 设置私有数据 */ 
	

	return 0; 
}

static ssize_t imx6uirq_read(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int ret = 0;
	unsigned char keyvalue;
	unsigned char releasekey;
	struct imx6ulirq_dev *dev = filp->private_data;

	keyvalue = atomic_read(&dev->keyvalue);
	releasekey = atomic_read(&dev->releasekey);

	if(releasekey) {
		if(keyvalue & 0x80) {
			keyvalue &= ~0x80;
			ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
		} else {
			goto data_error;
		}
		atomic_set(&dev->releasekey, 0); /* 按下标志清零 */
	}

	return ret;

data_error:
	return -EINVAL;
}

/* 字符设备操作集合 */
static struct file_operations imx6uirq_fops = { 
	.owner = THIS_MODULE, 
	.open = imx6uirq_open,
	.write = imx6uirq_read, 
};

static irqreturn_t key0_handler(int irq, void *dev_id) { 
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *)dev_id;
	dev->timer.data = (volatile long)dev_id; 
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
	return IRQ_HANDLED; 
}

static void timer_function(unsigned long arg) { 
	int value = 0;
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *)arg; 
	value = gpio_get_value(dev->irqkey[0].gpio);
	if(value == 0) {
		printk("KEY0 Push!\r\n");
		atomic_set(&dev->keyvalue, dev->irqkey[0].value); 
	} else if (value == 1) {
		printk("KEY0 release!\r\n");
		atomic_set(&dev->keyvalue, 0x80 | (dev->irqkey[0].value)); 
		atomic_set(&dev->releasekey, 1);
	}
}

/* 按键初始化 */
static int keyio_init(struct imx6uirq_dev *dev)
{
	int ret = 0;
	int i = 0;
	/* 1.按键初始化 */
	dev->nd = of_find_node_by_path("/key");
	if(dev->nd == NULL) {
		ret = -EINVAL;
		goto fail_nd;
	}

	for(i = 0; i < KEY_NUM; i++) {
		dev->irqkey[i].gpio = of_get_named_gpio(dev->nd, "key-gpios", 0);
	}

	for(i = 0; i < KEY_NUM; i++) {
		memset(dev->irqkey[i].name, 0, sizeof(dev->irqkey[i].name));
		sprintf(dev->irqkey[i].name, "KEY%d", i);
		gpio_request(dev->irqkey[i].gpio, dev->irqkey[i].name);
		gpio_direction_input(dev->irqkey[i].gpio);
		dev->irqkey[i].irqnum = gpio_to_irq(dev->irqkey[i].gpio);
		#if 0
			dev->irqkey[i].irqnum = irq_of_parse_and_map(dev->nd, i);
		#endif
	}
	dev->irqkey[0].handler = key0_handler;
	dev->irqkey[0].value = KEY0VALUE;

	/* 2.按键中断初始化 */
	for(i = 0; i < KEY_NUM; i++) {
		ret = request_irq(dev->irqkey[i].irqnum, dev->irqkey[0].handler, 
							IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_SHARED, 
							dev->irqkey[i].name, &imx6uirq);
		if(ret) {
			printk("irq %d request failed!\r\n", dev->irqkey[i].irqnum);
			goto fail_irq;
		}
	}

	/* 3.初始化定时器 */
	init_timer(&imx6uirq.timer);
	imx6uirq.timer.function = timer_function;

	return 0;

fail_irq:
	for(i = 0; i < KEY_NUM; i++){
		gpio_free(dev->irqkey[i].gpio);
	}
fail_nd:
	return ret;
}

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
	/* 初始化原子变量 */
	atomic_set(&imx6uirq.keyvalue, INVAKEY);
	atomic_set(&imx6uirq.releasekey, 0);
	ret = keyio_init(&imx6uirq);
	if(ret < 0) {
		goto fail_keyinit;
	}

	

 	return 0;

fail_keyinit:
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
	int i = 0;
	/* 1.释放中断 */
	for(i = 0; i < KEY_NUM; i++) {
		free_irq(imx6uirq.irqkey[i].irqnum, &imx6uirq);
	}
	/* 2.释放IO */
	for(i = 0; i < KEY_NUM; i++){
		gpio_free(imx6uirq.irqkey[i].gpio);
	}
	/* 3.删除定时器 */
	del_timer_sync(&imx6uirq.timer);
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
