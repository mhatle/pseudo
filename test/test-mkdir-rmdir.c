/*
 * Test mkdir, mkdirat, rmdir
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
    struct stat st;
    int dirfd;

    /* mkdir */
    check("mkdir 0755", mkdir("test_mkdir_d1", 0755) == 0);
    check("mkdir stat", stat("test_mkdir_d1", &st) == 0);
    check("mkdir is dir", S_ISDIR(st.st_mode));
    check("mkdir owned by root", st.st_uid == 0);

    /* mkdir nested */
    check("mkdir nested", mkdir("test_mkdir_d1/sub", 0700) == 0);
    check("mkdir nested stat", stat("test_mkdir_d1/sub", &st) == 0);
    check("mkdir nested is dir", S_ISDIR(st.st_mode));

    /* mkdirat */
    dirfd = open("test_mkdir_d1", O_RDONLY | O_DIRECTORY);
    check("open dirfd", dirfd >= 0);
    check("mkdirat", mkdirat(dirfd, "sub2", 0750) == 0);
    check("mkdirat stat", fstatat(dirfd, "sub2", &st, 0) == 0);
    check("mkdirat is dir", S_ISDIR(st.st_mode));

    /* mkdir duplicate should fail */
    check("mkdir dup EEXIST", mkdir("test_mkdir_d1", 0755) == -1 && errno == EEXIST);

    /* rmdir */
    check("rmdir sub2", rmdir("test_mkdir_d1/sub2") == 0);
    check("rmdir sub2 gone", stat("test_mkdir_d1/sub2", &st) == -1);
    check("rmdir sub", rmdir("test_mkdir_d1/sub") == 0);
    check("rmdir d1", rmdir("test_mkdir_d1") == 0);
    check("rmdir d1 gone", stat("test_mkdir_d1", &st) == -1);

    /* rmdir non-existent should fail */
    check("rmdir noexist", rmdir("test_mkdir_d1") == -1 && errno == ENOENT);

    close(dirfd);
    return failures;
}
