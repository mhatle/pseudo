/*
 * Test uid/gid functions: getuid, geteuid, getgid, getegid,
 * setuid, seteuid, setgid, setegid, setreuid, setregid,
 * setresuid, setresgid, getresuid, getresgid, getgroups
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    /* Under pseudo, we should appear to be root */
    check("getuid == 0", getuid() == 0);
    check("geteuid == 0", geteuid() == 0);
    check("getgid == 0", getgid() == 0);
    check("getegid == 0", getegid() == 0);

    /* setuid/setgid - should succeed under pseudo */
    check("setuid 0", setuid(0) == 0);
    check("setgid 0", setgid(0) == 0);
    check("seteuid 0", seteuid(0) == 0);
    check("setegid 0", setegid(0) == 0);

    /* setreuid, setregid */
    check("setreuid", setreuid(0, 0) == 0);
    check("setregid", setregid(0, 0) == 0);

    /* getresuid, getresgid */
    check("getresuid", getresuid(&ruid, &euid, &suid) == 0);
    check("getresuid ruid", ruid == 0);
    check("getresuid euid", euid == 0);
    check("getresuid suid", suid == 0);

    check("getresgid", getresgid(&rgid, &egid, &sgid) == 0);
    check("getresgid rgid", rgid == 0);
    check("getresgid egid", egid == 0);
    check("getresgid sgid", sgid == 0);

    /* setresuid, setresgid */
    check("setresuid", setresuid(0, 0, 0) == 0);
    check("setresgid", setresgid(0, 0, 0) == 0);

    /* getgroups */
    {
        int ngroups = getgroups(0, NULL);
        check("getgroups count", ngroups >= 0);
        if (ngroups > 0) {
            gid_t *groups = malloc(ngroups * sizeof(gid_t));
            check("getgroups", getgroups(ngroups, groups) >= 0);
            free(groups);
        }
    }

    return failures;
}
