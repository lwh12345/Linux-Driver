#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/input.h>
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

#define KEYINPUT_CNT		1			/* 设备号个数 	*/
#define KEYINPUT_NAME		"keyinput"	/* 名字 		*/
#define KEY0VALUE			0X01		/* KEY0按键值 	*/
#define INVAKEY				0XFF		/* 无效的按键值 */
#define KEY_NUM				1			/* 按键数量 	*/

/* 中断IO描述结构体 */
struct irq_keydesc {
	int gpio;								/* gpio */
	int irqnum;								/* 中断号     */
	unsigned char value;					/* 按键对应的键值 */
	char name[10];							/* 名字 */
	irqreturn_t (*handler)(int, void *);	/* 中断服务函数 */
};

/* keyinput设备结构体 */
struct keyinput_dev{
	struct device_node	*nd; /* 设备节点 */
	atomic_t keyvalue;		/* 有效的按键键值 */
	atomic_t releasekey;	/* 标记是否完成一次完成的按键，包括按下和释放 */
	struct timer_list timer;/* 定义一个定时器*/
	struct irq_keydesc irqkeydesc[KEY_NUM];	/* 按键描述数组 */
	unsigned char curkeynum;				/* 当前的按键号 */
	struct input_dev *inputdev;
};

struct keyinput_dev keyinputdev;	/* irq设备 */

/* @description		: 中断服务函数，开启定时器，延时10ms，
 *				  	  定时器用于按键消抖。
 * @param - irq 	: 中断号 
 * @param - dev_id	: 设备结构。
 * @return 			: 中断执行结果
 */
static irqreturn_t key0_handler(int irq, void *dev_id)
{
	struct keyinput_dev *dev = (struct keyinput_dev *)dev_id;

	dev->curkeynum = 0;
	dev->timer.data = (volatile long)dev_id;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));	/* 10ms定时 */
	return IRQ_RETVAL(IRQ_HANDLED);
}

/* @description	: 定时器服务函数，用于按键消抖，定时器到了以后
 *				  再次读取按键值，如果按键还是处于按下状态就表示按键有效。
 * @param - arg	: 设备结构变量
 * @return 		: 无
 */
void timer_function(unsigned long arg)
{
	unsigned char value;
	unsigned char num;
	struct irq_keydesc *keydesc;
	struct keyinput_dev *dev = (struct keyinput_dev *)arg;

	num = dev->curkeynum;
	keydesc = &dev->irqkeydesc[num];

	value = gpio_get_value(keydesc->gpio); 	/* 读取IO值 */
	if(value == 0){ 						/* 按下按键 */
		/* 上报按键值 */
		input_event(dev->inputdev, EV_KEY, KEY_0, 1);
		input_sync(dev->inputdev);
	}
	else{ 									/* 按键松开 */
		/* 上报按键值 */
		input_event(dev->inputdev, EV_KEY, KEY_0, 0);
		input_sync(dev->inputdev);		
	}	
}

/*
 * @description	: 按键IO初始化
 * @param 		: 无
 * @return 		: 无
 */
static int keyio_init(void)
{
	unsigned char i = 0;
	int ret = 0;
	
	keyinputdev.nd = of_find_node_by_path("/key");
	if (keyinputdev.nd== NULL){
		printk("key node not find!\r\n");
		return -EINVAL;
	} 

	/* 提取GPIO */
	for (i = 0; i < KEY_NUM; i++) {
		keyinputdev.irqkeydesc[i].gpio = of_get_named_gpio(keyinputdev.nd ,"key-gpios", i);
		if (keyinputdev.irqkeydesc[i].gpio < 0) {
			printk("can't get key%d\r\n", i);
		}
	}
	
	/* 初始化key所使用的IO，并且设置成中断模式 */
	for (i = 0; i < KEY_NUM; i++) {
		memset(keyinputdev.irqkeydesc[i].name, 0, sizeof(keyinputdev.irqkeydesc[i].name));	/* 缓冲区清零 */
		sprintf(keyinputdev.irqkeydesc[i].name, "KEY%d", i);		/* 组合名字 */
		gpio_request(keyinputdev.irqkeydesc[i].gpio, keyinputdev.irqkeydesc[i].name);
		gpio_direction_input(keyinputdev.irqkeydesc[i].gpio);	
		keyinputdev.irqkeydesc[i].irqnum = irq_of_parse_and_map(keyinputdev.nd, i);

		printk("key%d:gpio=%d, irqnum=%d\r\n",i, keyinputdev.irqkeydesc[i].gpio, 
                                         keyinputdev.irqkeydesc[i].irqnum);
	}
	/* 申请中断 */
	keyinputdev.irqkeydesc[0].handler = key0_handler;
	keyinputdev.irqkeydesc[0].value = KEY_0;
	
	for (i = 0; i < KEY_NUM; i++) {
		ret = request_irq(keyinputdev.irqkeydesc[i].irqnum, keyinputdev.irqkeydesc[i].handler, 
		                 IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING|IRQF_SHARED, keyinputdev.irqkeydesc[i].name, &keyinputdev);
		if(ret < 0){
			printk("irq %d request failed!\r\n", keyinputdev.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
	}

	/* 创建定时器 */
	init_timer(&keyinputdev.timer);
	keyinputdev.timer.function = timer_function;
	return 0;
}

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init keyinput_init(void)
{
	int ret = 0;
	/* 1.初始化按键 */
	keyio_init();

	/* 2.注册input_dev */
	keyinputdev.inputdev = input_allocate_device();
	if(keyinputdev.inputdev == NULL) {
		ret = -EINVAL;
		goto fail_keyint;
	}
	keyinputdev.inputdev->name = KEYINPUT_NAME;
	__set_bit(EV_KEY, keyinputdev.inputdev->evbit); /* 按键事件 */
	__set_bit(EV_REP, keyinputdev.inputdev->evbit); /* 重复事件 */
	__set_bit(KEY_0, keyinputdev.inputdev->keybit); /* 按键号 */

	/* 注册input_dev */ 
	ret = input_register_device(keyinputdev.inputdev);
	if(ret) {
		goto fail_input_register;
	}

	return 0;

fail_input_register:
	input_free_device(keyinputdev.inputdev);
fail_keyint:
	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit keyinput_exit(void)
{
	unsigned int i = 0;
	/* 删除定时器 */
	del_timer_sync(&keyinputdev.timer);	/* 删除定时器 */
		
	/* 释放中断 */
	for (i = 0; i < KEY_NUM; i++) {
		free_irq(keyinputdev.irqkeydesc[i].irqnum, &keyinputdev);
		gpio_free(keyinputdev.irqkeydesc[i].gpio);
	}

	/* 注销input_dev */
	input_unregister_device(keyinputdev.inputdev);
	input_free_device(keyinputdev.inputdev);

}

module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zuozhongkai");
