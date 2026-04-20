/*
 * Test truncate, truncate64
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

static off_t get_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) return -1;
    return st.st_size;
}

int main(void) {
    int fd;

    /* Create a file with some content */
    fd = open("test_trunc_file", O_CREAT | O_WRONLY, 0644);
    check("write", write(fd, "hello world, this is a test", 27) == 27);
    close(fd);

    check("initial size", get_size("test_trunc_file") == 27);

    /* truncate to smaller */
    check("truncate to 10", truncate("test_trunc_file", 10) == 0);
    check("size after truncate", get_size("test_trunc_file") == 10);

    /* truncate to 0 */
    check("truncate to 0", truncate("test_trunc_file", 0) == 0);
    check("size after truncate 0", get_size("test_trunc_file") == 0);

    /* truncate to extend */
    check("truncate extend", truncate("test_trunc_file", 100) == 0);
    check("size after extend", get_size("test_trunc_file") == 100);

    /* truncate64 */
    check("truncate64", truncate64("test_trunc_file", 50) == 0);
    check("size after truncate64", get_size("test_trunc_file") == 50);

    /* Verify ownership is preserved after truncate */
    check("chown", chown("test_trunc_file", 123, 456) == 0);
    check("truncate after chown", truncate("test_trunc_file", 25) == 0);
    {
        struct stat st;
        stat("test_trunc_file", &st);
        check("truncate preserves uid", st.st_uid == 123);
        check("truncate preserves gid", st.st_gid == 456);
    }

    unlink("test_trunc_file");

    return failures;
}
