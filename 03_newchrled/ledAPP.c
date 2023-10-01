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
#define LEDOFF 0 
#define LEDON  1

int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    unsigned char databuf[1];

    if(argc != 3) {
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
    databuf[0] = atoi(argv[2]);

    retvalue = write(fd, databuf, sizeof(databuf));
    if(retvalue < 0){
        printf("LED Control Failed!\r\n");
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}