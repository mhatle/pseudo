/*
 * Test access, faccessat, eaccess/euidaccess
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
    int fd, dirfd;

    /* Create test files */
    fd = open("test_access_f1", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_access_f2", O_CREAT | O_WRONLY, 0000);
    close(fd);

    /* access - file exists */
    check("access F_OK exists", access("test_access_f1", F_OK) == 0);
    check("access F_OK noexist", access("test_access_noexist", F_OK) == -1);
    check("access R_OK", access("test_access_f1", R_OK) == 0);
    check("access W_OK", access("test_access_f1", W_OK) == 0);

    /* Under pseudo, we're root, so even 0000 files are accessible */
    check("access root R_OK on 0000", access("test_access_f2", R_OK) == 0);
    check("access root W_OK on 0000", access("test_access_f2", W_OK) == 0);

    /* faccessat */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("faccessat F_OK", faccessat(dirfd, "test_access_f1", F_OK, 0) == 0);
    check("faccessat R_OK", faccessat(dirfd, "test_access_f1", R_OK, 0) == 0);
    check("faccessat noexist", faccessat(dirfd, "test_access_noexist", F_OK, 0) == -1);

    /* eaccess */
    check("eaccess F_OK", eaccess("test_access_f1", F_OK) == 0);
    check("eaccess R_OK", eaccess("test_access_f1", R_OK) == 0);
    check("eaccess noexist", eaccess("test_access_noexist", F_OK) == -1);

    close(dirfd);
    unlink("test_access_f1");
    unlink("test_access_f2");

    return failures;
}
