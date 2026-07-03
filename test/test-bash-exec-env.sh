#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test for the bash/pseudo environment conflict:
#   https://bugzilla.yoctoproject.org/show_bug.cgi?id=16078
#
# The compiled helper binary (test-bash-exec-env.c) exports its own
# getenv/setenv/unsetenv (simulating bash) and calls maybe_make_export_env()
# before each execve(), exactly reproducing the opkg-build crash pattern.
#
# Detection mechanism:
#   The binary strips all PSEUDO_* vars (except PSEUDO_PREFIX and
#   PSEUDO_LOCALSTATEDIR) from its internal table and environ before
#   starting.  This forces pseudo_setupenv() — called by pseudo's fork
#   wrapper in each child — to ADD those vars back.

valgrind=$(which valgrind)
if [ -z "${valgrind}" ]; then
    # We don't have valgrind, skip the test
    exit 255
fi

# Some older versions of valgrind may not have exit-on-first-error, but
# it should be sufficiently old enough to be generally available.
${valgrind} --tool=memcheck --exit-on-first-error=yes --error-exitcode=1 $(dirname "$0")/test-bash-exec-env || { echo "FAILED: test-bash-exec-env returned $?" ; exit 1; }

exit 0
