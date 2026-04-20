/*
 * Test pathconf
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
    long val;
    int fd;

    fd = open("test_pathconf_file", O_CREAT | O_WRONLY, 0644);
    close(fd);

    /* pathconf - NAME_MAX */
    errno = 0;
    val = pathconf("test_pathconf_file", _PC_NAME_MAX);
    check("pathconf NAME_MAX", val > 0 || errno == 0);

    /* pathconf - PATH_MAX */
    errno = 0;
    val = pathconf(".", _PC_PATH_MAX);
    check("pathconf PATH_MAX", val > 0 || errno == 0);

    /* pathconf - LINK_MAX */
    errno = 0;
    val = pathconf("test_pathconf_file", _PC_LINK_MAX);
    check("pathconf LINK_MAX", val > 0 || errno == 0);

    /* pathconf on nonexistent should fail */
    val = pathconf("test_pathconf_noexist", _PC_NAME_MAX);
    check("pathconf noexist", val == -1);

    unlink("test_pathconf_file");
    return failures;
}
