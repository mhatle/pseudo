/*
 * Test scandir, scandir64
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

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

/* Filter out . and .. */
static int filter_dots(const struct dirent *d) {
    return strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0;
}

static int filter_dots64(const struct dirent64 *d) {
    return strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0;
}

int main(void) {
    int fd, n;
    struct dirent **namelist;
    struct dirent64 **namelist64;

    /* Create test directory with files */
    mkdir("test_scandir_dir", 0755);
    fd = open("test_scandir_dir/alpha", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_scandir_dir/beta", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_scandir_dir/gamma", O_CREAT | O_WRONLY, 0644);
    close(fd);

    /* scandir */
    n = scandir("test_scandir_dir", &namelist, filter_dots, alphasort);
    check("scandir count", n == 3);
    if (n >= 3) {
        check("scandir sorted 0", strcmp(namelist[0]->d_name, "alpha") == 0);
        check("scandir sorted 1", strcmp(namelist[1]->d_name, "beta") == 0);
        check("scandir sorted 2", strcmp(namelist[2]->d_name, "gamma") == 0);
    }
    for (int i = 0; i < n; i++) free(namelist[i]);
    if (n > 0) free(namelist);

    /* scandir64 */
    n = scandir64("test_scandir_dir", &namelist64, filter_dots64, alphasort64);
    check("scandir64 count", n == 3);
    if (n >= 3) {
        check("scandir64 sorted 0", strcmp(namelist64[0]->d_name, "alpha") == 0);
        check("scandir64 sorted 1", strcmp(namelist64[1]->d_name, "beta") == 0);
        check("scandir64 sorted 2", strcmp(namelist64[2]->d_name, "gamma") == 0);
    }
    for (int i = 0; i < n; i++) free(namelist64[i]);
    if (n > 0) free(namelist64);

    /* Cleanup */
    unlink("test_scandir_dir/alpha");
    unlink("test_scandir_dir/beta");
    unlink("test_scandir_dir/gamma");
    rmdir("test_scandir_dir");

    return failures;
}
