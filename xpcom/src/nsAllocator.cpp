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

////////////////////////////////////////////////////////////////////////////////
// Implementation of nsIAllocator using NSPR
////////////////////////////////////////////////////////////////////////////////

#include "nsAllocator.h"
#include "nsIServiceManager.h"
#include <string.h>     /* for memcpy */

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);

nsAllocator::nsAllocator(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

nsAllocator::~nsAllocator(void)
{
}

NS_IMPL_AGGREGATED(nsAllocator);

NS_METHOD
nsAllocator::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kIAllocatorIID) || 
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE;
}

NS_METHOD
nsAllocator::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsAllocator* mm = new nsAllocator(outer);
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    mm->AddRef();
    if (aIID.Equals(kISupportsIID))
        *aInstancePtr = mm->GetInner();
    else
        *aInstancePtr = mm;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD_(void*)
nsAllocator::Alloc(PRUint32 size)
{
    return PR_Malloc(size);
}

NS_METHOD_(void*)
nsAllocator::Realloc(void* ptr, PRUint32 size)
{
    return PR_Realloc(ptr, size);
}

NS_METHOD
nsAllocator::Free(void* ptr)
{
    PR_Free(ptr);
    return NS_OK;
}

NS_METHOD
nsAllocator::HeapMinimize(void)
{
#ifdef XP_MAC
    // This used to live in the memory allocators no Mac, but does no more
    // Needs to be hooked up in the new world.
//    CallCacheFlushers(0x7fffffff);
#endif
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsAllocatorFactory::nsAllocatorFactory(void)
{
}

nsAllocatorFactory::~nsAllocatorFactory(void)
{
}

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsAllocatorFactory, kIFactoryIID);

NS_METHOD
nsAllocatorFactory::CreateInstance(nsISupports *aOuter,
                                   REFNSIID aIID,
                                   void **aResult)
{
    return nsAllocator::Create(aOuter, aIID, aResult);
}

NS_METHOD
nsAllocatorFactory::LockFactory(PRBool aLock)
{
    return NS_OK;       // XXX what?
}

////////////////////////////////////////////////////////////////////////////////

/*
* Public shortcuts to the shared allocator's methods
*   (all these methods are class statics)
*/

// public:
void* NSTaskMem::Alloc(PRUint32 size) 
{
    if(!EnsureAllocator()) return NULL;
    return mAllocator->Alloc(size);
}

void* NSTaskMem::Realloc(void* ptr, PRUint32 size)
{
    if(!EnsureAllocator()) return NULL;
    return mAllocator->Realloc(ptr, size);
}

void  NSTaskMem::Free(void* ptr)
{
    if(!EnsureAllocator()) return;
    mAllocator->Free(ptr);
}

void  NSTaskMem::HeapMinimize()
{
    if(!EnsureAllocator()) return;
    mAllocator->HeapMinimize();
}

void* NSTaskMem::Clone(const void* ptr,  PRUint32 size)
{
    if(!ptr || !EnsureAllocator()) return NULL;
    void* p = mAllocator->Alloc(size);
    if(p) memcpy(p, ptr, size);
    return p;
}        

// private:

nsIAllocator* NSTaskMem::mAllocator = NULL;

PRBool NSTaskMem::FetchAllocator()
{
    NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
    NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
    nsServiceManager::GetService(kAllocatorCID, kIAllocatorIID, 
                                 (nsISupports **)&mAllocator);
    NS_ASSERTION(mAllocator, "failed to get Allocator!");
    return (PRBool) mAllocator;
}    
