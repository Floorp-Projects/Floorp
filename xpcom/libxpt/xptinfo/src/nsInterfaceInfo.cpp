/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nscore.h"

#include "nsISupports.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIAllocator.h"

#include "nsInterfaceInfo.h"
#include "nsInterfaceInfoManager.h"
#include "xptinfo.h"

static NS_DEFINE_IID(kIInterfaceInfoIID, NS_IINTERFACEINFO_IID);
NS_IMPL_ISUPPORTS(nsInterfaceInfo, kIInterfaceInfoIID);

nsInterfaceInfo::nsInterfaceInfo(XPTInterfaceDirectoryEntry* entry,
                                     nsInterfaceInfo *parent)
    :   mEntry(entry),
        mParent(parent)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    if(mParent)
        NS_ADDREF(mParent);
    if(mParent) {
        mMethodBaseIndex =
            mParent->mMethodBaseIndex + mParent->mMethodCount;
        mConstantBaseIndex =
            mParent->mConstantBaseIndex + mParent->mConstantCount;
    }
    else
        mMethodBaseIndex = mConstantBaseIndex = 0;

    mMethodCount   = mEntry->interface_descriptor->num_methods;
    mConstantCount = mEntry->interface_descriptor->num_constants;
}

nsInterfaceInfo::~nsInterfaceInfo()
{
    if(mParent)
        NS_RELEASE(mParent);
}

NS_IMETHODIMP
nsInterfaceInfo::GetName(char** name)
{
    NS_PRECONDITION(name, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = nsInterfaceInfoManager::GetAllocator())) {
        int len = strlen(mEntry->name)+1;
        char* p = (char*)allocator->Alloc(len);
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, mEntry->name, len);
            *name = p;
            return NS_OK;
        }
    }
    
    *name = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInterfaceInfo::GetIID(nsIID** iid)
{
    NS_PRECONDITION(iid, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = nsInterfaceInfoManager::GetAllocator())) {
        nsIID* p = (nsIID*)allocator->Alloc(sizeof(nsIID));
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, &mEntry->iid, sizeof(nsIID));
            *iid = p;
            return NS_OK;
        }
    }

    *iid = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInterfaceInfo::GetParent(nsIInterfaceInfo** parent)
{
    NS_PRECONDITION(parent, "bad param");
    if(mParent) {
        NS_ADDREF(mParent);
        *parent = mParent;
        return NS_OK;
    }
    *parent = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInterfaceInfo::GetMethodCount(uint16* count)
{
    NS_PRECONDITION(count, "bad param");

    *count = mMethodBaseIndex + mMethodCount;
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetConstantCount(uint16* count)
{
    NS_PRECONDITION(count, "bad param");
    *count = mConstantBaseIndex + mConstantCount;
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetMethodInfo(uint16 index, const nsXPTMethodInfo** info)
{
    NS_PRECONDITION(info, "bad param");
    if(index < mMethodBaseIndex)
        return mParent->GetMethodInfo(index, info);

    if(index >= mMethodBaseIndex + mMethodCount)
    {
        NS_PRECONDITION(0, "bad param");
        *info = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *info = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                           &mEntry->interface_descriptor->
                           method_descriptors[index - mMethodBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetConstant(uint16 index, const nsXPTConstant** constant)
{
    NS_PRECONDITION(constant, "bad param");
    if(index < mConstantBaseIndex)
        return mParent->GetConstant(index, constant);

    if(index >= mConstantBaseIndex + mConstantCount)
    {
        NS_PRECONDITION(0, "bad param");
        *constant = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *constant = NS_REINTERPRET_CAST(nsXPTConstant*,
                               &mEntry->interface_descriptor->
                               const_descriptors[index-mConstantBaseIndex]);
    return NS_OK;
}
