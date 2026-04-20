#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test utime, utimes, lutimes

rm -f test_utime_file test_utime_link
./test/test-utime
