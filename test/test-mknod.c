/*
 * Test mknod, mknodat (creating FIFOs via mknod, since device nodes
 * require real root privileges)
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    struct stat st;
    int dirfd;

    /* mknod - create a FIFO */
    check("mknod fifo", mknod("test_mknod_fifo", S_IFIFO | 0644, 0) == 0);
    check("mknod stat", stat("test_mknod_fifo", &st) == 0);
    check("mknod is fifo", S_ISFIFO(st.st_mode));
    check("mknod owned by root", st.st_uid == 0);
    check("mknod mode", (st.st_mode & 07777) == 0644);

    /* mknod - create a character device (pseudo should track it) */
    check("mknod chardev", mknod("test_mknod_dev", S_IFCHR | 0666, makedev(1, 3)) == 0);
    check("mknod dev stat", lstat("test_mknod_dev", &st) == 0);
    check("mknod dev is char", S_ISCHR(st.st_mode));
    check("mknod dev major", major(st.st_rdev) == 1);
    check("mknod dev minor", minor(st.st_rdev) == 3);

    /* mknodat */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("mknodat fifo", mknodat(dirfd, "test_mknodat_fifo", S_IFIFO | 0644, 0) == 0);
    check("mknodat stat", fstatat(dirfd, "test_mknodat_fifo", &st, 0) == 0);
    check("mknodat is fifo", S_ISFIFO(st.st_mode));

    /* mknodat duplicate should fail */
    check("mknodat dup", mknodat(dirfd, "test_mknodat_fifo", S_IFIFO | 0644, 0) == -1);

    close(dirfd);
    unlink("test_mknod_fifo");
    unlink("test_mknod_dev");
    unlink("test_mknodat_fifo");

    return failures;
}
