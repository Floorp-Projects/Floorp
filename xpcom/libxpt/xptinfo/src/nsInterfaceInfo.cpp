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

/* Implementation of nsIInterfaceInfoManager. */

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

nsInterfaceInfo::nsInterfaceInfo(nsInterfaceRecord *record,
                                 nsInterfaceInfo *parent)
    :   mInterfaceRecord(record),
        mParent(parent)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    if(mParent != NULL) {
        NS_ADDREF(mParent);
        mMethodBaseIndex =
            mParent->mMethodBaseIndex + mParent->mMethodCount;
        mConstantBaseIndex =
            mParent->mConstantBaseIndex + mParent->mConstantCount;
    } else {
        mMethodBaseIndex = mConstantBaseIndex = 0;
    }

    mMethodCount   =
        mInterfaceRecord->entry->interface_descriptor->num_methods;
    mConstantCount =
        mInterfaceRecord->entry->interface_descriptor->num_constants;
}

nsInterfaceInfo::~nsInterfaceInfo()
{
    if (this->mParent != NULL)
        NS_RELEASE(mParent);

    // remove interface record's notion of my existence
    if (this->mInterfaceRecord != NULL)
        this->mInterfaceRecord->info = NULL;
}

NS_IMETHODIMP
nsInterfaceInfo::GetName(char** name)
{
    NS_PRECONDITION(name, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = nsInterfaceInfoManager::GetAllocator())) {
        int len = strlen(mInterfaceRecord->entry->name)+1;
        char* p = (char*)allocator->Alloc(len);
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, mInterfaceRecord->entry->name, len);
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
            memcpy(p, &(mInterfaceRecord->entry->iid), sizeof(nsIID));
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
    if (index < mMethodBaseIndex)
        return mParent->GetMethodInfo(index, info);

    if (index >= mMethodBaseIndex + mMethodCount) {
        NS_PRECONDITION(0, "bad param");
        *info = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *info = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                                &mInterfaceRecord->entry->interface_descriptor->
                                method_descriptors[index - mMethodBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetConstant(uint16 index, const nsXPTConstant** constant)
{
    NS_PRECONDITION(constant, "bad param");
    if (index < mConstantBaseIndex)
        return mParent->GetConstant(index, constant);

    if (index >= mConstantBaseIndex + mConstantCount) {
        NS_PRECONDITION(0, "bad param");
        *constant = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *constant =
        NS_REINTERPRET_CAST(nsXPTConstant*,
                            &mInterfaceRecord->entry->interface_descriptor->
                            const_descriptors[index-mConstantBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetInfoForParam(const nsXPTParamInfo *param, 
                                 nsIInterfaceInfo** info)
{

    NS_PRECONDITION(param->GetType().TagPart() == nsXPTType::T_INTERFACE,
                    "not an interface");

    nsIInterfaceInfoManager* mgr;
    if(NULL == (mgr = nsInterfaceInfoManager::GetInterfaceInfoManager())) {
        *info = NULL;
        return NS_ERROR_FAILURE;
    }

    // what typelib did the entry come from?
    XPTHeader *which_header = mInterfaceRecord->which_header;
    NS_ASSERTION(which_header != NULL, "missing header info assoc'd with II");

    // can't use IID, because it could be null for this entry.
    char *interface_name;

    // offset is 1-based, so subtract 1 to use in interface_directory.
    interface_name =
        which_header->interface_directory[param->type.type.interface - 1].name;

    // does addref.  I'll have to do addref if I find it through a magic
    // array walk.
    nsresult nsr = mgr->GetInfoForName(interface_name, info);
    NS_RELEASE(mgr);
    return nsr;
}

NS_IMETHODIMP
nsInterfaceInfo::GetIIDForParam(const nsXPTParamInfo* param, nsIID** iid)
{
    NS_PRECONDITION(param->GetType().TagPart() == nsXPTType::T_INTERFACE,
                    "not an interface");

    nsIInterfaceInfoManager* mgr;
    if(NULL == (mgr = nsInterfaceInfoManager::GetInterfaceInfoManager())) {
        *iid = NULL;
        return NS_ERROR_FAILURE;
    }

    // what typelib did the entry come from?
    XPTHeader *which_header = mInterfaceRecord->which_header;
    NS_ASSERTION(which_header != NULL, "header missing?");

    // can't use IID, because it could be null for this entry.
    char *interface_name;

    // offset is 1-based, so subtract 1 to use in interface_directory.
    interface_name =
        which_header->interface_directory[param->type.type.interface - 1].name;

    nsresult nsr = mgr->GetIIDForName(interface_name, iid);
    NS_RELEASE(mgr);
    return nsr;
}


#ifdef DEBUG
#include <stdio.h>
void
nsInterfaceInfo::print(FILE *fd)
{
    fprintf(fd, "iid: %s name: %s name_space: %s\n",
            this->mInterfaceRecord->entry->iid.ToString(),
            this->mInterfaceRecord->entry->name,
            this->mInterfaceRecord->entry->name_space);
    if (mParent != NULL) {
        fprintf(fd, "parent:\n\t");
        this->mParent->print(fd);
    }
}
#endif
