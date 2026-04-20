/*
 * Test exec family: execv, execve, execvp, execle
 * (execl already tested in test-execl.sh)
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
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

/* Fork and exec, returning child exit status */
static int fork_exec_test(const char *desc __attribute__((unused)), void (*exec_func)(void)) {
    pid_t pid = fork();
    if (pid == 0) {
        exec_func();
        /* If exec returns, it failed */
        _exit(99);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static void test_execv(void) {
    char *argv[] = { "true", NULL };
    execv("/bin/true", argv);
}

static void test_execvp(void) {
    char *argv[] = { "true", NULL };
    execvp("true", argv);
}

static void test_execve(void) {
    char *argv[] = { "true", NULL };
    extern char **environ;
    execve("/bin/true", argv, environ);
}

static void test_execle(void) {
    extern char **environ;
    execle("/bin/true", "true", (char *)NULL, environ);
}

int main(void) {
    int rc;

    rc = fork_exec_test("execv", test_execv);
    check("execv", rc == 0);

    rc = fork_exec_test("execvp", test_execvp);
    check("execvp", rc == 0);

    rc = fork_exec_test("execve", test_execve);
    check("execve", rc == 0);

    rc = fork_exec_test("execle", test_execle);
    check("execle", rc == 0);

    return failures;
}
