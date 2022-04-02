#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int fd = open("/mnt/pmem_emul/foo.txt", O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		perror("open");
		return fd;
	}

	printf("fd: %d\n", fd);

	// close(fd);

    return 0;
}