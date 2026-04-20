#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test bind() with unix domain sockets

rm -f test_bind_sock
./test/test-bind
