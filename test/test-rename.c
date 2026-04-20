/*
 * Test rename, renameat (basic operations not covered by test-renameat2)
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
    struct stat st;

    /* Create a file with known ownership */
    fd = open("test_rename_src", O_CREAT | O_WRONLY, 0644);
    check("write", write(fd, "data", 4) == 4);
    close(fd);
    check("chown src", chown("test_rename_src", 100, 200) == 0);

    /* rename */
    check("rename", rename("test_rename_src", "test_rename_dst") == 0);
    check("rename src gone", !file_exists("test_rename_src"));
    check("rename dst exists", file_exists("test_rename_dst"));
    stat("test_rename_dst", &st);
    check("rename preserves uid", st.st_uid == 100);
    check("rename preserves gid", st.st_gid == 200);

    /* rename onto existing file (should replace) */
    fd = open("test_rename_target", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("rename replace", rename("test_rename_dst", "test_rename_target") == 0);
    check("rename dst gone", !file_exists("test_rename_dst"));
    stat("test_rename_target", &st);
    check("rename replace uid", st.st_uid == 100);

    /* rename nonexistent should fail */
    check("rename noexist", rename("test_rename_noexist", "test_rename_foo") == -1);

    /* renameat */
    fd = open("test_renameat_src", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("chown renameat_src", chown("test_renameat_src", 300, 400) == 0);
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("renameat", renameat(dirfd, "test_renameat_src", dirfd, "test_renameat_dst") == 0);
    check("renameat src gone", !file_exists("test_renameat_src"));
    check("renameat dst exists", file_exists("test_renameat_dst"));
    stat("test_renameat_dst", &st);
    check("renameat preserves uid", st.st_uid == 300);

    /* rename directory */
    mkdir("test_rename_dir", 0755);
    fd = open("test_rename_dir/file", O_CREAT | O_WRONLY, 0644);
    close(fd);
    check("rename dir", rename("test_rename_dir", "test_rename_dir2") == 0);
    check("rename dir src gone", !file_exists("test_rename_dir"));
    check("rename dir dst exists", file_exists("test_rename_dir2/file"));

    close(dirfd);
    unlink("test_rename_target");
    unlink("test_renameat_dst");
    unlink("test_rename_dir2/file");
    rmdir("test_rename_dir2");

    return failures;
}
