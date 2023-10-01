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
int main(int argc, char *argv[])
{
    int ret = 0;
    int fd = 0;
    char *filename;
    char readbuf[100], writebuf[100];
    static char usrdata[] = {"usr data!"};

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
    if(atoi(argv[2]) == 1){ /* 读 */
        /* read */
        ret = read(fd, readbuf, 50);
        if (ret < 0){
            /* code */
            printf("read file %s failed!\r\n", filename);
        }
        else{
            printf("read data:%s\r\n",readbuf);
        }
    }

    /* write */
    if(atoi(argv[2]) == 2){
        memcpy(writebuf, usrdata, sizeof(usrdata));
        ret = write(fd, writebuf, 50);
        if (ret < 0){
            /* code */
            printf("write file %s failed!\r\n", filename);
        }
        else{

        }
    }
    

    /* close */
    ret = close(fd);
    if (ret < 0)
    {
        /* code */
        printf("close file %s failed!\r\n", filename);
    }
    
    return 0;
}