/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is a simple test of the PR_fprintf() function for size_t formats.
 */

#include "prprf.h"
#include <sys/types.h>
#include <limits.h>
#include <string.h>

int
main(int argc, char **argv)
{
    char buffer[128];

    size_t  unsigned_small  = 266;
#ifdef XP_UNIX
    ssize_t signed_small_p  = 943;
    ssize_t signed_small_n  = -1;
#endif
    size_t  unsigned_max    = SIZE_MAX;
    size_t  unsigned_min    = 0;
#ifdef XP_UNIX
    ssize_t signed_max      = SSIZE_MAX;
#endif

    printf("Test: unsigned small '%%zu' : ");
    PR_snprintf(buffer, sizeof(buffer), "%zu", unsigned_small);
    if (strncmp(buffer, "266", sizeof(buffer)) != 0) {
        printf("Failed, got '%s'\n", buffer);
        return -1;
    }
    printf("OK\n");

#ifdef XP_UNIX
    printf("Test: signed small positive '%%zd' : ");
    PR_snprintf(buffer, sizeof(buffer), "%zd", signed_small_p);
    if (strncmp(buffer, "943", sizeof(buffer)) != 0) {
        printf("Failed, got '%s'\n", buffer);
        return -1;
    }
    printf("OK\n");

    printf("Test: signed small negative '%%zd' : ");
    PR_snprintf(buffer, sizeof(buffer), "%zd", signed_small_n);
    if (strncmp(buffer, "-1", sizeof(buffer)) != 0) {
        printf("Failed, got '%s'\n", buffer);
        return -1;
    }
    printf("OK\n");
#endif

    printf("Test: 0 '%%zu' : ");
    PR_snprintf(buffer, sizeof(buffer), "%zu", unsigned_min);
    if (strncmp(buffer, "0", sizeof(buffer)) != 0) {
        printf("Failed, got '%s'\n", buffer);
        return -1;
    }
    printf("OK\n");

    printf("Test: SIZE_MAX '%%zx' : ");
    PR_snprintf(buffer, sizeof(buffer), "%zx", unsigned_max);
    if (strspn(buffer, "f") != sizeof(size_t) * 2) {
        printf("Failed, got '%s'\n", buffer);
        return -1;
    }
    printf("OK\n");

#ifdef XP_UNIX
    printf("Test: SSIZE_MAX '%%zx' : ");
    PR_snprintf(buffer, sizeof(buffer), "%zx", signed_max);
    if (*buffer != '7' ||
        strspn(buffer + 1, "f") != sizeof(ssize_t) * 2 - 1) {
        printf("Failed, got '%s'\n", buffer);
        return -1;
    }
    printf("OK\n");
#endif

    return 0;
}
