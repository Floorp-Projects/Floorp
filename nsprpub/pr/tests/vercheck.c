/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 * This release (4.5) is backward compatible with the
 * 4.0.x, 4.1.x, 4.2.x, 4.3.x, and 4.4.x releases.  It, of course,
 * is compatible with itself.
 */
static char *compatible_version[] = {
    "4.0", "4.0.1", "4.1", "4.1.1", "4.1.2", "4.1.3",
    "4.2", "4.2.1", "4.2.2", "4.3", "4.4", "4.4.1", PR_VERSION
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
    "4.5.3",
    "4.6", "4.6.1",
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
