/*
 * Test bind() with unix domain socket
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    int fd;
    struct sockaddr_un addr;
    struct stat st;

    /* Remove any stale socket */
    unlink("test_bind_sock");

    /* Create unix domain socket */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    check("socket", fd >= 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "test_bind_sock", sizeof(addr.sun_path) - 1);

    check("bind", bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    /* Verify socket file was created */
    check("socket file exists", lstat("test_bind_sock", &st) == 0);
    check("socket file is socket", S_ISSOCK(st.st_mode));
    check("socket owned by root", st.st_uid == 0);

    /* Bind to same path should fail */
    {
        int fd2 = socket(AF_UNIX, SOCK_STREAM, 0);
        check("bind dup fails", bind(fd2, (struct sockaddr *)&addr, sizeof(addr)) == -1);
        check("bind dup EADDRINUSE", errno == EADDRINUSE);
        close(fd2);
    }

    close(fd);
    unlink("test_bind_sock");

    return failures;
}
