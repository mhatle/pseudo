/*
 * Test popen(), system()
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    int rc;
    FILE *fp;
    char buf[256];
    struct stat st;

    /* system() - basic command */
    rc = system("touch test_system_output");
    check("system returns 0", rc == 0);
    check("system created file", stat("test_system_output", &st) == 0);
    check("system file owned by root", st.st_uid == 0);
    unlink("test_system_output");

    /* system() with NULL (checks if shell is available) */
    rc = system(NULL);
    check("system NULL returns nonzero", rc != 0);

    /* popen() - read mode */
    fp = popen("echo hello_popen", "r");
    check("popen read", fp != NULL);
    if (fp) {
        check("popen read content", fgets(buf, sizeof(buf), fp) != NULL);
        buf[strcspn(buf, "\n")] = 0;
        check("popen read value", strcmp(buf, "hello_popen") == 0);
        check("pclose", pclose(fp) == 0);
    }

    /* popen() - write mode */
    fp = popen("cat > test_popen_output", "w");
    check("popen write", fp != NULL);
    if (fp) {
        fprintf(fp, "written_via_popen\n");
        check("pclose write", pclose(fp) == 0);
    }
    /* Verify the file was created */
    check("popen write created file", stat("test_popen_output", &st) == 0);
    fp = fopen("test_popen_output", "r");
    if (fp) {
        check("fgets", fgets(buf, sizeof(buf), fp) != NULL);
        buf[strcspn(buf, "\n")] = 0;
        check("popen write content", strcmp(buf, "written_via_popen") == 0);
        fclose(fp);
    }
    unlink("test_popen_output");

    return failures;
}
