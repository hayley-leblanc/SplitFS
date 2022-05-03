#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
	char *write_buf = "hello!";
	char read_buf[16];

    int fd = open("/mnt/pmem_emul/foo.txt", O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		perror("open");
		return fd;
	}

	printf("fd: %d\n", fd);

	int ret = pwrite(fd, write_buf, strlen(write_buf), 0);
	if (ret < 0) {
		perror("write");
		// close(fd);
		return ret;
	}

	// ret = pread(fd, read_buf, 16, 0);
	// if (ret < 0) {
	// 	perror("read");
	// 	return ret;
	// }

	close(fd);

	// printf("DATA READ FROM REMOTE SERVER: %s\n", read_buf);


    return 0;
}