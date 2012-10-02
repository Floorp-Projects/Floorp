/*
 * Function to set error code only when meaningful error has not already 
 * been set.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: sslerr.c,v 1.5 2012/04/25 14:50:12 gerv%gerv.net Exp $ */

#include "prerror.h"
#include "secerr.h"
#include "sslerr.h"
#include "seccomon.h"

/* look at the current value of PR_GetError, and evaluate it to see
 * if it is meaningful or meaningless (out of context). 
 * If it is meaningless, replace it with the hiLevelError.
 * Returns the chosen error value.
 */
int
ssl_MapLowLevelError(int hiLevelError)
{
    int oldErr	= PORT_GetError();

    switch (oldErr) {

    case 0:
    case PR_IO_ERROR:
    case SEC_ERROR_IO:
    case SEC_ERROR_BAD_DATA:
    case SEC_ERROR_LIBRARY_FAILURE:
    case SEC_ERROR_EXTENSION_NOT_FOUND:
    case SSL_ERROR_BAD_CLIENT:
    case SSL_ERROR_BAD_SERVER:
    case SSL_ERROR_SESSION_NOT_FOUND:
    	PORT_SetError(hiLevelError);
	return hiLevelError;

    default:	/* leave the majority of error codes alone. */
	return oldErr;
    }
}
