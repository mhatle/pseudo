/*
 * Test open64, openat64, creat64
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
    int fd;
    struct stat st;
    int dirfd;

    /* open64 create */
    fd = open64("test_open64_file", O_CREAT | O_WRONLY, 0644);
    check("open64 create", fd >= 0);
    check("open64 write", write(fd, "test", 4) == 4);
    close(fd);
    check("open64 file exists", stat("test_open64_file", &st) == 0);
    check("open64 owned by root", st.st_uid == 0);

    /* open64 read */
    fd = open64("test_open64_file", O_RDONLY);
    check("open64 read", fd >= 0);
    {
        char buf[16];
        ssize_t n = read(fd, buf, sizeof(buf));
        check("open64 read data", n == 4);
    }
    close(fd);

    /* openat64 */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    fd = openat64(dirfd, "test_openat64_file", O_CREAT | O_WRONLY, 0644);
    check("openat64 create", fd >= 0);
    close(fd);
    check("openat64 file exists", fstatat(dirfd, "test_openat64_file", &st, 0) == 0);
    check("openat64 owned by root", st.st_uid == 0);

    /* creat64 */
    fd = creat64("test_creat64_file", 0644);
    check("creat64", fd >= 0);
    close(fd);
    check("creat64 file exists", stat("test_creat64_file", &st) == 0);
    check("creat64 owned by root", st.st_uid == 0);

    close(dirfd);
    unlink("test_open64_file");
    unlink("test_openat64_file");
    unlink("test_creat64_file");

    return failures;
}
