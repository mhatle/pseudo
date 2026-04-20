#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test stat, lstat, fstat, fstatat and 64-bit variants

rm -f test_stat_file test_stat_link
./test/test-stat
