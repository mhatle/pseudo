/*
 * Test stat, lstat, fstat, fstatat, stat64, lstat64, fstat64, fstatat64
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
    struct stat st;
    struct stat64 st64;
    int fd, dirfd;

    /* Create test file with known ownership */
    fd = open("test_stat_file", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("chown setup", chown("test_stat_file", 1000, 2000) == 0);
    chmod("test_stat_file", 0755);

    /* Create a symlink */
    check("symlink setup", symlink("test_stat_file", "test_stat_link") == 0);
    check("lchown setup", lchown("test_stat_link", 3000, 4000) == 0);

    /* stat - follows symlinks */
    check("stat", stat("test_stat_file", &st) == 0);
    check("stat uid", st.st_uid == 1000);
    check("stat gid", st.st_gid == 2000);
    check("stat mode", (st.st_mode & 07777) == 0755);
    check("stat is reg", S_ISREG(st.st_mode));

    /* stat through symlink - should follow */
    check("stat symlink", stat("test_stat_link", &st) == 0);
    check("stat symlink uid", st.st_uid == 1000);
    check("stat symlink gid", st.st_gid == 2000);

    /* lstat - should not follow symlink */
    check("lstat link", lstat("test_stat_link", &st) == 0);
    check("lstat is link", S_ISLNK(st.st_mode));
    check("lstat uid", st.st_uid == 3000);
    check("lstat gid", st.st_gid == 4000);

    /* lstat regular file */
    check("lstat file", lstat("test_stat_file", &st) == 0);
    check("lstat file is reg", S_ISREG(st.st_mode));
    check("lstat file uid", st.st_uid == 1000);

    /* fstat */
    fd = open("test_stat_file", O_RDONLY);
    check("fstat", fstat(fd, &st) == 0);
    check("fstat uid", st.st_uid == 1000);
    check("fstat gid", st.st_gid == 2000);
    close(fd);

    /* fstatat without AT_SYMLINK_NOFOLLOW - should follow */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("fstatat follow", fstatat(dirfd, "test_stat_link", &st, 0) == 0);
    check("fstatat follow uid", st.st_uid == 1000);

    /* fstatat with AT_SYMLINK_NOFOLLOW */
    check("fstatat nofollow", fstatat(dirfd, "test_stat_link", &st, AT_SYMLINK_NOFOLLOW) == 0);
    check("fstatat nofollow is link", S_ISLNK(st.st_mode));
    check("fstatat nofollow uid", st.st_uid == 3000);

    /* stat64 */
    check("stat64", stat64("test_stat_file", &st64) == 0);
    check("stat64 uid", st64.st_uid == 1000);
    check("stat64 gid", st64.st_gid == 2000);

    /* lstat64 */
    check("lstat64", lstat64("test_stat_link", &st64) == 0);
    check("lstat64 is link", S_ISLNK(st64.st_mode));
    check("lstat64 uid", st64.st_uid == 3000);

    /* fstat64 */
    fd = open("test_stat_file", O_RDONLY);
    check("fstat64", fstat64(fd, &st64) == 0);
    check("fstat64 uid", st64.st_uid == 1000);
    close(fd);

    /* fstatat64 */
    check("fstatat64", fstatat64(dirfd, "test_stat_link", &st64, AT_SYMLINK_NOFOLLOW) == 0);
    check("fstatat64 is link", S_ISLNK(st64.st_mode));
    check("fstatat64 uid", st64.st_uid == 3000);

    /* stat nonexistent */
    check("stat ENOENT", stat("test_stat_noexist", &st) == -1 && errno == ENOENT);

    close(dirfd);
    unlink("test_stat_link");
    unlink("test_stat_file");

    return failures;
}
