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
        mInterfaceRecord->interfaceDescriptor->num_methods;
    mConstantCount =
        mInterfaceRecord->interfaceDescriptor->num_constants;
}

nsInterfaceInfo::~nsInterfaceInfo()
{
    if (this->mParent != NULL)
        NS_RELEASE(mParent);

    // remove interface record's notion of my existence
    mInterfaceRecord->info = NULL;
}

NS_IMETHODIMP
nsInterfaceInfo::GetName(char **name)
{
    return this->mInterfaceRecord->GetName(name);
}

NS_IMETHODIMP
nsInterfaceInfo::GetIID(nsIID **iid)
{
    return this->mInterfaceRecord->GetIID(iid);
}

NS_IMETHODIMP
nsInterfaceInfo::IsScriptable(PRBool* result)
{
    if(!result)
        return NS_ERROR_NULL_POINTER;
    *result = XPT_ID_IS_SCRIPTABLE(this->mInterfaceRecord->
                                                interfaceDescriptor->flags);
    return NS_OK;
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
                                &mInterfaceRecord->interfaceDescriptor->
                                method_descriptors[index - mMethodBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetMethodInfoForName(const char* methodName, uint16 *index,
                                      const nsXPTMethodInfo** result)
{
    // XXX probably want to speed this up with a hashtable, or by at least interning
    // the names to avoid the strcmp
    nsresult rv;
    for (uint16 i = mMethodBaseIndex; i < mMethodCount; i++) {
        const nsXPTMethodInfo* info;
        info = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                                   &mInterfaceRecord->interfaceDescriptor->
                                   method_descriptors[i - mMethodBaseIndex]);
        if (PL_strcmp(methodName, info->name) == 0) {
#ifdef NS_DEBUG
            // make sure there aren't duplicate names
            for (; i < mMethodCount; i++) {
                const nsXPTMethodInfo* info2;
                info2 = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                                           &mInterfaceRecord->interfaceDescriptor->
                                           method_descriptors[i - mMethodBaseIndex]);
                NS_ASSERTION(PL_strcmp(methodName, info2->name)!= 0, "duplicate names");
            }
#endif
            *result = info;
            return NS_OK;
        }
    }
    return NS_ERROR_INVALID_ARG;
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
                            &mInterfaceRecord->interfaceDescriptor->
                            const_descriptors[index-mConstantBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
nsInterfaceInfo::GetInfoForParam(const nsXPTParamInfo *param, 
                                 nsIInterfaceInfo** info)
{
    // XXX should be a soft failure?
    NS_PRECONDITION(param->GetType().TagPart() == nsXPTType::T_INTERFACE,
                    "not an interface");

    nsInterfaceRecord *paramRecord =
        *(this->mInterfaceRecord->typelibRecord->
          interfaceRecords + (param->type.type.interface - 1));

    return paramRecord->GetInfo((nsInterfaceInfo **)info);
}

NS_IMETHODIMP
nsInterfaceInfo::GetIIDForParam(const nsXPTParamInfo* param, nsIID** iid)
{
    NS_PRECONDITION(param->GetType().TagPart() == nsXPTType::T_INTERFACE,
                    "not an interface");

    nsInterfaceRecord *paramRecord =
        *(this->mInterfaceRecord->typelibRecord->
          interfaceRecords + (param->type.type.interface - 1));
    
    return paramRecord->GetIID(iid);
}

#ifdef DEBUG
#include <stdio.h>
void
nsInterfaceInfo::print(FILE *fd)
{
    fprintf(fd, "iid: %s name: %s name_space: %s\n",
            this->mInterfaceRecord->iid.ToString(),
            this->mInterfaceRecord->name,
            this->mInterfaceRecord->name_space);
    if (mParent != NULL) {
        fprintf(fd, "parent:\n\t");
        this->mParent->print(fd);
    }
}
#endif
