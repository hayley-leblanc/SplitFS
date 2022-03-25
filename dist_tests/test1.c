#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

int main(void) {
    char buf[8];
    memset(buf, 'a', 7);

    int fd = open("/mnt/pmem_emul/test.txt", O_RDWR | O_CREAT, 0777);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    int ret = write(fd, buf, 8);
    if (ret < 0) {
        perror("write");
        close(fd);
        return ret;
    }
    

    close(fd);

    return 0;
}