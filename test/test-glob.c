/*
 * Test glob, glob64
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    int fd;
    glob_t globbuf;
    glob64_t globbuf64;
    int ret;

    /* Create test files */
    mkdir("test_glob_dir", 0755);
    fd = open("test_glob_dir/a.txt", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_glob_dir/b.txt", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_glob_dir/c.dat", O_CREAT | O_WRONLY, 0644);
    close(fd);

    /* glob *.txt */
    ret = glob("test_glob_dir/*.txt", 0, NULL, &globbuf);
    check("glob returns 0", ret == 0);
    check("glob found 2 .txt files", globbuf.gl_pathc == 2);
    globfree(&globbuf);

    /* glob *.dat */
    ret = glob("test_glob_dir/*.dat", 0, NULL, &globbuf);
    check("glob dat returns 0", ret == 0);
    check("glob found 1 .dat file", globbuf.gl_pathc == 1);
    globfree(&globbuf);

    /* glob no match */
    ret = glob("test_glob_dir/*.xyz", 0, NULL, &globbuf);
    check("glob no match", ret == GLOB_NOMATCH);

    /* glob * (all files) */
    ret = glob("test_glob_dir/*", 0, NULL, &globbuf);
    check("glob all returns 0", ret == 0);
    check("glob found 3 files", globbuf.gl_pathc == 3);
    globfree(&globbuf);

    /* glob64 */
    ret = glob64("test_glob_dir/*.txt", 0, NULL, &globbuf64);
    check("glob64 returns 0", ret == 0);
    check("glob64 found 2 files", globbuf64.gl_pathc == 2);
    globfree64(&globbuf64);

    /* Cleanup */
    unlink("test_glob_dir/a.txt");
    unlink("test_glob_dir/b.txt");
    unlink("test_glob_dir/c.dat");
    rmdir("test_glob_dir");

    return failures;
}
