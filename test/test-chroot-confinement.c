/*
 * Test that various file operations respect emulated chroot boundaries.
 * Each operation is tested with:
 *   1) Absolute paths (e.g. /file)
 *   2) Relative paths from CWD inside chroot (e.g. file, dir/file)
 *   3) Path traversal attempts (e.g. ../../etc/passwd) that must stay confined
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
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <fts.h>
#include <utime.h>
#include <glob.h>
#include <sys/syscall.h>

#if __has_include (<linux/openat2.h>)
# include <linux/openat2.h>
#else
  struct open_how {
	unsigned long long flags;
	unsigned long long mode;
	unsigned long long resolve;
  };
  #ifndef RESOLVE_BENEATH
  #define RESOLVE_BENEATH	0x08
  #endif
  #ifndef RESOLVE_IN_ROOT
  #define RESOLVE_IN_ROOT	0x10
  #endif
#endif

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif

extern int openat2(int dirfd, const char *path,
		   const struct open_how *how, size_t size) __attribute__((weak));

static int do_openat2_func(int dirfd, const char *path,
			   struct open_how *how, size_t size) {
	if (openat2)
		return openat2(dirfd, path, how, size);
	errno = ENOSYS;
	return -1;
}

static int do_renameat2(int olddirfd, const char *oldpath,
			int newdirfd, const char *newpath,
			unsigned int flags) {
#ifdef SYS_renameat2
	return syscall(SYS_renameat2, olddirfd, oldpath, newdirfd, newpath, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(int argc, char *argv[]) {
    int fd;
    struct stat st;
    char buf[PATH_MAX];
    char *result;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <chroot-dir>\n", argv[0]);
        return 2;
    }

    /* Set up files inside the chroot directory before chrooting */
    {
        char path[PATH_MAX];

        snprintf(path, sizeof(path), "%s/ct_file", argv[1]);
        fd = open(path, O_CREAT | O_WRONLY, 0644);
        check("setup: create file", fd >= 0);
        check("setup: write", write(fd, "hello", 5) == 5);
        close(fd);
        check("setup: chown file", chown(path, 100, 200) == 0);

        snprintf(path, sizeof(path), "%s/ct_dir", argv[1]);
        check("setup: mkdir", mkdir(path, 0755) == 0);

        snprintf(path, sizeof(path), "%s/ct_dir/subfile", argv[1]);
        fd = open(path, O_CREAT | O_WRONLY, 0644);
        close(fd);
        check("setup: chown subfile", chown(path, 300, 400) == 0);

        /* Absolute symlink - key chroot test: must resolve within chroot */
        snprintf(path, sizeof(path), "%s/ct_abs_link", argv[1]);
        check("setup: abs symlink", symlink("/ct_file", path) == 0);

        /* Relative symlink */
        snprintf(path, sizeof(path), "%s/ct_rel_link", argv[1]);
        check("setup: rel symlink", symlink("ct_file", path) == 0);
    }

    /* Now chroot */
    if (chroot(argv[1]) != 0) {
        perror("chroot");
        return 1;
    }
    if (chdir("/") != 0) {
        perror("chdir /");
        return 1;
    }

    /*
     * ================================================================
     * STAT - absolute paths
     * ================================================================
     */
    check("stat abs", stat("/ct_file", &st) == 0);
    check("stat abs uid", st.st_uid == 100);
    check("stat abs gid", st.st_gid == 200);
    check("stat abs reg", S_ISREG(st.st_mode));

    check("lstat abs link", lstat("/ct_abs_link", &st) == 0);
    check("lstat abs is link", S_ISLNK(st.st_mode));

    /* stat through absolute symlink must resolve within chroot */
    check("stat thru abs link", stat("/ct_abs_link", &st) == 0);
    check("stat thru abs uid", st.st_uid == 100);

    /* stat through relative symlink */
    check("stat thru rel link", stat("/ct_rel_link", &st) == 0);
    check("stat thru rel uid", st.st_uid == 100);

    check("stat abs dir", stat("/ct_dir", &st) == 0);
    check("stat abs dir isdir", S_ISDIR(st.st_mode));
    check("stat abs subfile", stat("/ct_dir/subfile", &st) == 0);
    check("stat abs subfile uid", st.st_uid == 300);

    /*
     * STAT - relative paths (CWD is /)
     */
    check("stat rel file", stat("ct_file", &st) == 0);
    check("stat rel uid", st.st_uid == 100);
    check("stat rel gid", st.st_gid == 200);

    check("lstat rel link", lstat("ct_abs_link", &st) == 0);
    check("lstat rel is link", S_ISLNK(st.st_mode));

    check("stat rel dir", stat("ct_dir", &st) == 0);
    check("stat rel dir isdir", S_ISDIR(st.st_mode));
    check("stat rel subfile", stat("ct_dir/subfile", &st) == 0);
    check("stat rel subfile uid", st.st_uid == 300);

    /*
     * STAT - path traversal must be confined
     */
    {
        /* /../ct_file should resolve to /ct_file (can't escape root) */
        check("stat traverse /../ct_file", stat("/../ct_file", &st) == 0);
        check("stat traverse uid", st.st_uid == 100);

        /* Multiple ../ should still be confined */
        check("stat traverse /../../ct_file",
              stat("/../../ct_file", &st) == 0);
        check("stat traverse multi uid", st.st_uid == 100);
    }

    /*
     * ================================================================
     * CHMOD/CHOWN - absolute paths
     * ================================================================
     */
    check("chmod abs", chmod("/ct_file", 0700) == 0);
    check("stat chmod abs", stat("/ct_file", &st) == 0);
    check("chmod abs mode", (st.st_mode & 07777) == 0700);

    check("chown abs", chown("/ct_file", 500, 600) == 0);
    check("stat chown abs", stat("/ct_file", &st) == 0);
    check("chown abs uid", st.st_uid == 500);
    check("chown abs gid", st.st_gid == 600);

    check("lchown abs link", lchown("/ct_abs_link", 700, 800) == 0);
    check("lstat lchown abs", lstat("/ct_abs_link", &st) == 0);
    check("lchown abs uid", st.st_uid == 700);
    /* lchown on link must not change target */
    check("lchown target unchanged", stat("/ct_file", &st) == 0);
    check("lchown target uid", st.st_uid == 500);

    /*
     * CHMOD/CHOWN - relative paths
     */
    check("chmod rel", chmod("ct_file", 0755) == 0);
    check("stat chmod rel", stat("ct_file", &st) == 0);
    check("chmod rel mode", (st.st_mode & 07777) == 0755);

    check("chown rel", chown("ct_file", 501, 601) == 0);
    check("stat chown rel", stat("ct_file", &st) == 0);
    check("chown rel uid", st.st_uid == 501);
    check("chown rel gid", st.st_gid == 601);

    check("lchown rel link", lchown("ct_rel_link", 701, 801) == 0);
    check("lstat lchown rel", lstat("ct_rel_link", &st) == 0);
    check("lchown rel uid", st.st_uid == 701);

    /*
     * CHMOD/CHOWN - path traversal
     */
    check("chmod traverse", chmod("/../ct_file", 0711) == 0);
    check("stat chmod traverse", stat("/ct_file", &st) == 0);
    check("chmod traverse mode", (st.st_mode & 07777) == 0711);

    check("chown traverse", chown("/../ct_file", 502, 602) == 0);
    check("stat chown traverse", stat("/ct_file", &st) == 0);
    check("chown traverse uid", st.st_uid == 502);

    /*
     * ================================================================
     * OPEN/READ/WRITE - absolute paths
     * ================================================================
     */
    fd = open("/ct_file", O_RDONLY);
    check("open abs read", fd >= 0);
    if (fd >= 0) {
        char data[16] = {0};
        check("read abs", read(fd, data, sizeof(data)) == 5);
        check("read abs content", memcmp(data, "hello", 5) == 0);
        close(fd);
    }

    fd = open("/ct_new_abs", O_CREAT | O_WRONLY, 0644);
    check("open abs create", fd >= 0);
    close(fd);
    check("chown abs new", chown("/ct_new_abs", 111, 222) == 0);
    check("stat abs new", stat("/ct_new_abs", &st) == 0);
    check("abs new uid", st.st_uid == 111);

    /*
     * OPEN/READ/WRITE - relative paths
     */
    fd = open("ct_file", O_RDONLY);
    check("open rel read", fd >= 0);
    if (fd >= 0) {
        char data[16] = {0};
        check("read rel", read(fd, data, sizeof(data)) == 5);
        check("read rel content", memcmp(data, "hello", 5) == 0);
        close(fd);
    }

    fd = open("ct_new_rel", O_CREAT | O_WRONLY, 0644);
    check("open rel create", fd >= 0);
    close(fd);
    check("chown rel new", chown("ct_new_rel", 112, 223) == 0);
    check("stat rel new", stat("ct_new_rel", &st) == 0);
    check("rel new uid", st.st_uid == 112);

    /*
     * OPEN - path traversal
     */
    fd = open("/../ct_file", O_RDONLY);
    check("open traverse read", fd >= 0);
    if (fd >= 0) {
        char data[16] = {0};
        check("read traverse", read(fd, data, sizeof(data)) == 5);
        close(fd);
    }

    /*
     * ================================================================
     * ACCESS - absolute, relative, traversal
     * ================================================================
     */
    check("access abs exist", access("/ct_file", F_OK) == 0);
    check("access abs read", access("/ct_file", R_OK) == 0);
    check("access abs noexist", access("/ct_noexist", F_OK) == -1);

    check("access rel exist", access("ct_file", F_OK) == 0);
    check("access rel read", access("ct_file", R_OK) == 0);
    check("access rel noexist", access("ct_noexist", F_OK) == -1);

    check("access traverse", access("/../ct_file", F_OK) == 0);

    /*
     * ================================================================
     * READLINK - absolute and relative
     * ================================================================
     */
    {
        ssize_t len;

        len = readlink("/ct_abs_link", buf, sizeof(buf) - 1);
        check("readlink abs", len > 0);
        if (len > 0) {
            buf[len] = '\0';
            check("readlink abs content", strcmp(buf, "/ct_file") == 0);
        }

        len = readlink("ct_abs_link", buf, sizeof(buf) - 1);
        check("readlink rel", len > 0);
        if (len > 0) {
            buf[len] = '\0';
            check("readlink rel content", strcmp(buf, "/ct_file") == 0);
        }
    }

    /*
     * ================================================================
     * LINK - absolute and relative
     * ================================================================
     */
    check("link abs", link("/ct_file", "/ct_hard_abs") == 0);
    check("stat hard abs", stat("/ct_hard_abs", &st) == 0);
    check("hard abs uid", st.st_uid == 502);

    check("link rel", link("ct_file", "ct_hard_rel") == 0);
    check("stat hard rel", stat("ct_hard_rel", &st) == 0);
    check("hard rel uid", st.st_uid == 502);

    /*
     * SYMLINK - absolute and relative
     */
    check("symlink abs target", symlink("/ct_file", "/ct_sym_abs") == 0);
    check("stat sym abs", stat("/ct_sym_abs", &st) == 0);
    check("sym abs uid", st.st_uid == 502);

    check("symlink rel target", symlink("ct_file", "ct_sym_rel") == 0);
    check("stat sym rel", stat("ct_sym_rel", &st) == 0);
    check("sym rel uid", st.st_uid == 502);

    /*
     * ================================================================
     * RENAME - absolute and relative
     * ================================================================
     */
    check("rename abs", rename("/ct_new_abs", "/ct_ren_abs") == 0);
    check("rename abs dst", stat("/ct_ren_abs", &st) == 0);
    check("rename abs uid", st.st_uid == 111);
    check("rename abs src gone", stat("/ct_new_abs", &st) == -1);

    check("rename rel", rename("ct_new_rel", "ct_ren_rel") == 0);
    check("rename rel dst", stat("ct_ren_rel", &st) == 0);
    check("rename rel uid", st.st_uid == 112);
    check("rename rel src gone", stat("ct_new_rel", &st) == -1);

    /*
     * ================================================================
     * MKDIR/RMDIR - absolute, relative, traversal
     * ================================================================
     */
    check("mkdir abs", mkdir("/ct_dir_abs", 0755) == 0);
    check("stat mkdir abs", stat("/ct_dir_abs", &st) == 0);
    check("mkdir abs isdir", S_ISDIR(st.st_mode));
    check("rmdir abs", rmdir("/ct_dir_abs") == 0);
    check("rmdir abs gone", stat("/ct_dir_abs", &st) == -1);

    check("mkdir rel", mkdir("ct_dir_rel", 0755) == 0);
    check("stat mkdir rel", stat("ct_dir_rel", &st) == 0);
    check("mkdir rel isdir", S_ISDIR(st.st_mode));
    check("rmdir rel", rmdir("ct_dir_rel") == 0);
    check("rmdir rel gone", stat("ct_dir_rel", &st) == -1);

    /* mkdir with traversal - should create inside chroot */
    check("mkdir traverse", mkdir("/../ct_dir_trv", 0755) == 0);
    check("stat mkdir traverse", stat("/ct_dir_trv", &st) == 0);
    check("mkdir traverse isdir", S_ISDIR(st.st_mode));
    check("rmdir traverse", rmdir("/ct_dir_trv") == 0);

    /*
     * ================================================================
     * GETCWD/CHDIR - absolute, relative, traversal
     * ================================================================
     */
    result = getcwd(buf, sizeof(buf));
    check("getcwd in chroot", result != NULL);
    if (result) {
        check("getcwd is /", strcmp(buf, "/") == 0);
    }

    /* chdir absolute */
    check("chdir abs", chdir("/ct_dir") == 0);
    result = getcwd(buf, sizeof(buf));
    check("getcwd after abs chdir", result != NULL);
    if (result) {
        check("cwd is /ct_dir", strcmp(buf, "/ct_dir") == 0);
    }
    /* stat relative from subdirectory */
    check("stat rel from subdir", stat("subfile", &st) == 0);
    check("stat rel subdir uid", st.st_uid == 300);

    /* chdir back with relative .. */
    check("chdir rel ..", chdir("..") == 0);
    result = getcwd(buf, sizeof(buf));
    check("getcwd after ..", result != NULL);
    if (result) {
        check("cwd after .. is /", strcmp(buf, "/") == 0);
    }

    /* chdir with traversal - should stay confined */
    check("chdir traverse", chdir("/../ct_dir") == 0);
    result = getcwd(buf, sizeof(buf));
    check("getcwd after traverse", result != NULL);
    if (result) {
        check("cwd traverse is /ct_dir", strcmp(buf, "/ct_dir") == 0);
    }
    check("chdir back", chdir("/") == 0);

    /*
     * ================================================================
     * OPENDIR - absolute and relative
     * ================================================================
     */
    {
        DIR *dir;
        struct dirent *de;
        int found;

        dir = opendir("/ct_dir");
        check("opendir abs", dir != NULL);
        if (dir) {
            found = 0;
            while ((de = readdir(dir)) != NULL) {
                if (strcmp(de->d_name, "subfile") == 0) found = 1;
            }
            check("opendir abs found", found);
            closedir(dir);
        }

        dir = opendir("ct_dir");
        check("opendir rel", dir != NULL);
        if (dir) {
            found = 0;
            while ((de = readdir(dir)) != NULL) {
                if (strcmp(de->d_name, "subfile") == 0) found = 1;
            }
            check("opendir rel found", found);
            closedir(dir);
        }
    }

    /*
     * ================================================================
     * CANONICALIZE - absolute and relative
     * ================================================================
     */
    result = canonicalize_file_name("/ct_abs_link");
    check("canon abs", result != NULL);
    if (result) {
        check("canon abs resolves", strcmp(result, "/ct_file") == 0);
        free(result);
    }

    result = canonicalize_file_name("ct_abs_link");
    check("canon rel", result != NULL);
    if (result) {
        check("canon rel resolves", strcmp(result, "/ct_file") == 0);
        free(result);
    }

    /* canonicalize with traversal */
    result = canonicalize_file_name("/../ct_abs_link");
    check("canon traverse", result != NULL);
    if (result) {
        check("canon traverse resolves", strcmp(result, "/ct_file") == 0);
        free(result);
    }

    /*
     * ================================================================
     * TRUNCATE - absolute and relative
     * ================================================================
     */
    check("truncate abs", truncate("/ct_file", 3) == 0);
    check("stat trunc abs", stat("/ct_file", &st) == 0);
    check("trunc abs size", st.st_size == 3);
    check("trunc abs uid", st.st_uid == 502);

    /* Restore content for relative test */
    fd = open("/ct_file", O_WRONLY);
    if (fd >= 0) { check("rewrite", write(fd, "hello", 5) == 5); close(fd); }

    check("truncate rel", truncate("ct_file", 4) == 0);
    check("stat trunc rel", stat("ct_file", &st) == 0);
    check("trunc rel size", st.st_size == 4);
    check("trunc rel uid", st.st_uid == 502);

    /*
     * ================================================================
     * UTIME - absolute and relative
     * ================================================================
     */
    {
        struct utimbuf ut;

        ut.actime = 1000000;
        ut.modtime = 2000000;
        check("utime abs", utime("/ct_file", &ut) == 0);
        check("stat utime abs", stat("/ct_file", &st) == 0);
        check("utime abs mtime", st.st_mtime == 2000000);

        ut.actime = 3000000;
        ut.modtime = 4000000;
        check("utime rel", utime("ct_file", &ut) == 0);
        check("stat utime rel", stat("ct_file", &st) == 0);
        check("utime rel mtime", st.st_mtime == 4000000);
    }

    /*
     * ================================================================
     * GLOB - absolute and relative
     * ================================================================
     */
    {
        glob_t g;

        check("glob abs", glob("/ct_file", 0, NULL, &g) == 0);
        check("glob abs match", g.gl_pathc == 1);
        globfree(&g);

        check("glob rel", glob("ct_file", 0, NULL, &g) == 0);
        check("glob rel match", g.gl_pathc == 1);
        globfree(&g);
    }

    /*
     * ================================================================
     * FTS - absolute and relative
     * ================================================================
     */
    {
        FTS *ftsp;
        FTSENT *ent;
        int found;

        /* fts with absolute path */
        {
            char *paths[] = { "/ct_dir", NULL };
            ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
            check("fts abs open", ftsp != NULL);
            if (ftsp) {
                found = 0;
                while ((ent = fts_read(ftsp)) != NULL) {
                    if (ent->fts_info == FTS_F &&
                        strcmp(ent->fts_name, "subfile") == 0) {
                        found = 1;
                        check("fts abs uid", ent->fts_statp->st_uid == 300);
                        check("fts abs gid", ent->fts_statp->st_gid == 400);
                    }
                }
                check("fts abs found", found);
                fts_close(ftsp);
            }
        }

        /* fts with relative path */
        {
            char *paths[] = { "ct_dir", NULL };
            ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
            check("fts rel open", ftsp != NULL);
            if (ftsp) {
                found = 0;
                while ((ent = fts_read(ftsp)) != NULL) {
                    if (ent->fts_info == FTS_F &&
                        strcmp(ent->fts_name, "subfile") == 0) {
                        found = 1;
                        check("fts rel uid", ent->fts_statp->st_uid == 300);
                        check("fts rel gid", ent->fts_statp->st_gid == 400);
                    }
                }
                check("fts rel found", found);
                fts_close(ftsp);
            }
        }
    }

    /*
     * ================================================================
     * SCANDIR - absolute and relative
     * ================================================================
     */
    {
        struct dirent **namelist;
        int n, found;

        n = scandir("/ct_dir", &namelist, NULL, alphasort);
        check("scandir abs", n >= 0);
        if (n >= 0) {
            found = 0;
            for (int i = 0; i < n; i++) {
                if (strcmp(namelist[i]->d_name, "subfile") == 0) found = 1;
                free(namelist[i]);
            }
            free(namelist);
            check("scandir abs found", found);
        }

        n = scandir("ct_dir", &namelist, NULL, alphasort);
        check("scandir rel", n >= 0);
        if (n >= 0) {
            found = 0;
            for (int i = 0; i < n; i++) {
                if (strcmp(namelist[i]->d_name, "subfile") == 0) found = 1;
                free(namelist[i]);
            }
            free(namelist);
            check("scandir rel found", found);
        }
    }

    /*
     * ================================================================
     * FSTATAT - absolute dirfd + relative name
     * ================================================================
     */
    {
        int dfd;

        dfd = open("/ct_dir", O_RDONLY | O_DIRECTORY);
        check("fstatat abs dir", dfd >= 0);
        if (dfd >= 0) {
            check("fstatat abs", fstatat(dfd, "subfile", &st, 0) == 0);
            check("fstatat abs uid", st.st_uid == 300);
            close(dfd);
        }

        dfd = open("ct_dir", O_RDONLY | O_DIRECTORY);
        check("fstatat rel dir", dfd >= 0);
        if (dfd >= 0) {
            check("fstatat rel", fstatat(dfd, "subfile", &st, 0) == 0);
            check("fstatat rel uid", st.st_uid == 300);
            close(dfd);
        }
    }

    /*
     * ================================================================
     * MKFIFO - absolute and relative
     * ================================================================
     */
    check("mkfifo abs", mkfifo("/ct_fifo_abs", 0644) == 0);
    check("stat fifo abs", stat("/ct_fifo_abs", &st) == 0);
    check("fifo abs isfifo", S_ISFIFO(st.st_mode));

    check("mkfifo rel", mkfifo("ct_fifo_rel", 0644) == 0);
    check("stat fifo rel", stat("ct_fifo_rel", &st) == 0);
    check("fifo rel isfifo", S_ISFIFO(st.st_mode));

    /*
     * ================================================================
     * UNLINK/REMOVE - absolute and relative
     * ================================================================
     */
    /* Create files to unlink */
    fd = open("/ct_unlink_abs", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("ct_unlink_rel", O_CREAT | O_WRONLY, 0644);
    close(fd);

    check("unlink abs", unlink("/ct_unlink_abs") == 0);
    check("unlink abs gone", stat("/ct_unlink_abs", &st) == -1);

    check("unlink rel", unlink("ct_unlink_rel") == 0);
    check("unlink rel gone", stat("ct_unlink_rel", &st) == -1);

    /*
     * ================================================================
     * OPENAT2 - absolute, relative, RESOLVE_BENEATH, RESOLVE_IN_ROOT
     * ================================================================
     */
    {
        struct open_how how;
        int ofd;

        /* Create a test file for openat2 */
        fd = open("/ct_openat2_file", O_CREAT | O_WRONLY, 0644);
        check("openat2 setup", fd >= 0);
        close(fd);

        /* openat2 with absolute path */
        memset(&how, 0, sizeof(how));
        how.flags = O_RDONLY;
        ofd = do_openat2_func(AT_FDCWD, "/ct_openat2_file", &how, sizeof(how));
        if (ofd == -1 && errno == ENOSYS) {
            /* openat2 not available, skip these tests */
        } else {
            check("openat2 abs", ofd >= 0);
            if (ofd >= 0) close(ofd);

            /* openat2 with relative path */
            ofd = do_openat2_func(AT_FDCWD, "ct_openat2_file", &how, sizeof(how));
            check("openat2 rel", ofd >= 0);
            if (ofd >= 0) close(ofd);

            /* openat2 with dirfd + relative path */
            {
                int dfd = open("/ct_dir", O_RDONLY | O_DIRECTORY);
                check("openat2 dirfd open", dfd >= 0);
                if (dfd >= 0) {
                    ofd = do_openat2_func(dfd, "subfile", &how, sizeof(how));
                    check("openat2 dirfd rel", ofd >= 0);
                    if (ofd >= 0) close(ofd);
                    close(dfd);
                }
            }

            /* RESOLVE_IN_ROOT: absolute path should resolve within dirfd tree.
             * Open /ct_dir as dirfd, then /subfile with RESOLVE_IN_ROOT
             * should look for <dirfd>/subfile.
             */
            {
                int dfd = open("/ct_dir", O_RDONLY | O_DIRECTORY);
                check("openat2 inroot dfd", dfd >= 0);
                if (dfd >= 0) {
                    memset(&how, 0, sizeof(how));
                    how.flags = O_RDONLY;
                    how.resolve = RESOLVE_IN_ROOT;
                    ofd = do_openat2_func(dfd, "/subfile", &how, sizeof(how));
                    check("openat2 RESOLVE_IN_ROOT abs", ofd >= 0);
                    if (ofd >= 0) close(ofd);
                    close(dfd);
                }
            }

            /* RESOLVE_BENEATH: relative path within dirfd should succeed */
            {
                int dfd = open("/ct_dir", O_RDONLY | O_DIRECTORY);
                check("openat2 beneath dfd", dfd >= 0);
                if (dfd >= 0) {
                    memset(&how, 0, sizeof(how));
                    how.flags = O_RDONLY;
                    how.resolve = RESOLVE_BENEATH;
                    ofd = do_openat2_func(dfd, "subfile", &how, sizeof(how));
                    check("openat2 RESOLVE_BENEATH rel", ofd >= 0);
                    if (ofd >= 0) close(ofd);
                    close(dfd);
                }
            }

            /* openat2 with path traversal */
            memset(&how, 0, sizeof(how));
            how.flags = O_RDONLY;
            ofd = do_openat2_func(AT_FDCWD, "/../ct_openat2_file", &how, sizeof(how));
            check("openat2 traverse", ofd >= 0);
            if (ofd >= 0) close(ofd);
        }

        unlink("/ct_openat2_file");
    }

    /*
     * ================================================================
     * RENAMEAT2 - absolute, relative, NOREPLACE, EXCHANGE in chroot
     * ================================================================
     */
    {
        int ret;

        /* Create test files */
        fd = open("/ct_ren2_a", O_CREAT | O_WRONLY, 0644);
        check("ren2 write a", fd >= 0 && write(fd, "aaa", 3) == 3);
        close(fd);
        check("ren2 chown a", chown("/ct_ren2_a", 111, 222) == 0);

        fd = open("ct_ren2_b", O_CREAT | O_WRONLY, 0644);
        check("ren2 write b", fd >= 0 && write(fd, "bbb", 3) == 3);
        close(fd);
        check("ren2 chown b", chown("ct_ren2_b", 333, 444) == 0);

        /* renameat2 flags=0 with absolute paths */
        fd = open("/ct_ren2_src", O_CREAT | O_WRONLY, 0644);
        close(fd);
        check("ren2 chown src", chown("/ct_ren2_src", 500, 600) == 0);

        ret = do_renameat2(AT_FDCWD, "/ct_ren2_src", AT_FDCWD, "/ct_ren2_dst", 0);
        if (ret == -1 && errno == ENOSYS) {
            /* renameat2 not available, skip */
        } else {
            check("renameat2 abs", ret == 0);
            check("renameat2 abs src gone", stat("/ct_ren2_src", &st) == -1);
            check("renameat2 abs dst exists", stat("/ct_ren2_dst", &st) == 0);
            check("renameat2 abs uid", st.st_uid == 500);
            unlink("/ct_ren2_dst");

            /* renameat2 flags=0 with relative paths */
            fd = open("ct_ren2_rsrc", O_CREAT | O_WRONLY, 0644);
            close(fd);
            check("ren2 chown rsrc", chown("ct_ren2_rsrc", 501, 601) == 0);

            ret = do_renameat2(AT_FDCWD, "ct_ren2_rsrc", AT_FDCWD, "ct_ren2_rdst", 0);
            check("renameat2 rel", ret == 0);
            check("renameat2 rel src gone", stat("ct_ren2_rsrc", &st) == -1);
            check("renameat2 rel dst exists", stat("ct_ren2_rdst", &st) == 0);
            check("renameat2 rel uid", st.st_uid == 501);
            unlink("ct_ren2_rdst");

            /* RENAME_NOREPLACE: must fail when target exists */
            ret = do_renameat2(AT_FDCWD, "/ct_ren2_a", AT_FDCWD, "/ct_ren2_b",
                               RENAME_NOREPLACE);
            check("ren2 NOREPLACE fail", ret == -1 && errno == EEXIST);
            /* Both files must still exist */
            check("ren2 NOREPLACE a exists", stat("/ct_ren2_a", &st) == 0);
            check("ren2 NOREPLACE b exists", stat("/ct_ren2_b", &st) == 0);

            /* RENAME_NOREPLACE: succeed when target does not exist */
            fd = open("/ct_ren2_nrsrc", O_CREAT | O_WRONLY, 0644);
            close(fd);
            check("ren2 chown nrsrc", chown("/ct_ren2_nrsrc", 700, 800) == 0);
            ret = do_renameat2(AT_FDCWD, "/ct_ren2_nrsrc",
                               AT_FDCWD, "/ct_ren2_nrdst", RENAME_NOREPLACE);
            check("ren2 NOREPLACE ok", ret == 0);
            check("ren2 NOREPLACE uid", stat("/ct_ren2_nrdst", &st) == 0 && st.st_uid == 700);
            unlink("/ct_ren2_nrdst");

            /* RENAME_EXCHANGE: atomically swap two paths */
            {
                ino_t ino_a, ino_b;
                check("stat a", stat("/ct_ren2_a", &st) == 0);
                ino_a = st.st_ino;
                check("stat b", stat("/ct_ren2_b", &st) == 0);
                ino_b = st.st_ino;

                ret = do_renameat2(AT_FDCWD, "/ct_ren2_a",
                                   AT_FDCWD, "/ct_ren2_b", RENAME_EXCHANGE);
                check("ren2 EXCHANGE", ret == 0);

                check("stat a after", stat("/ct_ren2_a", &st) == 0);
                check("ren2 EXCHANGE a->b", st.st_ino == ino_b);
                check("stat b after", stat("/ct_ren2_b", &st) == 0);
                check("ren2 EXCHANGE b->a", st.st_ino == ino_a);

                /* Verify ownership swapped too */
                check("ren2 EXCHANGE a uid", st.st_uid == 111);
                stat("/ct_ren2_a", &st);
                check("ren2 EXCHANGE b uid", st.st_uid == 333);
            }

            /* renameat2 with path traversal */
            fd = open("/../ct_ren2_trv", O_CREAT | O_WRONLY, 0644);
            close(fd);
            ret = do_renameat2(AT_FDCWD, "/../ct_ren2_trv",
                               AT_FDCWD, "/ct_ren2_trv2", 0);
            check("renameat2 traverse", ret == 0);
            check("renameat2 traverse dst", stat("/ct_ren2_trv2", &st) == 0);
            unlink("/ct_ren2_trv2");
        }

        unlink("/ct_ren2_a");
        unlink("/ct_ren2_b");
        unlink("/ct_ren2_src");
        unlink("/ct_ren2_dst");
    }

    /*
     * ================================================================
     * Cleanup inside chroot
     * ================================================================
     */
    unlink("/ct_fifo_abs");
    unlink("/ct_fifo_rel");
    unlink("/ct_sym_abs");
    unlink("/ct_sym_rel");
    unlink("/ct_hard_abs");
    unlink("/ct_hard_rel");
    unlink("/ct_ren_abs");
    unlink("/ct_ren_rel");
    unlink("/ct_abs_link");
    unlink("/ct_rel_link");
    unlink("/ct_dir/subfile");
    rmdir("/ct_dir");
    unlink("/ct_file");

    return failures;
}
