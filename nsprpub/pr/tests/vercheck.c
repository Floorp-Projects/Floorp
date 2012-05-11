/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File: vercheck.c
 *
 * Description:
 * This test tests the PR_VersionCheck() function.  The
 * compatible_version and incompatible_version arrays
 * need to be updated for each patch or release.
 *
 * Tested areas: library version compatibility check.
 */

#include "prinit.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * This release (4.9.1) is backward compatible with the
 * 4.0.x, 4.1.x, 4.2.x, 4.3.x, 4.4.x, 4.5.x, 4.6.x, 4.7.x,
 * 4.8.x, and 4.9 releases.  It, of course, is compatible with itself.
 */
static char *compatible_version[] = {
    "4.0", "4.0.1", "4.1", "4.1.1", "4.1.2", "4.1.3",
    "4.2", "4.2.1", "4.2.2", "4.3", "4.4", "4.4.1",
    "4.5", "4.5.1",
    "4.6", "4.6.1", "4.6.2", "4.6.3", "4.6.4", "4.6.5",
    "4.6.6", "4.6.7", "4.6.8",
    "4.7", "4.7.1", "4.7.2", "4.7.3", "4.7.4", "4.7.5",
    "4.7.6",
    "4.8", "4.8.1", "4.8.2", "4.8.3", "4.8.4", "4.8.5",
    "4.8.6", "4.8.7", "4.8.8", "4.8.9",
    "4.9", PR_VERSION
};

/*
 * This release is not backward compatible with the old
 * NSPR 2.1 and 3.x releases.
 *
 * Any release is incompatible with future releases and
 * patches.
 */
static char *incompatible_version[] = {
    "2.1 19980529",
    "3.0", "3.0.1",
    "3.1", "3.1.1", "3.1.2", "3.1.3",
    "3.5", "3.5.1",
    "4.9.2",
    "4.10", "4.10.1",
    "10.0", "11.1", "12.14.20"
};

int main(int argc, char **argv)
{
    int idx;
    int num_compatible = sizeof(compatible_version) / sizeof(char *);
    int num_incompatible = sizeof(incompatible_version) / sizeof(char *);

    printf("NSPR release %s:\n", PR_VERSION);
    for (idx = 0; idx < num_compatible; idx++) {
        if (PR_VersionCheck(compatible_version[idx]) == PR_FALSE) {
            fprintf(stderr, "Should be compatible with version %s\n",
                    compatible_version[idx]);
            exit(1);
        }
        printf("Compatible with version %s\n", compatible_version[idx]);
    }

    for (idx = 0; idx < num_incompatible; idx++) {
        if (PR_VersionCheck(incompatible_version[idx]) == PR_TRUE) {
            fprintf(stderr, "Should be incompatible with version %s\n",
                    incompatible_version[idx]);
            exit(1);
        }
        printf("Incompatible with version %s\n", incompatible_version[idx]);
    }

    printf("PASS\n");
    return 0;
}
