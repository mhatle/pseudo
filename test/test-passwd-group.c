/*
 * Test getpwnam, getpwuid, getgrnam, getgrgid and _r variants
 * getpwent, getgrent, setpwent, setgrent, endpwent, endgrent
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    struct passwd *pw;
    struct group *gr;
    struct passwd pwbuf;
    struct group grbuf;
    char buf[4096];
    struct passwd *pwresult;
    struct group *grresult;

    /* getpwnam for root - under pseudo, root should exist */
    pw = getpwnam("root");
    check("getpwnam root", pw != NULL);
    if (pw) {
        check("getpwnam root uid", pw->pw_uid == 0);
        check("getpwnam root name", strcmp(pw->pw_name, "root") == 0);
    }

    /* getpwuid for 0 */
    pw = getpwuid(0);
    check("getpwuid 0", pw != NULL);
    if (pw) {
        check("getpwuid 0 name", strcmp(pw->pw_name, "root") == 0);
    }

    /* getpwnam_r */
    check("getpwnam_r", getpwnam_r("root", &pwbuf, buf, sizeof(buf), &pwresult) == 0);
    check("getpwnam_r result", pwresult != NULL);
    if (pwresult) {
        check("getpwnam_r uid", pwresult->pw_uid == 0);
    }

    /* getpwuid_r */
    check("getpwuid_r", getpwuid_r(0, &pwbuf, buf, sizeof(buf), &pwresult) == 0);
    check("getpwuid_r result", pwresult != NULL);

    /* getgrnam for root */
    gr = getgrnam("root");
    check("getgrnam root", gr != NULL);
    if (gr) {
        check("getgrnam root gid", gr->gr_gid == 0);
    }

    /* getgrgid for 0 */
    gr = getgrgid(0);
    check("getgrgid 0", gr != NULL);
    if (gr) {
        check("getgrgid 0 name", strcmp(gr->gr_name, "root") == 0);
    }

    /* getgrnam_r */
    check("getgrnam_r", getgrnam_r("root", &grbuf, buf, sizeof(buf), &grresult) == 0);
    check("getgrnam_r result", grresult != NULL);

    /* getgrgid_r */
    check("getgrgid_r", getgrgid_r(0, &grbuf, buf, sizeof(buf), &grresult) == 0);
    check("getgrgid_r result", grresult != NULL);

    /* getpwent / setpwent / endpwent - enumerate passwd entries */
    {
        int count = 0;
        setpwent();
        while ((pw = getpwent()) != NULL) {
            count++;
        }
        endpwent();
        check("getpwent found entries", count > 0);
    }

    /* getgrent / setgrent / endgrent - enumerate group entries */
    {
        int count = 0;
        setgrent();
        while ((gr = getgrent()) != NULL) {
            count++;
        }
        endgrent();
        check("getgrent found entries", count > 0);
    }

    return failures;
}
