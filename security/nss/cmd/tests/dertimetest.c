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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include <stdio.h>
#include <stdlib.h>

#include "secder.h"
#include "secerr.h"

int main()
{
    SECItem badTime;
    PRTime prtime;
    SECStatus rv;
    int error;
    PRBool failed = PR_FALSE;

    /* A UTCTime string with an embedded null. */
    badTime.type = siBuffer;
    badTime.data = (unsigned char *)"091219000000Z\0junkjunkjunkjunkjunkjunk";
    badTime.len = 38;
    rv = DER_UTCTimeToTime(&prtime, &badTime);
    if (rv == SECSuccess) {
        fprintf(stderr, "DER_UTCTimeToTime should have failed but "
                "succeeded\n");
        failed = PR_TRUE;
    } else {
        error = PORT_GetError();
        if (error != SEC_ERROR_INVALID_TIME) {
            fprintf(stderr, "DER_UTCTimeToTime failed with error %d, "
                    "expected error %d\n", error, SEC_ERROR_INVALID_TIME);
            failed = PR_TRUE;
        }
    }

    /* A UTCTime string with junk after a valid date/time. */
    badTime.type = siBuffer;
    badTime.data = (unsigned char *)"091219000000Zjunk";
    badTime.len = 17;
    rv = DER_UTCTimeToTime(&prtime, &badTime);
    if (rv == SECSuccess) {
        fprintf(stderr, "DER_UTCTimeToTime should have failed but "
                "succeeded\n");
        failed = PR_TRUE;
    } else {
        error = PORT_GetError();
        if (error != SEC_ERROR_INVALID_TIME) {
            fprintf(stderr, "DER_UTCTimeToTime failed with error %d, "
                    "expected error %d\n", error, SEC_ERROR_INVALID_TIME);
            failed = PR_TRUE;
        }
    }

    /* A GeneralizedTime string with an embedded null. */
    badTime.type = siBuffer;
    badTime.data = (unsigned char *)"20091219000000Z\0junkjunkjunkjunkjunkjunk";
    badTime.len = 40;
    rv = DER_GeneralizedTimeToTime(&prtime, &badTime);
    if (rv == SECSuccess) {
        fprintf(stderr, "DER_GeneralizedTimeToTime should have failed but "
                "succeeded\n");
        failed = PR_TRUE;
    } else {
        error = PORT_GetError();
        if (error != SEC_ERROR_INVALID_TIME) {
            fprintf(stderr, "DER_GeneralizedTimeToTime failed with error %d, "
                    "expected error %d\n", error, SEC_ERROR_INVALID_TIME);
            failed = PR_TRUE;
        }
    }

    /* A GeneralizedTime string with junk after a valid date/time. */
    badTime.type = siBuffer;
    badTime.data = (unsigned char *)"20091219000000Zjunk";
    badTime.len = 19;
    rv = DER_GeneralizedTimeToTime(&prtime, &badTime);
    if (rv == SECSuccess) {
        fprintf(stderr, "DER_GeneralizedTimeToTime should have failed but "
                "succeeded\n");
        failed = PR_TRUE;
    } else {
        error = PORT_GetError();
        if (error != SEC_ERROR_INVALID_TIME) {
            fprintf(stderr, "DER_GeneralizedTimeToTime failed with error %d, "
                    "expected error %d\n", error, SEC_ERROR_INVALID_TIME);
            failed = PR_TRUE;
        }
    }

    if (failed) {
        fprintf(stderr, "FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
