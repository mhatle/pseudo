/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * test-bash-exec-env.c
 *
 * Reproducer for https://bugzilla.yoctoproject.org/show_bug.cgi?id=16078
 *
 * What this binary simulates
 * --------------------------
 * bash exports getenv(), putenv(), setenv() and unsetenv() as dynamic
 * symbols.  Because the main executable appears first in the dynamic
 * linker search order, every library loaded into the process (including
 * libpseudo.so) resolves calls to those names to bash's own
 * implementations.
 *
 * bash maintains a private variable table (SHELL_VARs) that is separate
 * from the C-library 'environ' array.  bash's setenv/getenv operate on
 * that table; 'environ' is only rebuilt lazily by
 * maybe_make_export_env().
 *
 * Rather than approximate bash's behaviour with a hand-written stand-in,
 * this reproducer compiles bash's *actual* environment overrides -
 * bash-5.1's lib/sh/getenv.c - verbatim (see test/bash-getenv.c) and
 * links them against a minimal reimplementation of bash's variable
 * subsystem (find_variable/bind_variable/unbind_variable/
 * maybe_make_export_env, backed by a simple SHELL_VAR table) provided
 * below.  The getenv/putenv/setenv/unsetenv the process exports are
 * therefore byte-for-byte bash's own.
 *
 * The old pseudo workaround (commit 80c6334a) used
 *   pseudo_real_setenv = dlsym(RTLD_NEXT, "setenv")
 * to bypass bash's override and call glibc's setenv directly.
 * dlsym(RTLD_NEXT, ...) from within an LD_PRELOAD library searches the
 * shared-library chain *after* the LD_PRELOAD library, so it finds
 * glibc's implementation - it does NOT find the main executable's
 * (bash's) version.
 *
 * glibc's setenv modifies 'environ' directly, allocating a new heap
 * block (block A) for the updated "LD_PRELOAD=libpseudo.so:..." entry.
 * bash's internal variable table is NOT updated.
 *
 * When bash later calls maybe_make_export_env() (before exec-ing each
 * pipeline stage), it calls strvec_flush() which frees every entry in
 * the current 'environ' array - including block A - and rebuilds from
 * the internal table (which still has the original LD_PRELOAD without
 * libpseudo.so).
 *
 * The freed block A therefore remains live in the allocator's free list
 * while pseudo's exec wrapper (pseudo_setupenvp) iterates 'environ'.
 * Whether the access to freed memory produces a crash depends on whether
 * the allocator happens to reuse that block and overwrite its contents
 * before pseudo reads from it.
 *
 * This binary faithfully reproduces the bash conditions:
 *
 *   1.  It links bash's real getenv/putenv/setenv/unsetenv (from
 *       test/bash-getenv.c) which maintain a private SHELL_VAR table,
 *       separate from 'environ'.  The regular setenv/getenv calls in
 *       pseudo therefore go to *bash's* functions; only
 *       dlsym(RTLD_NEXT, ...) from inside libpseudo.so reaches glibc's
 *       implementations.
 *
 *   2.  It implements strvec_flush() and maybe_make_export_env() that
 *       rebuild 'environ' from the SHELL_VAR table, freeing the old
 *       array (which contains glibc-setenv-allocated blocks).
 *
 *   3.  It runs an executable which triggers maybe_make_export_env()
 *       then execve().  This is th sequence trigging the crash.
 *
 * Detection
 * ---------
 * The only deterministic way to detect the issue is to use valgrind.
 * glibc memory allocation checker is unable to find the errors due
 * to when and how they are produced.
 * The reproducer is assisted by main stripping the derived PSEUDO_*
 * variables (PSEUDO_BINDIR, PSEUDO_LIBDIR, ...) from both the
 * SHELL_VAR table and the real 'environ'.  pseudo's fork wrapper then
 * runs pseudo_setupenv() in every child, which re-adds them:
 *
 *   Unfixed pseudo calls glibc's setenv (via dlsym(RTLD_NEXT, ...)),
 *   which writes the variables straight back into the real 'environ'
 *   array - the very environ/allocator desync that corrupts bash's
 *   heap in production.  The child observes the reappearing PSEUDO_*
 *   entries, pseudo tries to readd them triggering valgrind to report
 *   the bug.
 *
 *   Fixed pseudo calls the process's own setenv (bash's), which updates
 *   only the SHELL_VAR table, and the exec/system/popen wrappers operate
 *   on a private pseudo_setupenvp() copy and restore environ afterwards.
 *   The real 'environ' is never mutated, so the PSEUDO_* variables do
 *   not reappear and the child exits cleanly.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bash-getenv-shim.h"

/* ==================================================================
 * Minimal reimplementation of bash's variable subsystem.
 *
 * bash's getenv.c (test/bash-getenv.c) is compiled verbatim and calls
 * these functions.  They provide the same externally observable
 * behaviour as bash: getenv/setenv/unsetenv operate on a private
 * SHELL_VAR table, and 'environ' is only ever rebuilt on demand by
 * maybe_make_export_env().  That is exactly the environ/variable-table
 * split that the pseudo bug depends on.
 * ==================================================================
 */

/* Concrete definition of bash's variable-context head.  getenv.c only
 * tests shell_variables for non-NULL, so a trivial object suffices. */
struct var_context { int initialised; };
static struct var_context shell_variables_storage;
VAR_CONTEXT *shell_variables = NULL;   /* NULL until main() initialises. */

/* The SHELL_VAR table (bash's variable hash table, flattened). */
static SHELL_VAR **var_table = NULL;
static int         var_count = 0;
static int         var_cap   = 0;

/* bash's exported-environment array and its bookkeeping. */
static char **export_env       = NULL;
static int    export_env_size  = 0;
static int    export_env_index = 0;

/* Set whenever the exported environment must be rebuilt. */
int array_needs_making = 1;

static void *xrealloc_or_die(void *p, size_t n)
{
    void *r = realloc(p, n);
    if (!r) {
        perror("realloc");
        exit(1);
    }
    return r;
}

static SHELL_VAR *var_lookup(const char *name)
{
    for (int i = 0; i < var_count; i++)
        if (strcmp(var_table[i]->name, name) == 0)
            return var_table[i];
    return NULL;
}

static SHELL_VAR *make_new_variable(const char *name)
{
    if (var_count + 1 > var_cap) {
        var_cap = var_cap ? var_cap * 2 : 64;
        var_table = xrealloc_or_die(var_table,
                                    (size_t)var_cap * sizeof(SHELL_VAR *));
    }
    SHELL_VAR *v = calloc(1, sizeof(SHELL_VAR));
    if (!v) {
        perror("calloc");
        exit(1);
    }
    v->name = savestring(name);
    v->value = NULL;
    v->attributes = 0;
    var_table[var_count++] = v;
    return v;
}

/* Look up the variable entry named NAME.  Returns the entry or NULL. */
SHELL_VAR *
find_variable(const char *name)
{
    return var_lookup(name);
}

/* We keep no temporary environment, so there is never a tempenv match. */
SHELL_VAR *
find_tempenv_variable(const char *name)
{
    (void)name;
    return NULL;
}

/* Make a shell variable named NAME have value VALUE, creating it if
   necessary.  Marks the exported environment as needing to be remade. */
SHELL_VAR *
bind_variable(const char *name, char *value, int flags)
{
    (void)flags;
    SHELL_VAR *v = var_lookup(name);
    if (v == NULL)
        v = make_new_variable(name);

    FREE(v->value);
    v->value = value ? savestring(value) : NULL;

    VUNSETATTR(v, att_invisible);
    array_needs_making = 1;
    return v;
}

/* Remove the variable named NAME from the table. */
int
unbind_variable(const char *name)
{
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i]->name, name) == 0) {
            FREE(var_table[i]->name);
            FREE(var_table[i]->value);
            free(var_table[i]);
            memmove(&var_table[i], &var_table[i + 1],
                    (size_t)(var_count - i - 1) * sizeof(SHELL_VAR *));
            var_count--;
            array_needs_making = 1;
            return 0;
        }
    }
    return -1;
}

/* Return the offset of the `=' in an assignment string, or 0 if STRING
   is not a valid assignment.  Simplified form of bash's assignment(). */
int
assignment(const char *string, int flags)
{
    (void)flags;
    unsigned char c = string[0];

    if (!(isalpha(c) || c == '_'))
        return 0;

    for (int i = 0; (c = string[i]); i++) {
        if (c == '=')
            return i;
        if (!(isalnum(c) || c == '_'))
            return 0;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * strvec_flush / strvec_resize / mk_env_string - faithful equivalents
 * of bash's helpers in lib/sh/stringvec.c and variables.c.
 *
 * Note (as in bash): strvec_flush() frees every *string* in the array
 * but not the array itself; the array is reused via strvec_resize().
 * ------------------------------------------------------------------
 */
static void strvec_flush(char **array)
{
    if (array == NULL)
        return;
    for (int i = 0; array[i]; i++)
        free(array[i]);
}

static char **strvec_resize(char **array, int nsize)
{
    return (char **)xrealloc_or_die(array, (size_t)nsize * sizeof(char *));
}

static char *mk_env_string(const char *name, const char *value)
{
    size_t nlen = strlen(name);
    size_t vlen = value ? strlen(value) : 0;
    char *p = malloc(nlen + 1 + vlen + 1);
    if (!p) {
        perror("malloc");
        exit(1);
    }
    memcpy(p, name, nlen);
    p[nlen] = '=';
    if (vlen)
        memcpy(p + nlen + 1, value, vlen);
    p[nlen + 1 + vlen] = '\0';
    return p;
}

/* ------------------------------------------------------------------
 * maybe_make_export_env - rebuild 'environ' from the SHELL_VAR table.
 *
 * This is the function that triggers the use-after-free:
 *
 *   1. pseudo's fork wrapper called glibc's setenv (via
 *      dlsym(RTLD_NEXT)) which allocated block A and stored it in
 *      environ[LD_PRELOAD_idx].
 *
 *   2. strvec_flush(export_env) frees block A.
 *
 *   3. environ is updated to point to the freshly built array
 *      (built from the SHELL_VAR table, which has the original
 *      LD_PRELOAD without libpseudo.so).
 *
 *   4. When the exec wrapper runs pseudo_setupenvp(environ), the
 *      allocator may have already reused block A, causing the freed
 *      memory to be read with stale or garbage content.
 *
 * The logic mirrors bash's maybe_make_export_env(): only rebuild when
 * array_needs_making is set, flush the old strings, resize the export
 * array, and repopulate it from the exported, visible variables.
 * ------------------------------------------------------------------
 */
void
maybe_make_export_env(void)
{
    if (array_needs_making == 0)
        return;

    if (export_env)
        strvec_flush(export_env);

    int new_size = var_count + 1;
    if (new_size > export_env_size) {
        export_env_size = new_size;
        export_env = strvec_resize(export_env, export_env_size);
        environ = export_env;
    }

    export_env_index = 0;
    export_env[0] = NULL;

    for (int i = 0; i < var_count; i++) {
        SHELL_VAR *v = var_table[i];
        if (exported_p(v) && (v->attributes & att_invisible) == 0) {
            export_env[export_env_index++] =
                mk_env_string(v->name, value_cell(v));
        }
    }
    export_env[export_env_index] = NULL;

    environ = export_env;
    array_needs_making = 0;
}

/* ==================================================================
 * bash's actual getenv/putenv/setenv/unsetenv, compiled verbatim.
 * They call the variable-subsystem functions defined above.
 * ==================================================================
 */
#include "bash-getenv.c"

/* ------------------------------------------------------------------
 * is_kept - return 1 if an env entry should NOT be stripped by main().
 * Keep only non-PSEUDO, except for LD_PRELOAD which is also dropped.
 * ------------------------------------------------------------------
 */
static int is_kept(const char *entry)
{
    if (strncmp(entry, "PSEUDO_", 7) != 0 &&
        strncmp(entry, "LD_PRELOAD=", 11) != 0)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------
 * seed_variable_table - populate the SHELL_VAR table from the initial
 * 'environ', mirroring bash importing the inherited environment during
 * shell startup.
 *
 * Each imported variable is marked exported (as bash marks variables
 * that came from the environment) so that bash's getenv() - which only
 * returns exported variables once shell_variables is set - can see them.
 *
 * While importing, the derived PSEUDO_* variables are dropped.
 *
 * When the test runs inside the pseudo test framework, the outer
 * pseudo's exec wrapper pre-populates all PSEUDO_* vars
 * (PSEUDO_BINDIR etc.) in the binary's initial environ via
 * pseudo_setupenvp().  If these are already present,
 * pseudo_setupenv() treats them as existing (overwrite=0) and
 * makes no change - no __add_to_environ() call, no heap-corruption
 * opportunity, or invalid memory reads occur.
 *
 * By stripping them we force pseudo_setupenv() (in the fork wrapper
 * of every child) to ADD them as NEW environ entries.  The unfixed
 * code (glibc setenv via dlsym RTLD_NEXT) calls __add_to_environ()
 * which calls realloc(environ_array).  strvec_flush() later frees
 * that allocation triggering the issue.
 * The fixed code calls bash's setenv() which updates the SHELL_VAR
 * table only - no __add_to_environ(), no realloc, no corruption.
 * ------------------------------------------------------------------
 */
static void seed_variable_table(void)
{
    for (int i = 0; environ && environ[i]; i++) {
        const char *entry = environ[i];
        const char *eq = strchr(entry, '=');
        if (eq == NULL)
            continue;   /* malformed, skip */

        size_t nlen = (size_t)(eq - entry);
        char *name = malloc(nlen + 1);
        if (!name) {
            perror("malloc");
            exit(1);
        }
        memcpy(name, entry, nlen);
        name[nlen] = '\0';
        const char *value = eq + 1;

        if (is_kept(entry)) {
            SHELL_VAR *v = make_new_variable(name);
            FREE(v->value);
            v->value = savestring(value);
            VSETATTR(v, att_exported);
        }
        /* else: derived PSEUDO_* var - drop it. */

        free(name);
    }
}

int main(void)
{
    /*
     * Import the inherited environment into bash's SHELL_VAR table,
     * dropping the derived PSEUDO_* vars (see seed_variable_table()).
     */
    seed_variable_table();

    /*
     * From here on, bash's getenv() must consult the SHELL_VAR table
     * rather than the pre-initialisation 'environ' fallback, so mark
     * the shell variables as initialised (shell_variables != NULL).
     */
    shell_variables_storage.initialised = 1;
    shell_variables = &shell_variables_storage;

    /*
     * Build the exported environment from the SHELL_VAR table and point
     * 'environ' at it.  This mirrors bash constructing its own
     * heap-allocated export_env during shell initialisation: the
     * kernel-provided environ (static memory) is left untouched, and
     * every subsequent maybe_make_export_env() manages only the
     * heap-allocated export_env.  After this call 'environ' is free of
     * the stripped PSEUDO_* variables.
     */
    array_needs_making = 1;
    maybe_make_export_env();

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        /*
         * Child: pseudo's fork wrapper has already run
         * pseudo_setupenv() at this point.
         *
         * Otherwise behave like bash: rebuild environ from the
         * SHELL_VAR table and exec the pipeline stage.
         */
        maybe_make_export_env();
        char *const args[] = { "/bin/true", NULL };
        execve(args[0], args, environ);
        _exit(127);
    }

    while(1) {
        int status;

        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }
        if (WEXITSTATUS(status) == 0)
            break;
        else
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Fork/exec failed: status=%d\n", status);
                return -1;
            }
    }

    return 0;
}
