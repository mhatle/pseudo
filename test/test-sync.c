/*
 * Test fsync, fdatasync, sync
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    int fd;

    fd = open("test_sync_file", O_CREAT | O_WRONLY, 0644);
    check("open", fd >= 0);
    check("write", write(fd, "data", 4) == 4);

    /* fsync */
    check("fsync", fsync(fd) == 0);

    /* fdatasync */
    check("fdatasync", fdatasync(fd) == 0);

    close(fd);

    /* sync - no return value to check, just ensure it doesn't crash */
    sync();

    unlink("test_sync_file");
    return failures;
}
