/*
 * Test mkfifo, mkfifoat
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

    /* mkfifo */
    check("mkfifo", mkfifo("test_mkfifo_f1", 0644) == 0);
    check("mkfifo stat", stat("test_mkfifo_f1", &st) == 0);
    check("mkfifo is fifo", S_ISFIFO(st.st_mode));
    check("mkfifo mode", (st.st_mode & 07777) == 0644);
    check("mkfifo owned by root", st.st_uid == 0);

    /* mkfifo duplicate should fail */
    check("mkfifo dup EEXIST", mkfifo("test_mkfifo_f1", 0644) == -1 && errno == EEXIST);

    /* mkfifoat */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("mkfifoat", mkfifoat(dirfd, "test_mkfifo_f2", 0600) == 0);
    check("mkfifoat stat", fstatat(dirfd, "test_mkfifo_f2", &st, 0) == 0);
    check("mkfifoat is fifo", S_ISFIFO(st.st_mode));
    check("mkfifoat mode", (st.st_mode & 07777) == 0600);

    close(dirfd);
    unlink("test_mkfifo_f1");
    unlink("test_mkfifo_f2");

    return failures;
}
