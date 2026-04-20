#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test mknod, mknodat

rm -f test_mknod_fifo test_mknod_dev test_mknodat_fifo
./test/test-mknod
