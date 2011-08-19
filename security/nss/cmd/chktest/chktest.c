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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#include <stdio.h>
#include <stdlib.h>

#include "blapi.h"
#include "secutil.h"

static int Usage()
{
    fprintf(stderr, "Usage:  chktest <full-path-to-shared-library>\n");
    fprintf(stderr, "        Will test for valid chk file.\n");
    fprintf(stderr, "        Will print SUCCESS or FAILURE.\n");
    exit(1);
}

int main(int argc, char **argv)
{
    SECStatus rv = SECFailure;
    PRBool good_result = PR_FALSE;

    if (argc != 2)
      return Usage();
    
    rv = RNG_RNGInit();
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError("");
        return -1;
    }
    rv = BL_Init();
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError("");
        return -1;
    }
    RNG_SystemInfoForRNG();

    good_result = BLAPI_SHVerifyFile(argv[1]);
    printf("%s\n", 
      (good_result ? "SUCCESS" : "FAILURE"));
    return (good_result) ? SECSuccess : SECFailure;
}
