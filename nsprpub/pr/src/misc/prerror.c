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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "primpl.h"

#include <string.h>
#include <stdlib.h>

PR_IMPLEMENT(PRErrorCode) PR_GetError(void)
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->errorCode;
}

PR_IMPLEMENT(PRInt32) PR_GetOSError(void)
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->osErrorCode;
}

PR_IMPLEMENT(void) PR_SetError(PRErrorCode code, PRInt32 osErr)
{
    PRThread *thread = PR_GetCurrentThread();
    thread->errorCode = code;
    thread->osErrorCode = osErr;
    thread->errorStringLength = 0;
}

PR_IMPLEMENT(void) PR_SetErrorText(PRIntn textLength, const char *text)
{
    PRThread *thread = PR_GetCurrentThread();

    if (0 == textLength)
    {
	    if (NULL != thread->errorString)
	        PR_DELETE(thread->errorString);
	    thread->errorStringSize = 0;
    }
    else
    {
	    PRIntn size = textLength + 31;  /* actual length to allocate. Plus a little extra */
        if (thread->errorStringSize < textLength+1)  /* do we have room? */
        {
	        if (NULL != thread->errorString)
	            PR_DELETE(thread->errorString);
		    thread->errorString = (char*)PR_MALLOC(size);
            if ( NULL == thread->errorString ) {
                thread->errorStringSize = 0;
                thread->errorStringLength = 0;
                return;
            }
            thread->errorStringSize = size;
	    }
        memcpy(thread->errorString, text, textLength+1 );
    }
    thread->errorStringLength = textLength;
}

PR_IMPLEMENT(PRInt32) PR_GetErrorTextLength(void)
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->errorStringLength;
}  /* PR_GetErrorTextLength */

PR_IMPLEMENT(PRInt32) PR_GetErrorText(char *text)
{
    PRThread *thread = PR_GetCurrentThread();
    if (0 != thread->errorStringLength)
        memcpy(text, thread->errorString, thread->errorStringLength+1);
    return thread->errorStringLength;
}  /* PR_GetErrorText */


