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

#include "nsFileUtilities.h"
#include "nsIComponentManager.h"
#include "xp.h"

////////////////////////////////////////////////////////////////////////////////
// File Utilities Interface

nsFileUtilities::nsFileUtilities(nsISupports* outer)
    : fProgramPath(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

nsFileUtilities::~nsFileUtilities(void)
{
}

NS_IMPL_AGGREGATED(nsFileUtilities);

static NS_DEFINE_IID(kIFileUtilitiesIID, NS_IFILEUTILITIES_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_METHOD
nsFileUtilities::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(kIFileUtilitiesIID) || 
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE;
}

NS_METHOD
nsFileUtilities::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsFileUtilities* fu = new nsFileUtilities(outer);
    if (fu == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    fu->AddRef();
    *aInstancePtr = fu->GetInner();
    return NS_OK;
}    

NS_METHOD
nsFileUtilities::GetProgramPath(const char* *result)
{
    *result = fProgramPath;
    return NS_OK;
}

NS_METHOD
nsFileUtilities::GetTempDirPath(const char* *result)
{
    // XXX I don't need a static really, the browser holds the tempDir name
    // as a static string -- it's just the XP_TempDirName that strdups it.
    static const char* tempDirName = NULL;
    if (tempDirName == NULL)
        tempDirName = XP_TempDirName();
    *result = tempDirName;
    return NS_OK;
}

#if 0
NS_METHOD
nsFileUtilities::GetFileName(const char* fn, FileNameType type,
                             char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    XP_FileType filetype;

    if (type == SIGNED_APPLET_DBNAME)
        filetype = xpSignedAppletDB;
    else if (type == TEMP_FILENAME)
        filetype = xpTemporary;
    else 
        return NS_ERROR_ILLEGAL_VALUE;

    char* tempName = WH_FileName(fn, filetype);
    if (tempName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}
#endif

NS_METHOD
nsFileUtilities::NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    char* tempName = WH_TempName(xpTemporary, prefix);
    if (tempName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

