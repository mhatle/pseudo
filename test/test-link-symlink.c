/*
 * Test link, linkat, symlink, symlinkat, readlink, readlinkat
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
#include <errno.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    struct stat st, st2;
    char buf[PATH_MAX];
    ssize_t len;
    int fd, dirfd;

    /* Create a test file */
    fd = open("test_link_file", O_CREAT | O_WRONLY, 0644);
    check("write", write(fd, "hello", 5) == 5);
    close(fd);
    check("chown", chown("test_link_file", 100, 200) == 0);

    /* symlink */
    check("symlink", symlink("test_link_file", "test_link_sym") == 0);
    check("symlink lstat", lstat("test_link_sym", &st) == 0);
    check("symlink is link", S_ISLNK(st.st_mode));

    /* readlink */
    len = readlink("test_link_sym", buf, sizeof(buf));
    check("readlink len", len > 0);
    buf[len] = '\0';
    check("readlink value", strcmp(buf, "test_link_file") == 0);

    /* symlink target stat should show chown'd values */
    stat("test_link_sym", &st);
    check("symlink target uid", st.st_uid == 100);
    check("symlink target gid", st.st_gid == 200);

    /* link (hard link) */
    check("link", link("test_link_file", "test_link_hard") == 0);
    stat("test_link_hard", &st);
    stat("test_link_file", &st2);
    check("hardlink same inode", st.st_ino == st2.st_ino);
    check("hardlink uid", st.st_uid == 100);
    check("hardlink gid", st.st_gid == 200);

    /* linkat */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("linkat", linkat(dirfd, "test_link_file", dirfd, "test_link_hard2", 0) == 0);
    stat("test_link_hard2", &st);
    check("linkat uid", st.st_uid == 100);

    /* symlinkat */
    check("symlinkat", symlinkat("test_link_file", dirfd, "test_link_sym2") == 0);
    check("symlinkat lstat", fstatat(dirfd, "test_link_sym2", &st, AT_SYMLINK_NOFOLLOW) == 0);
    check("symlinkat is link", S_ISLNK(st.st_mode));

    /* readlinkat */
    len = readlinkat(dirfd, "test_link_sym2", buf, sizeof(buf));
    check("readlinkat len", len > 0);
    buf[len] = '\0';
    check("readlinkat value", strcmp(buf, "test_link_file") == 0);

    close(dirfd);
    unlink("test_link_sym");
    unlink("test_link_sym2");
    unlink("test_link_hard");
    unlink("test_link_hard2");
    unlink("test_link_file");

    return failures;
}
