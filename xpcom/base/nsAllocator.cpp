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
#include <string.h>     /* for memcpy */

nsAllocatorImpl::nsAllocatorImpl(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

nsAllocatorImpl::~nsAllocatorImpl(void)
{
}

NS_IMPL_AGGREGATED(nsAllocatorImpl);

NS_METHOD
nsAllocatorImpl::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

	 if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	     *aInstancePtr = GetInner();
    else if (aIID.Equals(nsIAllocator::GetIID()))
        *aInstancePtr = NS_STATIC_CAST(nsIAllocator*, this);
	 else { 
        *aInstancePtr = nsnull; 
        return NS_NOINTERFACE; 
    }
	 
	 NS_ADDREF((nsISupports*)*aInstancePtr); 
    return NS_OK;
}

NS_METHOD
nsAllocatorImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
	 NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsAllocatorImpl* mm = new nsAllocatorImpl(outer);
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

	 nsresult rv = mm->AggregatedQueryInterface(aIID, aInstancePtr);
	 
	 if (NS_FAILED(rv))
	     delete mm;
	 return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD_(void*)
nsAllocatorImpl::Alloc(PRUint32 size)
{
    return PR_Malloc(size);
}

NS_METHOD_(void*)
nsAllocatorImpl::Realloc(void* ptr, PRUint32 size)
{
    return PR_Realloc(ptr, size);
}

NS_METHOD
nsAllocatorImpl::Free(void* ptr)
{
    PR_Free(ptr);
    return NS_OK;
}

NS_METHOD
nsAllocatorImpl::HeapMinimize(void)
{
#ifdef XP_MAC
    // This used to live in the memory allocators no Mac, but does no more
    // Needs to be hooked up in the new world.
//    CallCacheFlushers(0x7fffffff);
#endif
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
#if 0
nsAllocatorFactory::nsAllocatorFactory(void)
{
    NS_INIT_REFCNT();
}

nsAllocatorFactory::~nsAllocatorFactory(void)
{
}

NS_IMPL_ISUPPORTS1(nsAllocatorFactory, nsIFactory)

NS_METHOD
nsAllocatorFactory::CreateInstance(nsISupports *aOuter,
                                   REFNSIID aIID,
                                   void **aResult)
{
    return nsAllocatorImpl::Create(aOuter, aIID, aResult);
}

NS_METHOD
nsAllocatorFactory::LockFactory(PRBool aLock)
{
    return NS_OK;       // XXX what?
}
#endif

////////////////////////////////////////////////////////////////////////////////

/*
* Public shortcuts to the shared allocator's methods
*   (all these methods are class statics)
*/

// public:
void* nsAllocator::Alloc(PRUint32 size) 
{
    if(!EnsureAllocator()) return NULL;
    return mAllocator->Alloc(size);
}

void* nsAllocator::Realloc(void* ptr, PRUint32 size)
{
    if(!EnsureAllocator()) return NULL;
    return mAllocator->Realloc(ptr, size);
}

void  nsAllocator::Free(void* ptr)
{
    if(!EnsureAllocator()) return;
    mAllocator->Free(ptr);
}

void  nsAllocator::HeapMinimize()
{
    if(!EnsureAllocator()) return;
    mAllocator->HeapMinimize();
}

void* nsAllocator::Clone(const void* ptr,  PRUint32 size)
{
    if(!ptr || !EnsureAllocator()) return NULL;
    void* p = mAllocator->Alloc(size);
    if(p) memcpy(p, ptr, size);
    return p;
}

NS_EXPORT nsIAllocator* 
nsAllocator::GetGlobalAllocator()
{
    if(!EnsureAllocator()) return nsnull;
    NS_ADDREF(mAllocator);
    return mAllocator;
}

// private:

nsIAllocator* nsAllocator::mAllocator = NULL;

PRBool nsAllocator::FetchAllocator()
{
    nsAllocatorImpl::Create(NULL, nsIAllocator::GetIID(), (void**)&mAllocator);
    NS_ASSERTION(mAllocator, "failed to get Allocator!");
    return (PRBool) mAllocator;
}    
