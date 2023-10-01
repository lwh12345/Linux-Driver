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
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define PLATFORM_NAME "platled"
#define PLATFORM_COUNT 1

/* 地址映射后的虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;  
static void __iomem *SW_PAD_GPIO1_IO03; 
static void __iomem *GPIO1_DR; 
static void __iomem *GPIO1_GDIR;

#define LEDOFF  0
#define LEDON   1 

/* LED设备结构体 */
struct newchrled_dev
{
	/* data */
	struct cdev cdev;   /* 字符设备 */
	dev_t devid;
	struct class  *class;
	struct device *device;
	int major;
	int minor;
};

struct newchrled_dev newchrled;

/* LED灯打开/关闭 */
static void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON){
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);       /* 打开LED灯 */
		writel(val, GPIO1_DR);
	}else if (sta == LEDOFF){
		val = readl(GPIO1_DR);
		val |= (1 << 3);       /* 关闭LED灯 */
		writel(val, GPIO1_DR);
	}
}

static int newchrled_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &newchrled;
	return 0;
}

static int newchrled_release(struct inode *inode, struct file *filp)
{
	// struct newcheled_dev *dev = (struct newcheled_dev*)filp->private_data;
	
	return 0;
}

static ssize_t newchrled_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	/* 向用户空间发送数据 */

	return 0;
}

static ssize_t newchrled_write(struct file *filp, const char __user *buf, size_t count, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0){
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}
	led_switch(databuf[0]);
	return 0;
}

static const struct file_operations newchrled_fops = {
	.owner = THIS_MODULE,
	.open = newchrled_open,
	.read = newchrled_read,
	.write = newchrled_write,
	.release = newchrled_release,
};

static int led_probe(struct platform_device *dev)
{
	int i = 0;
	int result;
	struct resource *ledsource[5];
	int ret = 0;
	unsigned int val = 0;
	printk("led driver probe!\r\n");
	/* 初始化LED，字符设备驱动 */
	/* 1.从设备中获取资源 */
	for(i = 0; i < 5; i++) {
		ledsource[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
		if (ledsource[i] == NULL)
			return -EINVAL;
		
	}
	// ledsource[0]->end - ledsource[0]->start + 1;
	/* 内存映射 */
	/* 1.初始化LED */
	IMX6U_CCM_CCGR1 = ioremap(ledsource[0]->start, resource_size(ledsource[0]));
	SW_MUX_GPIO1_IO03 = ioremap(ledsource[1]->start, resource_size(ledsource[1]));  
	SW_PAD_GPIO1_IO03 = ioremap(ledsource[2]->start, resource_size(ledsource[2])); 
	GPIO1_DR = ioremap(ledsource[3]->start, resource_size(ledsource[3])); 
	GPIO1_GDIR = ioremap(ledsource[4]->start, resource_size(ledsource[4]));
	
	/* 2.使能GPIO1时钟 */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);
	val |= (3 << 26); /* 设置新值 */ 
	writel(val, IMX6U_CCM_CCGR1);

	/* 3、设置GPIO1_IO03的复用功能，将其复用为 
	 * GPIO1_IO03，最后设置IO属性。  
	 */
	writel(0x5, SW_MUX_GPIO1_IO03);
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* 4.设置GPIO1_IO03为输出功能 */
	val = readl(GPIO1_GDIR);
	val |= 1 << 3;
	writel(val, GPIO1_GDIR);

	/* 5. */
	val = readl(GPIO1_DR);
	val |= (1 << 3);       /* 关闭LED灯 */
	writel(val, GPIO1_DR);

	newchrled.major = 0;
	/* 2.注册字符设备 */
	if(newchrled.major){
		newchrled.devid = MKDEV(newchrled.major, 0);
		ret = register_chrdev_region(newchrled.devid, PLATFORM_COUNT, PLATFORM_NAME);
	} else {
		ret = alloc_chrdev_region(&newchrled.devid, 0, PLATFORM_COUNT, PLATFORM_NAME);
		newchrled.major = MAJOR(newchrled.devid);
		newchrled.minor = MINOR(newchrled.devid);
	}
	if(ret < 0) {
		printk("newchrled chrdev_region err!\r\n");
		goto fail_devid;
	}
	printk("newchrled major=%d, minor=%d\r\n", newchrled.major, newchrled.minor);
	
	/* 3.注册字符设备 */
	newchrled.cdev.owner = THIS_MODULE;
	cdev_init(&newchrled.cdev, &newchrled_fops);
	if(ret < 0) {
		goto fail_cdev;
	}
	ret = cdev_add(&newchrled.cdev, newchrled.devid, PLATFORM_COUNT);
	if(ret < 0) {
		goto fail_cdev;
	}
	/* 4.自动创建设备节点 */
	newchrled.class = class_create(THIS_MODULE, PLATFORM_NAME);
	if (IS_ERR(newchrled.class)) { 
		result = PTR_ERR(newchrled.class); 
		goto fail_class;
	}
	newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, PLATFORM_NAME);
	if (IS_ERR(newchrled.device)) { 
		return PTR_ERR(newchrled.device); 
	}
	return 0;
fail_devid:
	return -1;
fail_cdev:
	unregister_chrdev_region(newchrled.devid, PLATFORM_COUNT);
	return -2;
fail_class:
	unregister_chrdev_region(newchrled.devid, PLATFORM_COUNT);
	cdev_del(&newchrled.cdev);
	return result;
}

static int led_remove(struct platform_device *dev)
{
	printk("led driver remove!\r\n");
	unsigned int val = 0;
	val = readl(GPIO1_DR);
	val |= (1 << 3);       /* 关闭LED灯 */
	writel(val, GPIO1_DR);

	/* 1.取消地址映射 */
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);
	/* 1.删除字符设备 */
	cdev_del(&newchrled.cdev);
	/* 2.注销设备号 */
	unregister_chrdev_region(newchrled.devid, PLATFORM_COUNT);
	/* 3.摧毁设备 */
	device_destroy(newchrled.class, newchrled.devid);
	/* 4.摧毁类 */
	class_destroy(newchrled.class);
	return 0;
}

/* platform驱动结构体 */
static struct platform_driver led_driver = {
	.driver = {
		.name = "imx6ull-led", /* 驱动名字，用于和设备匹配 */
	},
	.probe = led_probe,
	.remove = led_remove,
};
		
/*
 * @description	: 驱动模块加载 
 * @param 		: 无
 * @return 		: 无
 */
static int __init leddriver_init(void)
{
	return platform_driver_register(&led_driver);
}

/*
 * @description	: 设备模块注销
 * @param 		: 无
 * @return 		: 无
 */
static void __exit leddriver_exit(void)
{
	platform_driver_unregister(&led_driver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
