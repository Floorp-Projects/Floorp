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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "primpl.h"

#include <string.h>
#include <stdlib.h>

PR_IMPLEMENT(PRErrorCode) PR_GetError()
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->errorCode;
}

PR_IMPLEMENT(PRInt32) PR_GetOSError()
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->osErrorCode;
}

PR_IMPLEMENT(const char*) PR_GetErrorString()
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->errorString;
}

PR_IMPLEMENT(void) PR_SetError(PRErrorCode code, PRInt32 osErr)
{
    PRThread *thread = PR_GetCurrentThread();
    thread->errorCode = code;
    thread->osErrorCode = osErr;
    thread->errorStringSize = 0;
    PR_DELETE(thread->errorString);
}

PR_IMPLEMENT(void) PR_SetErrorText(PRIntn textLength, const char *text)
{
    PRThread *thread = PR_GetCurrentThread();

    if (0 == textLength)
    {
	    if (NULL != thread->errorString)
	        PR_DELETE(thread->errorString);
    }
    else
    {
	    PRIntn size = textLength + 1;  /* actual length to allocate */
        if (thread->errorStringSize < textLength)  /* do we have room? */
        {
	        if (NULL != thread->errorString)
	            PR_DELETE(thread->errorString);
		    thread->errorString = (char*)PR_MALLOC(size);
	    }
        memcpy(thread->errorString, text, size);
    }
    thread->errorStringSize = textLength;
}

PR_IMPLEMENT(PRInt32) PR_GetErrorTextLength(void)
{
    PRThread *thread = PR_GetCurrentThread();
    return thread->errorStringSize;
}  /* PR_GetErrorTextLength */

PR_IMPLEMENT(PRInt32) PR_GetErrorText(char *text)
{
    PRThread *thread = PR_GetCurrentThread();
    if (0 != thread->errorStringSize)
        memcpy(text, thread->errorString, thread->errorStringSize + 1);
    return thread->errorStringSize;
}  /* PR_GetErrorText */


