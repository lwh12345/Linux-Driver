#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"
#include "linux/input.h"

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
static struct input_event inputevent;
int main(int argc, char *argv[])
{
	int fd;
	int err = 0;
	char *filename;
	unsigned char data;
	
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

	while (1)
	{
		err = read(fd, &inputevent, sizeof(inputevent));
		if(err > 0) {
			switch (inputevent.type)
			{
				case EV_KEY:
					if(inputevent.code < BTN_MISC) {
						printf("key %d %s\r\n", inputevent.code, inputevent.value ? "press" : "release");
					} else {
						printf("button %d %s\r\n", inputevent.code, inputevent.value ? "press" : "release");
					}
					break;
				case EV_SYN:
					break;
				case EV_REL:
					break;
				case EV_ABS:
					break;	
			}
		} else {
			printf("读取数据失败\r\n");
		}
	}

	close(fd);
	return 0;
}
