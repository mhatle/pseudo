/*
 * Test canonicalize_file_name
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
    char *result;
    char cwd[PATH_MAX];
    int fd;

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        return 1;
    }

    /* Create a test file and symlink */
    fd = open("test_canon_file", O_CREAT | O_WRONLY, 0644);
    close(fd);
    if (symlink("test_canon_file", "test_canon_link") != 0) {
        perror("symlink");
        return 1;
    }

    /* canonicalize_file_name on regular file */
    result = canonicalize_file_name("test_canon_file");
    check("canon file", result != NULL);
    if (result) {
        check("canon file starts with /", result[0] == '/');
        check("canon file contains name", strstr(result, "test_canon_file") != NULL);
        free(result);
    }

    /* canonicalize_file_name on symlink (should resolve) */
    result = canonicalize_file_name("test_canon_link");
    check("canon link", result != NULL);
    if (result) {
        check("canon link resolves", strstr(result, "test_canon_file") != NULL);
        /* Should NOT contain the link name */
        check("canon link not link", strstr(result, "test_canon_link") == NULL);
        free(result);
    }

    /* canonicalize_file_name on . */
    result = canonicalize_file_name(".");
    check("canon dot", result != NULL);
    if (result) {
        check("canon dot matches cwd", strcmp(result, cwd) == 0);
        free(result);
    }

    /* canonicalize_file_name on nonexistent should return NULL */
    result = canonicalize_file_name("test_canon_noexist");
    check("canon noexist NULL", result == NULL);

    unlink("test_canon_link");
    unlink("test_canon_file");

    return failures;
}
