/*
 * Function to set error code only when meaningful error has not already 
 * been set.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: sslerr.c,v 1.2 2000/09/07 03:35:31 nelsonb%netscape.com Exp $
 */

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
