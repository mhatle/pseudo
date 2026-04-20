/*
 * Test dup, dup2, close
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

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    int fd, fd2, fd3;
    struct stat st1, st2;

    /* Create a test file */
    fd = open("test_dup_file", O_CREAT | O_WRONLY, 0644);
    check("open", fd >= 0);

    /* dup */
    fd2 = dup(fd);
    check("dup returns valid fd", fd2 >= 0);
    check("dup different fd", fd2 != fd);

    /* Both fds should refer to same file */
    fstat(fd, &st1);
    fstat(fd2, &st2);
    check("dup same inode", st1.st_ino == st2.st_ino);

    /* dup2 */
    fd3 = dup2(fd, 50);
    check("dup2 returns target", fd3 == 50);
    fstat(fd3, &st2);
    check("dup2 same inode", st1.st_ino == st2.st_ino);

    /* dup2 onto existing fd (should close it first) */
    {
        int fd4 = open("test_dup_file2", O_CREAT | O_WRONLY, 0644);
        int fd5 = dup2(fd, fd4);
        check("dup2 onto existing", fd5 == fd4);
        fstat(fd5, &st2);
        check("dup2 replaced", st1.st_ino == st2.st_ino);
        close(fd5);
        unlink("test_dup_file2");
    }

    /* close */
    check("close dup", close(fd2) == 0);
    check("close dup2", close(fd3) == 0);
    check("close original", close(fd) == 0);

    /* close invalid fd should fail */
    check("close invalid", close(9999) == -1 && errno == EBADF);

    unlink("test_dup_file");

    return failures;
}
