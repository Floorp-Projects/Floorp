/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#include "nsDebug.h"
#include "nsXPIDLString.h"
#include "plstr.h"

// XXX change to use nsIAllocator or whatever
#define XPIDL_STRING_ALLOC(__size)  new PRUnichar[(__size)]
#define XPIDL_CSTRING_ALLOC(__size) new char[(__size)]
#define XPIDL_FREE(__ptr)           delete[] (__ptr)

////////////////////////////////////////////////////////////////////////
// nsXPIDLString

nsXPIDLString::nsXPIDLString()
    : mBufOwner(PR_FALSE),
      mBuf(0)
{
}


nsXPIDLString::~nsXPIDLString()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);
}


nsXPIDLString::operator const PRUnichar*()
{
    return mBuf;
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

    mBufOwner = PR_TRUE;
    return &mBuf;
}


const PRUnichar**
nsXPIDLString::StartAssignmentByReference()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);

    mBufOwner = PR_FALSE;
    return (const PRUnichar**) &mBuf;
}


////////////////////////////////////////////////////////////////////////
// nsXPIDLCString

nsXPIDLCString::nsXPIDLCString()
    : mBufOwner(PR_FALSE),
      mBuf(0)
{
}


nsXPIDLCString::~nsXPIDLCString()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);
}


nsXPIDLCString::operator const char*()
{
    return mBuf;
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

    mBufOwner = PR_TRUE;
    return &mBuf;
}


const char**
nsXPIDLCString::StartAssignmentByReference()
{
    if (mBufOwner && mBuf)
        XPIDL_FREE(mBuf);

    mBufOwner = PR_FALSE;
    return (const char**) &mBuf;
}


