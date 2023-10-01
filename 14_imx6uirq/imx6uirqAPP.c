#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
/*
 * argc：应用程序参数
 */
#define KEY0VALUE  0XF0
#define INVAKEY    0X00

int main(int argc, char *argv[])
{
    int value = 0;
    int fd, retvalue;
    char *filename;
    unsigned char databuf[1];

    if(argc != 2) {
        printf("Error Usage!\r\n");
        return -1;
    }
    filename = argv[1];

    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        /* code */
        printf("Can't open files %s\r\n", filename);
        return -1;
    }

    /* 循环读取按键值 */
    while (1)
    {
        read(fd, &value, sizeof(value));
        if(value == KEY0VALUE) {
            printf("KEY0 Press, value = %d\r\n", value);
        }
    }

    close(fd);

    return 0;
}