/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
 * This release (3.1) is backward compatible with all
 * the previous releases.  It, of course, is compatible
 * with itself.
 */
static char *compatible_version[] = {
    "2.1 19980529", "3.0", "3.0.1", PR_VERSION
};

/*
 * Any release is incompatible with future releases and
 * patches.
 */
static char *incompatible_version[] = {
    "3.1.2",
    "3.5", "3.5.1",
    "4.0", "4.0.3",
    "10.0", "11.1", "12.14.20"
};

int main()
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
