/*
 * Test mkdtemp, mkstemp, mkostemp, mkstemps, mkostemps, mkstemp64, mkostemp64
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

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    char template[256];
    char *ret;
    int fd;
    struct stat st;

    /* mkdtemp */
    strcpy(template, "test_mktemp_dir_XXXXXX");
    ret = mkdtemp(template);
    check("mkdtemp", ret != NULL);
    check("mkdtemp is dir", stat(template, &st) == 0 && S_ISDIR(st.st_mode));
    check("mkdtemp owned by root", st.st_uid == 0);
    rmdir(template);

    /* mkstemp */
    strcpy(template, "test_mktemp_XXXXXX");
    fd = mkstemp(template);
    check("mkstemp", fd >= 0);
    check("mkstemp file exists", fstat(fd, &st) == 0);
    check("mkstemp is regular", S_ISREG(st.st_mode));
    check("mkstemp owned by root", st.st_uid == 0);
    close(fd);
    unlink(template);

    /* mkostemp */
    strcpy(template, "test_mktemp_XXXXXX");
    fd = mkostemp(template, O_APPEND);
    check("mkostemp", fd >= 0);
    check("mkostemp file exists", fstat(fd, &st) == 0);
    close(fd);
    unlink(template);

    /* mkstemps (with suffix) */
    strcpy(template, "test_mktemp_XXXXXX.txt");
    fd = mkstemps(template, 4); /* 4 = strlen(".txt") */
    check("mkstemps", fd >= 0);
    check("mkstemps has suffix", strlen(template) > 4 && strcmp(template + strlen(template) - 4, ".txt") == 0);
    check("mkstemps file exists", fstat(fd, &st) == 0);
    close(fd);
    unlink(template);

    /* mkostemps */
    strcpy(template, "test_mktemp_XXXXXX.log");
    fd = mkostemps(template, 4, O_APPEND);
    check("mkostemps", fd >= 0);
    check("mkostemps has suffix", strlen(template) > 4 && strcmp(template + strlen(template) - 4, ".log") == 0);
    close(fd);
    unlink(template);

    /* mkstemp64 */
    strcpy(template, "test_mktemp64_XXXXXX");
    fd = mkstemp64(template);
    check("mkstemp64", fd >= 0);
    check("mkstemp64 file exists", fstat(fd, &st) == 0);
    close(fd);
    unlink(template);

    /* mkostemp64 */
    strcpy(template, "test_mktemp64_XXXXXX");
    fd = mkostemp64(template, O_APPEND);
    check("mkostemp64", fd >= 0);
    check("mkostemp64 file exists", fstat(fd, &st) == 0);
    close(fd);
    unlink(template);

    return failures;
}
