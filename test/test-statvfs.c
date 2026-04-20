/*
 * Test statvfs, statvfs64
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/statvfs.h>
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

int main(void) {
    struct statvfs vfs;
    struct statvfs64 vfs64;

    /* statvfs on current directory */
    check("statvfs", statvfs(".", &vfs) == 0);
    check("statvfs bsize > 0", vfs.f_bsize > 0);
    check("statvfs frsize > 0", vfs.f_frsize > 0);
    check("statvfs blocks > 0", vfs.f_blocks > 0);

    /* statvfs on / */
    check("statvfs /", statvfs("/", &vfs) == 0);
    check("statvfs / bsize > 0", vfs.f_bsize > 0);

    /* statvfs64 */
    check("statvfs64", statvfs64(".", &vfs64) == 0);
    check("statvfs64 bsize > 0", vfs64.f_bsize > 0);
    check("statvfs64 frsize > 0", vfs64.f_frsize > 0);

    return failures;
}
