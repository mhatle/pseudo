#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

# Test if we re-invoke pseudo that chroot still works

# The following should just run the test since pseudo is already loaded
./bin/pseudo ./test/test-reexec-chroot `pwd`
