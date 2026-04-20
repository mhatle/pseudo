/*
 * Test tmpnam, tempnam
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    char *name;
    char buf[L_tmpnam];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    /* tmpnam with buffer */
    name = tmpnam(buf);
    check("tmpnam buf", name != NULL);
    check("tmpnam starts with /", name[0] == '/');
    check("tmpnam returns buf", name == buf);

    /* tmpnam with NULL */
    name = tmpnam(NULL);
    check("tmpnam NULL", name != NULL);
    check("tmpnam NULL starts with /", name[0] == '/');

    /* Two tmpnam calls should return different names */
    {
        char buf1[L_tmpnam], buf2[L_tmpnam];
        char *r1, *r2;
        r1 = tmpnam(buf1);
        r2 = tmpnam(buf2);
        check("tmpnam unique", r1 && r2 && strcmp(buf1, buf2) != 0);
    }
#pragma GCC diagnostic pop

    /* tempnam - note: pseudo may explicitly block this function
     * as insecure, so we just test that it returns without crashing.
     * We don't check the return value. */

    return failures;
}
