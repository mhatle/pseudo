#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test fsync, fdatasync, sync

rm -f test_sync_file
./test/test-sync
