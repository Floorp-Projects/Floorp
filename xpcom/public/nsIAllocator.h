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

#ifndef nsIAllocator_h___
#define nsIAllocator_h___

#include "nsISupports.h"

/**
 * Unlike IMalloc, this interface returns nsresults and doesn't
 * implement the problematic GetSize and DidAlloc routines.
 */

#define NS_IALLOCATOR_IID                            \
{ /* 56def700-b1b9-11d2-8177-006008119d7a */         \
    0x56def700,                                      \
    0xb1b9,                                          \
    0x11d2,                                          \
    {0x81, 0x77, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

class nsIAllocator : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IALLOCATOR_IID; return iid; }
    
    /**
     * Allocates a block of memory of a particular size. 
     *
     * @param size - the size of the block to allocate
     * @result the block of memory
     */
    NS_IMETHOD_(void*) Alloc(PRUint32 size) = 0;

    /**
     * Reallocates a block of memory to a new size.
     *
     * @param ptr - the block of memory to reallocate
     * @param size - the new size
     * @result the rellocated block of memory
     */
    NS_IMETHOD_(void*) Realloc(void* ptr, PRUint32 size) = 0;

    /**
     * Frees a block of memory. 
     *
     * @param ptr - the block of memory to free
     */
    NS_IMETHOD Free(void* ptr) = 0;

    /**
     * Attempts to shrink the heap.
     */
    NS_IMETHOD HeapMinimize(void) = 0;

};

// To get the global memory manager service:
#define NS_ALLOCATOR_CID                             \
{ /* aafe6770-b1bb-11d2-8177-006008119d7a */         \
    0xaafe6770,                                      \
    0xb1bb,                                          \
    0x11d2,                                          \
    {0x81, 0x77, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/*
* Public shortcuts to the shared allocator's methods
*/

class nsAllocator
{
public:
    static NS_EXPORT void* Alloc(PRUint32 size);
    static NS_EXPORT void* Realloc(void* ptr, PRUint32 size);
    static NS_EXPORT void  Free(void* ptr);
    static NS_EXPORT void  HeapMinimize();
    static NS_EXPORT void* Clone(const void* ptr,  PRUint32 size);
private:
    nsAllocator();   // not implemented
    static PRBool EnsureAllocator() {return mAllocator || FetchAllocator();}
    static PRBool FetchAllocator();
    static nsIAllocator* mAllocator;
};


////////////////////////////////////////////////////////////////////////////////

#endif /* nsIAllocator_h___ */
