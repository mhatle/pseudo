/*
 * Test getcwd, getwd, get_current_dir_name, chdir, fchdir
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
#include <limits.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    char buf[PATH_MAX];
    char *cwd;
    char original[PATH_MAX];
    int fd;

    /* Save original directory */
    cwd = getcwd(original, sizeof(original));
    check("getcwd initial", cwd != NULL);
    check("getcwd returns buf", cwd == original);
    check("getcwd non-empty", strlen(original) > 0);
    check("getcwd starts with /", original[0] == '/');

    /* getcwd with NULL (glibc extension: allocates buffer) */
    cwd = getcwd(NULL, 0);
    check("getcwd NULL alloc", cwd != NULL);
    check("getcwd NULL matches", strcmp(cwd, original) == 0);
    free(cwd);

    /* get_current_dir_name */
    cwd = get_current_dir_name();
    check("get_current_dir_name", cwd != NULL);
    check("get_current_dir_name matches", strcmp(cwd, original) == 0);
    free(cwd);

    /* chdir */
    mkdir("test_chdir_d1", 0755);
    check("chdir", chdir("test_chdir_d1") == 0);
    cwd = getcwd(buf, sizeof(buf));
    check("chdir getcwd", cwd != NULL);
    /* Should end with /test_chdir_d1 */
    {
        char *p = strrchr(cwd, '/');
        check("chdir correct", p && strcmp(p, "/test_chdir_d1") == 0);
    }

    /* chdir back */
    check("chdir back", chdir(original) == 0);
    cwd = getcwd(buf, sizeof(buf));
    check("chdir back correct", strcmp(cwd, original) == 0);

    /* fchdir */
    fd = open("test_chdir_d1", O_RDONLY | O_DIRECTORY);
    check("fchdir fd open", fd >= 0);
    check("fchdir", fchdir(fd) == 0);
    cwd = getcwd(buf, sizeof(buf));
    {
        char *p = strrchr(cwd, '/');
        check("fchdir correct", p && strcmp(p, "/test_chdir_d1") == 0);
    }
    close(fd);

    /* chdir back and cleanup */
    check("chdir back", chdir(original) == 0);
    rmdir("test_chdir_d1");

    return failures;
}
