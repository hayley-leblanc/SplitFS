#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
	char *write_buf = "hello!";
	char read_buf[16];

    int fd1 = open("/mnt/pmem_emul/foo.txt", O_CREAT | O_RDWR, 0777);
	if (fd1 < 0) {
		perror("open");
		return fd1;
	}

	printf("fd1: %d\n", fd1);

	int ret1 = pwrite(fd1, write_buf, strlen(write_buf), 0);
	if (ret1 < 0) {
		perror("write");
		// close(fd1);
		return ret1;
	}

	printf("wrote %d bytes\n", ret1);

	ret1 = pread(fd1, read_buf, 16, 0);
	if (ret1 < 0) {
		perror("read");
		return ret1;
	}

	close(fd1);

	printf("DATA READ FROM REMOTE SERVER: 1 %s\n", read_buf);
    return 0;
}
