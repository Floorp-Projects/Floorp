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

/* Persistent interface records, shared between typelib records. */

#include "nsInterfaceRecord.h"
#include "nsInterfaceInfo.h"

// Follow XPCOM pattern for these functions to allow forwarding
// through xpcom interfaces.

nsresult
nsInterfaceRecord::GetInfo(nsInterfaceInfo **result)
{
    if (this->info != NULL) {
        // XXX should add ref here? (not xposed in public api...)
        NS_ADDREF(this->info);
        *result = this->info;
        return NS_OK;
    }

    if (this->interfaceDescriptor == NULL) {
        *result = NULL;
        return NS_ERROR_FAILURE;
    }

    // time to make one.  First, find a parent.
    nsIInterfaceInfo *parent;
    uint16 parent_index = this->interfaceDescriptor->parent_interface;
    if (parent_index == 0) { // is it nsISupports?
        parent = NULL;
    } else {
        nsInterfaceRecord *parent_record =
            this->typelibRecord->interfaceRecords[parent_index - 1];
        nsresult nsr = parent_record->GetInfo((nsInterfaceInfo **)&parent);
        if (NS_IS_ERROR(nsr)) {
            *result = NULL;
            return nsr;  // ? ... or NS_ERROR_FAILURE
        }
    }

    // got a parent for it, now build the object itself
    *result = new nsInterfaceInfo(this, (nsInterfaceInfo *)parent);
    if (*result == NULL) {
        NS_RELEASE(parent);
    } else {
        NS_ADDREF(*result);
        this->info = *result;
    }
    return NS_OK;
}

nsresult
nsInterfaceRecord::GetIID(nsIID **iid) {
    NS_PRECONDITION(iid, "bad param");

    nsIAllocator* allocator;
    if(this->interfaceDescriptor != NULL &&
       NULL != (allocator = nsInterfaceInfoManager::GetAllocator()))
    {
        nsIID* p = (nsIID*)allocator->Alloc(sizeof(nsIID));
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, &(this->iid), sizeof(nsIID));
            *iid = p;
            return NS_OK;
        }
    }

    *iid = NULL;
    return NS_ERROR_FAILURE;
}

nsresult
nsInterfaceRecord::GetName(char **name)
{
    NS_PRECONDITION(name, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = nsInterfaceInfoManager::GetAllocator())) {
        int len = strlen(this->name)+1;
        char* p = (char*)allocator->Alloc(len);
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, this->name, len);
            *name = p;
            return NS_OK;
        }
    }
    
    *name = NULL;
    return NS_ERROR_FAILURE;
}
