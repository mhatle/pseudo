#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test that file operations respect emulated chroot boundaries
# using both absolute and relative paths, including path traversal.

CHROOT_DIR=$(mktemp -d "${PWD}/chroot_conf_XXXXXX")
trap "rm -rf '$CHROOT_DIR'" 0

./test/test-chroot-confinement "$CHROOT_DIR"
