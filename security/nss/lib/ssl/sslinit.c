/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prtypes.h"
#include "prinit.h"
#include "seccomon.h"
#include "secerr.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"

static int ssl_isInited = 0;
static PRCallOnceType ssl_init = { 0 };

PRStatus
ssl_InitCallOnce(void *arg)
{
    int *error = (int *)arg;
    SECStatus rv;

    rv = ssl_InitializePRErrorTable();
    if (rv != SECSuccess) {
        *error = SEC_ERROR_NO_MEMORY;
        return PR_FAILURE;
    }
#ifdef DEBUG
    ssl3_CheckCipherSuiteOrderConsistency();
#endif

    rv = ssl3_ApplyNSSPolicy();
    if (rv != SECSuccess) {
        *error = PORT_GetError();
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

SECStatus
ssl_Init(void)
{
    PRStatus nrv;

    /* short circuit test if we are already inited */
    if (!ssl_isInited) {
        int error;
        /* only do this once at init time, block all others until we are done */
        nrv = PR_CallOnceWithArg(&ssl_init, ssl_InitCallOnce, &error);
        if (nrv != PR_SUCCESS) {
            PORT_SetError(error);
            return SECFailure;
        }
        ssl_isInited = 1;
    }
    return SECSuccess;
}
