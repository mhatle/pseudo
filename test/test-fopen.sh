#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test fopen, fclose, freopen, fopen64, freopen64

rm -f test_fopen_file test_fopen_file2 test_fopen64_file test_fopen64_file2
./test/test-fopen
