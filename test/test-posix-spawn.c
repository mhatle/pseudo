/*
 * Test posix_spawn, posix_spawnp
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char **environ;

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    pid_t pid;
    int status, ret;

    /* posix_spawn */
    {
        char *argv[] = { "true", NULL };
        ret = posix_spawn(&pid, "/bin/true", NULL, NULL, argv, environ);
        check("posix_spawn returns 0", ret == 0);
        if (ret == 0) {
            waitpid(pid, &status, 0);
            check("posix_spawn child exited", WIFEXITED(status));
            check("posix_spawn child rc 0", WEXITSTATUS(status) == 0);
        }
    }

    /* posix_spawnp (searches PATH) */
    {
        char *argv[] = { "true", NULL };
        ret = posix_spawnp(&pid, "true", NULL, NULL, argv, environ);
        check("posix_spawnp returns 0", ret == 0);
        if (ret == 0) {
            waitpid(pid, &status, 0);
            check("posix_spawnp child exited", WIFEXITED(status));
            check("posix_spawnp child rc 0", WEXITSTATUS(status) == 0);
        }
    }

    /* posix_spawn with a command that produces output */
    {
        int pipefd[2];
        check("pipe", pipe(pipefd) == 0);

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipefd[0]);

        char *argv[] = { "echo", "hello_spawn", NULL };
        ret = posix_spawnp(&pid, "echo", &actions, NULL, argv, environ);
        check("posix_spawn with pipe", ret == 0);

        close(pipefd[1]);
        if (ret == 0) {
            char buf[64] = {0};
            check("read pipe", read(pipefd[0], buf, sizeof(buf) - 1) > 0);
            waitpid(pid, &status, 0);
            check("posix_spawn output", buf[0] == 'h');
        }
        close(pipefd[0]);
        posix_spawn_file_actions_destroy(&actions);
    }

    return failures;
}
