#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
	char *write_buf = "hello!";

    int fd = open("/mnt/pmem_emul/foo.txt", O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		perror("open");
		return fd;
	}

	printf("fd: %d\n", fd);

	int ret = write(fd, write_buf, strlen(write_buf));
	if (fd < 0) {
		perror("write");
		// close(fd);
		return fd;
	}

	// close(fd);

    return 0;
}