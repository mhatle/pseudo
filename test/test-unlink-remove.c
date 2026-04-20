/*
 * Test unlink, unlinkat, remove
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

static int file_exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

int main(void) {
    int fd, dirfd;

    /* unlink a regular file */
    fd = open("test_unlink_f1", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("file created", file_exists("test_unlink_f1"));
    check("unlink", unlink("test_unlink_f1") == 0);
    check("unlink gone", !file_exists("test_unlink_f1"));

    /* unlink a symlink */
    fd = open("test_unlink_target", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("symlink", symlink("test_unlink_target", "test_unlink_sym") == 0);
    check("unlink symlink", unlink("test_unlink_sym") == 0);
    check("symlink gone", !file_exists("test_unlink_sym"));
    check("target still exists", file_exists("test_unlink_target"));
    unlink("test_unlink_target");

    /* unlink nonexistent */
    check("unlink noexist", unlink("test_unlink_noexist") == -1 && errno == ENOENT);

    /* unlinkat */
    fd = open("test_unlinkat_f1", O_CREAT | O_WRONLY, 0644);
    close(fd);
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("unlinkat", unlinkat(dirfd, "test_unlinkat_f1", 0) == 0);
    check("unlinkat gone", !file_exists("test_unlinkat_f1"));

    /* unlinkat with AT_REMOVEDIR */
    mkdir("test_unlinkat_dir", 0755);
    check("unlinkat rmdir", unlinkat(dirfd, "test_unlinkat_dir", AT_REMOVEDIR) == 0);
    check("unlinkat rmdir gone", !file_exists("test_unlinkat_dir"));

    close(dirfd);

    /* remove file */
    fd = open("test_remove_f1", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("remove file", remove("test_remove_f1") == 0);
    check("remove file gone", !file_exists("test_remove_f1"));

    /* remove directory */
    mkdir("test_remove_dir", 0755);
    check("remove dir", remove("test_remove_dir") == 0);
    check("remove dir gone", !file_exists("test_remove_dir"));

    return failures;
}
