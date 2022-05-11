#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUFSIZE 4194304 

int main(void) {
    char buf[BUFSIZE];
    char *filename_base = "/mlfs/test_file";
    char index[5];
    char filename[64];
    double cpu_time_used;
    clock_t start, end;

    for (int i = 0; i < 10; i++) {
        sprintf(index, "%d", i);
        strcat(filename, filename_base);
        strcat(filename, index);
        printf("%s\n", filename);
        int fd = open("/mlfs/test_file", O_RDWR);
        if (fd < 0) {
            perror("open");
            return fd;
        }

        start = clock();
        int ret = read(fd, buf, BUFSIZE);
        if (ret < 0) {
            perror("write");
            return ret;
        }
        end = clock();
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

        printf("TIME: %f\n", cpu_time_used);

        memset(filename, 0, 64);
    }

    return 0;
}