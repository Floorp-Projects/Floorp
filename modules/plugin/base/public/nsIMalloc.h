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

#ifndef nsIMalloc_h___
#define nsIMalloc_h___

#include "nsISupports.h"

class nsIMalloc : public nsISupports {
public:

    /**
     * Allocates a block of memory of a particular size. 
     *
     * @param size - the size of the block to allocate
     * @result the block of memory
     */
    NS_IMETHOD_(void*)
    Alloc(PRUint32 size) = 0;

    /**
     * Reallocates a block of memory to a new size.
     *
     * @param ptr - the block of memory to reallocate
     * @param size - the new size
     * @result the rellocated block of memory
     */
    NS_IMETHOD_(void*)
    Realloc(void* ptr, PRUint32 size) = 0;

    /**
     * Frees a block of memory. 
     *
     * @param ptr - the block of memory to free
     */
    NS_IMETHOD_(void)
    Free(void* ptr) = 0;

    /**
     * Returns the size of a block of memory. Returns -1
     * if the size is not available.
     *
     * @param ptr - the block of memory
     * @result the size or -1 if not available
     */
    NS_IMETHOD_(PRInt32)
    GetSize(void* ptr) = 0;

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
    DidAlloc(void* ptr) = 0;

    /**
     * Attempts to shrink the heap.
     */
    NS_IMETHOD_(void)
    HeapMinimize(void) = 0;

};

#define NS_IMALLOC_IID                               \
{ /* c4744e60-1875-11d2-815f-006008119d7a */         \
    0xc4744e60,                                      \
    0x1875,                                          \
    0x11d2,                                          \
    {0x81, 0x5f, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIMalloc_h___ */
