/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "prerror.h"
#include "sslerr.h"
#include "prinit.h"
#include "nssutil.h"
#include "ssl.h"

#define ER3(name, value, str) { #name, str },

static const struct PRErrorMessage ssltext[] = {
#include "SSLerrs.h"
    { 0, 0 }
};

static const struct PRErrorTable ssl_et = {
    ssltext, "sslerr", SSL_ERROR_BASE,
    (sizeof ssltext) / (sizeof ssltext[0])
};

static PRStatus
ssl_InitializePRErrorTableOnce(void)
{
    return PR_ErrorInstallTable(&ssl_et);
}

static PRCallOnceType once;

SECStatus
ssl_InitializePRErrorTable(void)
{
    return (PR_SUCCESS == PR_CallOnce(&once, ssl_InitializePRErrorTableOnce))
               ? SECSuccess
               : SECFailure;
}
