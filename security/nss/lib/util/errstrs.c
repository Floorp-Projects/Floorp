/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "prerror.h"
#include "secerr.h"
#include "secport.h"
#include "prinit.h"
#include "prprf.h"
#include "prtypes.h"
#include "prlog.h"
#include "plstr.h"
#include "nssutil.h"
#include <string.h>

#define ER3(name, value, str) {#name, str},

static const struct PRErrorMessage sectext[] = {
#include "SECerrs.h"
    {0,0}
};

static const struct PRErrorTable sec_et = {
    sectext, "secerrstrings", SEC_ERROR_BASE, 
        (sizeof sectext)/(sizeof sectext[0]) 
};

static PRStatus 
nss_InitializePRErrorTableOnce(void) {
    return PR_ErrorInstallTable(&sec_et);
}

static PRCallOnceType once;

SECStatus
NSS_InitializePRErrorTable(void)
{
    return (PR_SUCCESS == PR_CallOnce(&once, nss_InitializePRErrorTableOnce))
		? SECSuccess : SECFailure;
}

