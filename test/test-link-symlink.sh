#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test link, linkat, symlink, symlinkat, readlink, readlinkat

rm -f test_link_file test_link_sym test_link_sym2 test_link_hard test_link_hard2
./test/test-link-symlink
