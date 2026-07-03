/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * bash-getenv-shim.h
 *
 * Minimal reimplementation of the GNU bash variable-subsystem interface
 * that is required to compile bash's own lib/sh/getenv.c (see
 * test/bash-getenv.c) unmodified.
 *
 * bash's getenv.c overrides the C library's getenv(), putenv(), setenv()
 * and unsetenv() so that they operate on bash's private SHELL_VAR table
 * instead of the C-library 'environ' array.  It relies on a handful of
 * declarations and helper macros that normally come from bash's
 * config.h / general.h / xmalloc.h / variables.h / shell.h.
 *
 * This header provides just enough of that interface (the SHELL_VAR
 * struct, the export/invisible attribute bits, and the accessor macros)
 * for getenv.c to build, plus prototypes for the variable-subsystem
 * functions that test-bash-exec-env.c reimplements.
 */

#ifndef BASH_GETENV_SHIM_H
#define BASH_GETENV_SHIM_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------
 * Configuration macros normally supplied by bash's config.h.
 *
 *   CAN_REDEFINE_GETENV : compile the whole of getenv.c.
 *   HAVE_STD_PUTENV     : putenv() takes (char *)     -> matches glibc.
 *   HAVE_STD_UNSETENV   : unsetenv() returns int      -> matches glibc.
 *
 * Getting the two HAVE_STD_* macros right is important: it makes the
 * signatures of the redefined putenv()/unsetenv() match glibc's own
 * prototypes so the compiler does not reject them as conflicting.
 * ------------------------------------------------------------------
 */
#define CAN_REDEFINE_GETENV	1
#define HAVE_UNISTD_H		1
#define HAVE_STD_PUTENV		1
#define HAVE_STD_UNSETENV	1

/* ------------------------------------------------------------------
 * bash general.h / xmalloc.h helper macros used by getenv.c.
 * ------------------------------------------------------------------
 */
#ifndef savestring
#  define savestring(x)	(char *)strcpy ((char *)malloc (1 + strlen (x)), (x))
#endif

#define FREE(s)		do { if (s) free (s); } while (0)

#define STREQN(a, b, n)	((n == 0) ? (1) \
				  : ((a)[0] == (b)[0] && strncmp(a, b, n) == 0))

/* ------------------------------------------------------------------
 * bash variables.h: the SHELL_VAR object and the small subset of the
 * attribute bits / accessor macros that getenv.c touches.
 * ------------------------------------------------------------------
 */
#define att_exported	0x0000001	/* export to environment */
#define att_invisible	0x0001000	/* cannot see */

typedef struct variable {
  char *name;		/* Symbol that the user types.   */
  char *value;		/* Value that is returned.       */
  int   attributes;	/* export, invisible, ...        */
} SHELL_VAR;

#define value_cell(var)		((var)->value)
#define exported_p(var)		((((var)->attributes) & (att_exported)))

#define VSETATTR(var, att)	((var)->attributes |= (att))
#define VUNSETATTR(var, att)	((var)->attributes &= ~(att))

/* Head of bash's variable-context chain.  getenv.c only ever tests it
 * for non-NULL (to decide between the shell-variable path and the
 * pre-initialisation 'environ' fallback), so a forward declaration and
 * an opaque pointer are all that is needed here. */
typedef struct var_context VAR_CONTEXT;
extern VAR_CONTEXT *shell_variables;

/* ------------------------------------------------------------------
 * Variable-subsystem entry points used by getenv.c.  These are
 * reimplemented (backed by a simple SHELL_VAR table) in
 * test-bash-exec-env.c.
 * ------------------------------------------------------------------
 */
extern SHELL_VAR *find_variable (const char *name);
extern SHELL_VAR *find_tempenv_variable (const char *name);
extern SHELL_VAR *bind_variable (const char *name, char *value, int flags);
extern int        unbind_variable (const char *name);
extern int        assignment (const char *string, int flags);

/* Rebuild 'environ' (bash's export_env) from the exported SHELL_VARs. */
extern void       maybe_make_export_env (void);

#endif /* BASH_GETENV_SHIM_H */
