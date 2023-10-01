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

#define NEWCHRLED_NAME "newchrled"
#define NEWCHRLED_COUNT 1

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE          (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE  (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE  (0X020E02F4)
#define GPIO1_DR_BASE           (0X0209C000)
#define	GPIO1_GDIR_BASE         (0X0209C004)


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
	struct newcheled_dev *dev = (struct newcheled_dev*)filp->private_data;
	
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

static int __init newchrled_init(void)
{
	int ret = 0;
	int result;
	unsigned int val = 0;
	printk("newchrled_init\r\n");
	/* 1.初始化LED */
	IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);  
	SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4); 
	GPIO1_DR = ioremap(GPIO1_DR_BASE, 4); 
	GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);
	
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
		ret = register_chrdev_region(newchrled.devid, NEWCHRLED_COUNT, NEWCHRLED_NAME);
	} else {
		ret = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_COUNT, NEWCHRLED_NAME);
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
	ret = cdev_init(&newchrled.cdev, &newchrled_fops);
	if(ret < 0) {
		goto fail_cdev;
	}
	ret = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_COUNT);
	if(ret < 0) {
		goto fail_cdev;
	}
	/* 4.自动创建设备节点 */
	newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
	if (IS_ERR(newchrled.class)) { 
		result = PTR_ERR(newchrled.class); 
		goto fail_class;
	}
	newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
	if (IS_ERR(newchrled.device)) { 
		return PTR_ERR(newchrled.device); 
	}
	return 0;
fail_devid:
	return -1;
fail_cdev:
	unregister_chrdev_region(newchrled.devid, NEWCHRLED_COUNT);
	return -2;
fail_class:
	unregister_chrdev_region(newchrled.devid, NEWCHRLED_COUNT);
	cdev_del(&newchrled.cdev);
	return result;
}

static void __exit newchrled_exit(void)
{
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
	unregister_chrdev_region(newchrled.devid, NEWCHRLED_COUNT);
	printk("newchrled_exit\r\n");
	/* 3.摧毁设备 */
	device_destroy(newchrled.class, newchrled.devid);
	/* 4.摧毁类 */
	class_destroy(newchrled.class);
}

/* 注册驱动加载和卸载 */
module_init(newchrled_init);
module_exit(newchrled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");