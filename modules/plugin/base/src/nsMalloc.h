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
// Implementation of nsIMalloc using NSPR
////////////////////////////////////////////////////////////////////////////////

#ifndef nsMalloc_h__
#define nsMalloc_h__

#include "nsIMalloc.h"
#include "prmem.h"
#include "nsAgg.h"

class nsMalloc : public nsIMalloc {
public:

    /**
     * Allocates a block of memory of a particular size. 
     *
     * @param size - the size of the block to allocate
     * @result the block of memory
     */
    NS_IMETHOD_(void*)
    Alloc(PRUint32 size);

    /**
     * Reallocates a block of memory to a new size.
     *
     * @param ptr - the block of memory to reallocate
     * @param size - the new size
     * @result the rellocated block of memory
     */
    NS_IMETHOD_(void*)
    Realloc(void* ptr, PRUint32 size);

    /**
     * Frees a block of memory. 
     *
     * @param ptr - the block of memory to free
     */
    NS_IMETHOD_(void)
    Free(void* ptr);

    /**
     * Returns the size of a block of memory. Returns -1
     * if the size is not available.
     *
     * @param ptr - the block of memory
     * @result the size or -1 if not available
     */
    NS_IMETHOD_(PRInt32)
    GetSize(void* ptr);

    /**
     * Returns whether the block of memory was allocated by this
     * memory allocator. Returns PR_FALSE if this information is
     * not available.
     *
     * @param ptr - the block of memory
     * @result true if allocated by this nsIMalloc, false if not or 
     * if it can't be determined
     */
    NS_IMETHOD_(PRBool)
    DidAlloc(void* ptr);

    /**
     * Attempts to shrink the heap.
     */
    NS_IMETHOD_(void)
    HeapMinimize(void);

    ////////////////////////////////////////////////////////////////////////////

    nsMalloc(nsISupports* outer);
    virtual ~nsMalloc(void);

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

};

////////////////////////////////////////////////////////////////////////////////
#endif // nsMalloc_h__
