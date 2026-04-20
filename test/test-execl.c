/*
 * Test execl
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include <unistd.h>

int main(void) {
    return execl("/usr/bin/env", "/usr/bin/env", "A=A", "B=B", "C=C", NULL);
}
