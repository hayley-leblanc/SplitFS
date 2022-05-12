#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
	char *write_buf = "hello!";
	char read_buf[16];
    char read_buf1[16];

    int fd1 = open("/mnt/pmem_emul/sang.txt", O_CREAT | O_RDWR, 0777);
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



    int fd2 = open("/mnt/pmem_emul/sam.txt", O_CREAT | O_RDWR, 0777);
	if (fd2 < 0) {
		perror("open");
		return fd2;
	}

	printf("fd2: %d\n", fd2);

	int ret2 = pwrite(fd2, write_buf, strlen(write_buf), 0);
	if (ret2 < 0) {
		perror("write");
		// close(fd1);
		return ret2;
	}

	printf("wrote %d bytes\n", ret2);

	ret2 = pread(fd2, read_buf1, 16, 0);
	if (ret2 < 0) {
		perror("read");
		return ret2;
	}

	close(fd2);

	printf("DATA READ FROM REMOTE SERVER: 2 %s\n", read_buf1);




    return 0;
}
