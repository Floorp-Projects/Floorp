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
    if (mBuf)
        XPIDL_FREE(mBuf);
}


PRUnichar**
nsXPIDLString::StartAssignmentByValue()
{
    if (mBuf)
        XPIDL_FREE(mBuf);
    mBuf = 0;
    return &mBuf;
}


////////////////////////////////////////////////////////////////////////
// nsXPIDLCString

nsXPIDLCString::~nsXPIDLCString()
{
    if (mBuf)
        XPIDL_FREE(mBuf);
}


char**
nsXPIDLCString::StartAssignmentByValue()
{
    if (mBuf)
        XPIDL_FREE(mBuf);
    mBuf = 0;
    return &mBuf;
}


