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

////////////////////////////////////////////////////////////////////////////////
// Implementation of nsIAllocator using NSPR
////////////////////////////////////////////////////////////////////////////////

#ifndef nsAllocator_h__
#define nsAllocator_h__

#include "nsIAllocator.h"
#include "prmem.h"
#include "nsAgg.h"
#include "nsIFactory.h"

class nsAllocatorImpl : public nsIAllocator {
public:
    static const nsCID& CID() { static nsCID cid = NS_ALLOCATOR_CID; return cid; }

    /**
     * Allocates a block of memory of a particular size. 
     *
     * @param size - the size of the block to allocate
     * @result the block of memory
     */
    NS_IMETHOD_(void*) Alloc(PRUint32 size);

    /**
     * Reallocates a block of memory to a new size.
     *
     * @param ptr - the block of memory to reallocate
     * @param size - the new size
     * @result the rellocated block of memory
     */
    NS_IMETHOD_(void*) Realloc(void* ptr, PRUint32 size);

    /**
     * Frees a block of memory. 
     *
     * @param ptr - the block of memory to free
     */
    NS_IMETHOD Free(void* ptr);

    /**
     * Attempts to shrink the heap.
     */
    NS_IMETHOD HeapMinimize(void);

    ////////////////////////////////////////////////////////////////////////////

    nsAllocatorImpl(nsISupports* outer);
    virtual ~nsAllocatorImpl(void);

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
};

////////////////////////////////////////////////////////////////////////////////
#if 0
class nsAllocatorFactory : public nsIFactory {
public:
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    nsAllocatorFactory(void);
    virtual ~nsAllocatorFactory(void);

    NS_DECL_ISUPPORTS
};
#endif
////////////////////////////////////////////////////////////////////////////////
#endif // nsAllocator_h__
