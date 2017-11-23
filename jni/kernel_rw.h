#pragma once

#include <unistd.h>

ssize_t read_kern(void *writebuf, void *readbuf, size_t count);
ssize_t write_kern(void *readbuf, void *writebuf, size_t count);
