#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif

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

static int touch_file(const char *path) {
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	if (write(fd, "x", 1) != 1) {
		perror("write");
		close(fd);
		return -1;
	}
	if (close(fd) == -1) {
		perror("close");
		return -1;
	}
	return 0;
}

/*
 * Mode 0: use rename()
 * Mode 1: use renameat2() with flags=0
 */
static int rename_loop(const char *tmpdir, int worker_id, int mode,
		       const char *mode_name) {
	char src[PATH_MAX];
	char dst[PATH_MAX];
	int run;

	for (run = 0; run < 10; run++) {
		snprintf(src, sizeof(src), "%s/file.%s.%d.%d",
			 tmpdir, mode_name, worker_id, run);
		snprintf(dst, sizeof(dst), "%s/final.%s",
			 tmpdir, mode_name);

		if (touch_file(src) == -1) {
			fprintf(stderr, "FAILED: touch %s\n", src);
			return 1;
		}

		if (mode == 0) {
			if (rename(src, dst) == -1) {
				fprintf(stderr, "FAILED: rename(%s, %s): %s\n",
					src, dst, strerror(errno));
				return 1;
			}
		} else {
			if (do_renameat2(AT_FDCWD, src, AT_FDCWD, dst, 0) == -1) {
				fprintf(stderr, "FAILED: renameat2(%s, %s, 0): %s\n",
					src, dst, strerror(errno));
				return 1;
			}
		}
	}
	return 0;
}

/*
 * Test that rename to a non-existent directory fails with ENOENT.
 * Mode 0: rename(), Mode 1: renameat2()
 */
static int test_rename_invalid_dest(const char *tmpdir, int mode,
				    const char *mode_name) {
	char src[PATH_MAX];
	char dst[PATH_MAX];
	int ret;

	snprintf(src, sizeof(src), "%s/src_invalid_%s", tmpdir, mode_name);
	snprintf(dst, sizeof(dst), "%s/no_such_dir/dest_%s", tmpdir, mode_name);

	if (touch_file(src) == -1) {
		fprintf(stderr, "FAILED: touch %s\n", src);
		return 1;
	}

	if (mode == 0) {
		ret = rename(src, dst);
	} else {
		ret = do_renameat2(AT_FDCWD, src, AT_FDCWD, dst, 0);
	}

	if (ret == 0) {
		fprintf(stderr, "FAILED: %s to invalid dir succeeded unexpectedly\n",
			mode_name);
		return 1;
	}
	if (errno != ENOENT) {
		fprintf(stderr, "FAILED: %s to invalid dir: expected ENOENT, got %s\n",
			mode_name, strerror(errno));
		return 1;
	}

	/* Source file should still exist after failed rename */
	struct stat st;
	if (stat(src, &st) == -1) {
		fprintf(stderr, "FAILED: %s source disappeared after failed rename\n",
			mode_name);
		return 1;
	}

	unlink(src);
	return 0;
}

/*
 * Test that renameat2 RENAME_NOREPLACE to a non-existent directory fails.
 */
static int test_renameat2_noreplace_invalid_dest(const char *tmpdir) {
	char src[PATH_MAX];
	char dst[PATH_MAX];
	int ret;

	snprintf(src, sizeof(src), "%s/src_noreplace_invalid", tmpdir);
	snprintf(dst, sizeof(dst), "%s/no_such_dir/dst_noreplace_invalid", tmpdir);

	if (touch_file(src) == -1) {
		fprintf(stderr, "FAILED: touch %s\n", src);
		return 1;
	}

	ret = do_renameat2(AT_FDCWD, src, AT_FDCWD, dst, RENAME_NOREPLACE);
	if (ret == 0) {
		fprintf(stderr, "FAILED: renameat2 NOREPLACE to invalid dir succeeded\n");
		return 1;
	}
	if (errno != ENOENT) {
		fprintf(stderr, "FAILED: renameat2 NOREPLACE to invalid dir: "
			"expected ENOENT, got %s\n", strerror(errno));
		return 1;
	}

	struct stat st;
	if (stat(src, &st) == -1) {
		fprintf(stderr, "FAILED: source disappeared after failed renameat2 NOREPLACE\n");
		return 1;
	}

	unlink(src);
	return 0;
}

/*
 * Fork NUM_WORKERS children, each running rename_loop in parallel.
 * Returns 0 on success, 1 on any failure.
 */
#define NUM_WORKERS 4

static int run_parallel(const char *tmpdir, int mode, const char *mode_name) {
	pid_t pids[NUM_WORKERS];
	int i, status;
	int failed = 0;

	for (i = 0; i < NUM_WORKERS; i++) {
		pids[i] = fork();
		if (pids[i] == -1) {
			perror("fork");
			return 1;
		}
		if (pids[i] == 0) {
			/* child */
			_exit(rename_loop(tmpdir, i, mode, mode_name));
		}
	}

	for (i = 0; i < NUM_WORKERS; i++) {
		if (waitpid(pids[i], &status, 0) == -1) {
			perror("waitpid");
			failed = 1;
			continue;
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fprintf(stderr, "FAILED: %s worker %d exited with status %d\n",
				mode_name, i,
				WIFEXITED(status) ? WEXITSTATUS(status) : -1);
			failed = 1;
		}
	}

	return failed;
}

int main(void) {
	char template[] = "test-parallel-rename-c.XXXXXX";
	char *tmpdir;
	int rc = 0;
	int renameat2_works = 1;

	tmpdir = mkdtemp(template);
	if (!tmpdir) {
		perror("mkdtemp");
		return 1;
	}

	/* ------------------------------------------------------- */
	/* Part 1: Parallel rename() calls                         */
	/* ------------------------------------------------------- */
	fprintf(stderr, "Testing parallel rename()...\n");
	if (run_parallel(tmpdir, 0, "rename")) {
		fprintf(stderr, "FAILED: parallel rename()\n");
		rc = 1;
		goto cleanup;
	}

	/* ------------------------------------------------------- */
	/* Part 2: Parallel renameat2() calls                      */
	/* ------------------------------------------------------- */
	/* Probe whether renameat2 is available */
	{
		char probe_src[PATH_MAX], probe_dst[PATH_MAX];
		int ret;

		snprintf(probe_src, sizeof(probe_src), "%s/probe_src", tmpdir);
		snprintf(probe_dst, sizeof(probe_dst), "%s/probe_dst", tmpdir);

		if (touch_file(probe_src) == -1) {
			rc = 1;
			goto cleanup;
		}

		ret = do_renameat2(AT_FDCWD, probe_src, AT_FDCWD, probe_dst, 0);
		if (ret == -1 && errno == ENOSYS) {
			fprintf(stderr, "renameat2 not available, skipping renameat2 tests\n");
			renameat2_works = 0;
			unlink(probe_src);
		} else if (ret == -1) {
			perror("renameat2 probe");
			rc = 1;
			goto cleanup;
		} else {
			unlink(probe_dst);
		}
	}

	if (renameat2_works) {
		fprintf(stderr, "Testing parallel renameat2()...\n");
		if (run_parallel(tmpdir, 1, "renameat2")) {
			fprintf(stderr, "FAILED: parallel renameat2()\n");
			rc = 1;
			goto cleanup;
		}
	}

	/* ------------------------------------------------------- */
	/* Part 3: rename() to invalid destination directory        */
	/* ------------------------------------------------------- */
	fprintf(stderr, "Testing rename() to invalid destination...\n");
	if (test_rename_invalid_dest(tmpdir, 0, "rename")) {
		rc = 1;
		goto cleanup;
	}

	/* ------------------------------------------------------- */
	/* Part 4: renameat2() to invalid destination directory     */
	/* ------------------------------------------------------- */
	if (renameat2_works) {
		fprintf(stderr, "Testing renameat2() to invalid destination...\n");
		if (test_rename_invalid_dest(tmpdir, 1, "renameat2")) {
			rc = 1;
			goto cleanup;
		}

		/* ------------------------------------------------------- */
		/* Part 5: renameat2 NOREPLACE to invalid dest             */
		/* ------------------------------------------------------- */
		fprintf(stderr, "Testing renameat2 NOREPLACE to invalid destination...\n");
		if (test_renameat2_noreplace_invalid_dest(tmpdir)) {
			rc = 1;
			goto cleanup;
		}
	}

	if (rc == 0)
		fprintf(stderr, "All tests passed.\n");

cleanup:
	/* Best-effort recursive cleanup of temp dir */
	{
		char cmd[PATH_MAX + 16];
		snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpdir);
		if (system(cmd) != 0)
			perror("cleanup");
	}

	return rc;
}
