#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"
#include "signal.h"
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: asyncnotiApp.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: 定时器测试应用程序
其他	   	: 无
使用方法	：./asyncnotiApp /dev/imx6uirq 打开测试App
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/7/26 左忠凯创建
***************************************************************/
int fd;
static void sigio_signal_func(int num)
{
	int err;
	unsigned int keyvalue = 0;
	err = read(fd, &keyvalue, sizeof(keyvalue));
	if(err < 0) {
		printf("signal err!");
	} else {
		printf("sigio signal! key value = %d\r\n", keyvalue);
	}
}

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	char *filename;
	unsigned char data;
	int flags = 0;
	
	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

	/* 设置信号处理函数 */
	signal(SIGIO, sigio_signal_func);

	fcntl(fd, F_SETOWN, getpid());
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | FASYNC);

	while (1)
	{
		/* code */
		sleep(2);
	}
	

	close(fd);
	return ret;
}
