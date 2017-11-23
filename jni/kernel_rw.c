#include <unistd.h>

#include "kernel_rw.h"

ssize_t read_kern(void *writebuf, void *readbuf, size_t count) {
    int pipefd[2];
    ssize_t len;

    pipe(pipefd);

    len = write(pipefd[1], writebuf, count);

    read(pipefd[0], readbuf, count);

    close(pipefd[0]);
    close(pipefd[1]);

    return len;
}

ssize_t write_kern(void *readbuf, void *writebuf, size_t count) {
    int pipefd[2];
    ssize_t len;

    pipe(pipefd);

    write(pipefd[1], writebuf, count);
    len = read(pipefd[0], readbuf, count);

    close(pipefd[0]);
    close(pipefd[1]);

    return len;
}
