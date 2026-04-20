/*
 * Test chmod, fchmod, chown, fchown, lchown, fchmodat, fchownat
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

static mode_t get_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror(path);
        return 0;
    }
    return st.st_mode & 07777;
}

static mode_t get_mode_fd(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return 0;
    }
    return st.st_mode & 07777;
}

static uid_t get_uid(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) return (uid_t)-1;
    return st.st_uid;
}

static gid_t get_gid(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) return (gid_t)-1;
    return st.st_gid;
}

int main(void) {
    int fd;

    /* chmod */
    fd = open("test_chmod_file", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("chmod 0755", chmod("test_chmod_file", 0755) == 0);
    check("chmod result", get_mode("test_chmod_file") == 0755);
    check("chmod 0400", chmod("test_chmod_file", 0400) == 0);
    check("chmod result", get_mode("test_chmod_file") == 0400);

    /* fchmod */
    fd = open("test_chmod_file", O_RDONLY);
    check("fchmod 0666", fchmod(fd, 0666) == 0);
    check("fchmod result", get_mode_fd(fd) == 0666);
    close(fd);
    check("fchmod persisted", get_mode("test_chmod_file") == 0666);

    /* chown */
    check("chown 100:200", chown("test_chmod_file", 100, 200) == 0);
    check("chown uid", get_uid("test_chmod_file") == 100);
    check("chown gid", get_gid("test_chmod_file") == 200);

    /* fchown */
    fd = open("test_chmod_file", O_RDONLY);
    check("fchown 300:400", fchown(fd, 300, 400) == 0);
    close(fd);
    check("fchown uid", get_uid("test_chmod_file") == 300);
    check("fchown gid", get_gid("test_chmod_file") == 400);

    /* lchown on a symlink */
    check("symlink for lchown", symlink("test_chmod_file", "test_chmod_link") == 0);
    check("lchown 500:600", lchown("test_chmod_link", 500, 600) == 0);
    {
        struct stat st;
        lstat("test_chmod_link", &st);
        check("lchown uid on symlink", st.st_uid == 500);
        check("lchown gid on symlink", st.st_gid == 600);
    }
    /* target should be unchanged */
    check("lchown didnt change target uid", get_uid("test_chmod_file") == 300);

    /* fchmodat */
    int dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("fchmodat 0700", fchmodat(dirfd, "test_chmod_file", 0700, 0) == 0);
    check("fchmodat result", get_mode("test_chmod_file") == 0700);

    /* fchownat */
    check("fchownat 700:800", fchownat(dirfd, "test_chmod_file", 700, 800, 0) == 0);
    check("fchownat uid", get_uid("test_chmod_file") == 700);
    check("fchownat gid", get_gid("test_chmod_file") == 800);

    /* fchownat with AT_SYMLINK_NOFOLLOW */
    check("fchownat nofollow", fchownat(dirfd, "test_chmod_link", 900, 1000, AT_SYMLINK_NOFOLLOW) == 0);
    {
        struct stat st;
        lstat("test_chmod_link", &st);
        check("fchownat nofollow uid", st.st_uid == 900);
        check("fchownat nofollow gid", st.st_gid == 1000);
    }

    close(dirfd);
    unlink("test_chmod_link");
    unlink("test_chmod_file");

    return failures;
}
