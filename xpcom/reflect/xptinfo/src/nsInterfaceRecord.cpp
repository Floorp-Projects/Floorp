/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
        NS_ADDREF(this->info);
        *result = this->info;
        return NS_OK;
    }

    if (this->interfaceDescriptor == NULL || this->typelibRecord == NULL) {
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
        if (NS_FAILED(nsr)) {
            *result = NULL;
            return nsr;
        }
    }

    // got a parent for it, now build the object itself
    *result = new nsInterfaceInfo(this, (nsInterfaceInfo *)parent);

    // Since the newly created nsInterfaceInfo always holds a ref to
    // the parent, the reference we hold always needs to be released here.
    NS_IF_RELEASE(parent);

    if (*result != NULL) {
        this->info = *result;
    }
    return NS_OK;
}

nsresult
nsInterfaceRecord::GetIID(nsIID **in_iid) {
    NS_PRECONDITION(in_iid, "bad param");

    nsIAllocator* allocator;
    if(this->interfaceDescriptor != NULL &&
       NULL != (allocator = nsInterfaceInfoManager::GetAllocator()))
    {
        nsIID* p = (nsIID*)allocator->Alloc(sizeof(nsIID));
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, &(this->iid), sizeof(nsIID));
            *in_iid = p;
            return NS_OK;
        }
    }

    *in_iid = NULL;
    return NS_ERROR_FAILURE;
}

nsresult
nsInterfaceRecord::GetName(char **in_name)
{
    NS_PRECONDITION(in_name, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = nsInterfaceInfoManager::GetAllocator())) {
        int len = strlen(this->name)+1;
        char* p = (char*)allocator->Alloc(len);
        NS_RELEASE(allocator);
        if(p) {
            memcpy(p, this->name, len);
            *in_name = p;
            return NS_OK;
        }
    }
    
    *in_name = NULL;
    return NS_ERROR_FAILURE;
}
