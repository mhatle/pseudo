/*
 * Test opendir, closedir
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
#include <dirent.h>
#include <errno.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    DIR *dir;
    struct dirent *ent;
    int fd, count;

    /* Create test directory structure */
    mkdir("test_opendir_d", 0755);
    fd = open("test_opendir_d/file1", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_opendir_d/file2", O_CREAT | O_WRONLY, 0644);
    close(fd);

    /* opendir */
    dir = opendir("test_opendir_d");
    check("opendir", dir != NULL);

    /* Read entries */
    count = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            count++;
        }
    }
    check("readdir found 2 entries", count == 2);

    /* closedir */
    check("closedir", closedir(dir) == 0);

    /* opendir nonexistent */
    dir = opendir("test_opendir_noexist");
    check("opendir noexist NULL", dir == NULL);
    check("opendir noexist ENOENT", errno == ENOENT);

    /* Cleanup */
    unlink("test_opendir_d/file1");
    unlink("test_opendir_d/file2");
    rmdir("test_opendir_d");

    return failures;
}
