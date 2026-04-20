/*
 * Test close_range, closefrom
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/close_range.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

static int fd_is_open(int fd) {
    return fcntl(fd, F_GETFD) != -1;
}

int main(void) {
    int fd1, fd2, fd3;

    /* Open some high-numbered fds */
    fd1 = open("/dev/null", O_RDONLY);
    fd2 = open("/dev/null", O_RDONLY);
    fd3 = open("/dev/null", O_RDONLY);

    check("fds opened", fd1 >= 0 && fd2 >= 0 && fd3 >= 0);
    check("fd1 is open", fd_is_open(fd1));
    check("fd2 is open", fd_is_open(fd2));
    check("fd3 is open", fd_is_open(fd3));

    /* close_range - close fd2 and fd3 (assuming they are consecutive) */
    if (fd3 > fd2 && fd2 > fd1) {
        int rc = close_range(fd2, fd3, 0);
        if (rc == -1 && errno == ENOSYS) {
            /* Kernel too old, skip */
            close(fd1);
            close(fd2);
            close(fd3);
            return 0;
        }
        check("close_range", rc == 0);
        check("fd1 still open", fd_is_open(fd1));
        check("fd2 closed", !fd_is_open(fd2));
        check("fd3 closed", !fd_is_open(fd3));
        close(fd1);
    } else {
        close(fd1);
        close(fd2);
        close(fd3);
    }

    return failures;
}
