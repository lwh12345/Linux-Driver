#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

#define CHRDEVBASE_MAJOR	200				/* 主设备号 */
#define CHRDEVBASE_NAME		"chrdevbase" 	/* 设备名     */

static char readbuf[100]; /* 读缓冲区 */ 
static char writebuf[100]; /* 写缓冲区 */ 
static char kerneldata[] = {"kernel data!"};

static int chrdevbase_open(struct inode *inode, struct file *filp)
{
	//printk("chrdevbase_open\r\n");
	return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *filp)
{
	//printk("chrdevbase_release\r\n");
	return 0;
}

static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;
	
	/* 向用户空间发送数据 */
	memcpy(readbuf, kerneldata, sizeof(kerneldata));
	//printk("chrdevbase read!\r\n");
	retvalue = copy_to_user(buf, readbuf, cnt);
	if(retvalue == 0){ 
		printk("kernel senddata ok!\r\n"); 
	}else{ 
		printk("kernel senddata failed!\r\n"); 
	}
	return 0;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;
	/* 接收用户空间传递给内核的数据并且打印出来 */
	retvalue = copy_from_user(writebuf, buf, cnt);
	if(retvalue == 0){
		printk("kernel recevdata:%s\r\n", writebuf);
	}else{
		printk("kernel recevdata failed!\r\n");
	}
	//printk("chrdevbase write!\r\n");
	return 0;
}

static struct file_operations chrdevbase_fops = {
	.owner = THIS_MODULE,	
	.open = chrdevbase_open,
	.read = chrdevbase_read,
	.write = chrdevbase_write,
	.release = chrdevbase_release,
};

static int __init chrdevbase_init(void)
{
	int ret = 0;
	printk("chrdevbase_init\r\n");
	ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
	if(ret < 0)
	{
		printk("chrdevbase init failed\r\n");
	}
	return 0;
}

static void __exit chadevbase_exit(void)
{
	printk("chrdevbase_exit\r\n");
	unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
}

/*
 模块入口与出口
 */
module_init(chrdevbase_init);
module_exit(chadevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");