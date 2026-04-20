/*
 * Test utime, utimes, lutimes
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>

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
    struct utimbuf ut;
    struct timeval tv[2];

    /* Create test file */
    fd = open("test_utime_file", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("symlink", symlink("test_utime_file", "test_utime_link") == 0);

    /* utime - set specific access and modification times */
    ut.actime = 1000000;
    ut.modtime = 2000000;
    check("utime", utime("test_utime_file", &ut) == 0);
    stat("test_utime_file", &st);
    check("utime atime", st.st_atime == 1000000);
    check("utime mtime", st.st_mtime == 2000000);

    /* utime with NULL (set to current time) */
    check("utime NULL", utime("test_utime_file", NULL) == 0);
    stat("test_utime_file", &st);
    check("utime NULL changed", st.st_mtime != 2000000);

    /* utimes */
    tv[0].tv_sec = 3000000;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = 4000000;
    tv[1].tv_usec = 0;
    check("utimes", utimes("test_utime_file", tv) == 0);
    stat("test_utime_file", &st);
    check("utimes atime", st.st_atime == 3000000);
    check("utimes mtime", st.st_mtime == 4000000);

    /* lutimes - should modify the symlink, not the target */
    tv[0].tv_sec = 5000000;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = 6000000;
    tv[1].tv_usec = 0;
    check("lutimes", lutimes("test_utime_link", tv) == 0);
    lstat("test_utime_link", &st);
    check("lutimes mtime", st.st_mtime == 6000000);

    /* target should still have old times */
    stat("test_utime_file", &st);
    check("lutimes target unchanged", st.st_mtime == 4000000);

    unlink("test_utime_link");
    unlink("test_utime_file");

    return failures;
}
