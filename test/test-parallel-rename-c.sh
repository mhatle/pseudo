#! /bin/bash

# Test parallel rename() and renameat2() from a C program.
# Modeled after test-parallel-rename.sh but using direct syscalls
# instead of the mv utility.

./test/test-parallel-rename-c
rc=$?
if [ "$rc" -ne 0 ]; then
	exit "$rc"
fi
