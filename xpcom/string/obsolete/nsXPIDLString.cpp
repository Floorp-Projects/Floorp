/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDebug.h"
#include "nsMemory.h"
#include "nsXPIDLString.h"
#include "plstr.h"

// If the allocator changes, fix it here.
#define XPIDL_STRING_ALLOC(__len)  ((PRUnichar*) nsMemory::Alloc((__len) * sizeof(PRUnichar)))
#define XPIDL_CSTRING_ALLOC(__len) ((char*) nsMemory::Alloc((__len) * sizeof(char)))
#define XPIDL_FREE(__ptr)          (nsMemory::Free(__ptr))

////////////////////////////////////////////////////////////////////////
// nsXPIDLString

nsXPIDLString::~nsXPIDLString()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);
}


PRUnichar*
nsXPIDLString::Copy(const PRUnichar* aString)
{
    NS_ASSERTION(aString, "null ptr");
    if (! aString)
        return 0;

    PRInt32 len = 0;

    {
        const PRUnichar* p = aString;
        while (*p++)
            len++;
    }

    PRUnichar* result = XPIDL_STRING_ALLOC(len + 1);
    if (result) {
        PRUnichar* q = result;
        while (*aString) {
            *q = *aString;
            q++;
            aString++;
        }
        *q = '\0';
    }
    return result;
}


PRUnichar**
nsXPIDLString::StartAssignmentByValue()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);

    mBuf = 0;
    mBufOwner = PR_TRUE;
    return &mBuf;
}


const PRUnichar**
nsXPIDLString::StartAssignmentByReference()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);

    mBuf = 0;
    mBufOwner = PR_FALSE;
    return (const PRUnichar**) &mBuf;
}


////////////////////////////////////////////////////////////////////////
// nsXPIDLCString

nsXPIDLCString::~nsXPIDLCString()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);
}


nsXPIDLCString& nsXPIDLCString::operator =(const char* aCString)
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);
	
    if (aCString) {
        mBuf = Copy(aCString);
        mBufOwner = PR_TRUE;
    }
    else {
        mBuf = 0;
        mBufOwner = PR_FALSE;
    }
    
    return *this;
}


char*
nsXPIDLCString::Copy(const char* aCString)
{
    NS_ASSERTION(aCString, "null ptr");
    if (! aCString)
        return 0;

    PRInt32 len = PL_strlen(aCString);
    char* result = XPIDL_CSTRING_ALLOC(len + 1);
    if (result)
        PL_strcpy(result, aCString);

    return result;
}


char**
nsXPIDLCString::StartAssignmentByValue()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);

    mBuf = 0;
    mBufOwner = PR_TRUE;
    return &mBuf;
}


const char**
nsXPIDLCString::StartAssignmentByReference()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);

    mBuf = 0;
    mBufOwner = PR_FALSE;
    return (const char**) &mBuf;
}


