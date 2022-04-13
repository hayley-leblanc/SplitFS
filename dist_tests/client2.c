#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int fd = open("/mnt/pmem_emul/foo.txt", O_RDWR);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    close(fd);


    return 0;
}