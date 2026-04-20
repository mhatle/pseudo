/*
 * Test fopen, fclose, freopen, fopen64, freopen64
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
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
    FILE *fp;
    struct stat st;
    char buf[128];

    /* fopen write */
    fp = fopen("test_fopen_file", "w");
    check("fopen w", fp != NULL);
    fprintf(fp, "hello world\n");
    fclose(fp);

    /* Verify file was created and owned by root under pseudo */
    check("fopen file exists", stat("test_fopen_file", &st) == 0);
    check("fopen owned by root", st.st_uid == 0);

    /* fopen read */
    fp = fopen("test_fopen_file", "r");
    check("fopen r", fp != NULL);
    check("fopen read", fgets(buf, sizeof(buf), fp) != NULL);
    check("fopen content", strcmp(buf, "hello world\n") == 0);
    fclose(fp);

    /* fopen append */
    fp = fopen("test_fopen_file", "a");
    check("fopen a", fp != NULL);
    fprintf(fp, "appended\n");
    fclose(fp);

    /* fclose */
    fp = fopen("test_fopen_file", "r");
    check("fclose", fclose(fp) == 0);

    /* freopen */
    fp = fopen("test_fopen_file", "r");
    fp = freopen("test_fopen_file2", "w", fp);
    check("freopen", fp != NULL);
    fprintf(fp, "freopen test\n");
    fclose(fp);
    check("freopen created file", stat("test_fopen_file2", &st) == 0);

    /* fopen64 */
    fp = fopen64("test_fopen64_file", "w");
    check("fopen64", fp != NULL);
    fprintf(fp, "fopen64 test\n");
    fclose(fp);
    check("fopen64 file exists", stat("test_fopen64_file", &st) == 0);
    check("fopen64 owned by root", st.st_uid == 0);

    /* freopen64 */
    fp = fopen("test_fopen64_file", "r");
    fp = freopen64("test_fopen64_file2", "w", fp);
    check("freopen64", fp != NULL);
    fprintf(fp, "freopen64 test\n");
    fclose(fp);
    check("freopen64 created file", stat("test_fopen64_file2", &st) == 0);

    unlink("test_fopen_file");
    unlink("test_fopen_file2");
    unlink("test_fopen64_file");
    unlink("test_fopen64_file2");

    return failures;
}
