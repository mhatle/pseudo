/*
 * Helper for chroot re-exec test
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2)
        return 2;
    return (chroot(argv[1]) == -1);
}
