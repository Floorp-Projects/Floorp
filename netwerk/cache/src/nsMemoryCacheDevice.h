/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsMemoryCacheDevice.h, released February 20, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 20-February-2001
 */

#ifndef _nsMemoryCacheDevice_h_
#define _nsMemoryCacheDevice_h_

#include "nsCacheDevice.h"
#include "pldhash.h"
#include "nsCacheEntry.h"



class nsMemoryCacheDevice : public nsCacheDevice
{
public:
    enum { kMemoryCacheID = 0x4d656d43 };  // 'MemC'

    nsMemoryCacheDevice();
    virtual ~nsMemoryCacheDevice();

    nsresult  Init();

    static nsresult  Create(nsCacheDevice **result);


    virtual nsresult ActivateEntryIfFound( nsCacheEntry * entry );
    virtual nsresult DeactivateEntry( nsCacheEntry * entry );
    virtual nsresult BindEntry( nsCacheEntry * entry );
    
private:
    nsCacheEntryHashTable   mInactiveEntries;

    PRUint32                mTemporaryMaxLimit;
    PRUint32                mNormalMaxLimit;
    PRUint32                mCurrentTotal;

    PRCList                 mEvictionList;

    //** what other stats do we want to keep?
};


class nsMemoryEvictionElement
{
private:
    friend class nsMemoryCacheDevice;

    nsMemoryEvictionElement(nsCacheEntry *entry)
        : entry(entry)
    {
        PR_INIT_CLIST(&link);
    }

    ~nsMemoryEvictionElement() {}

    PRCList *                        GetListNode(void)         { return &link; }
    static nsMemoryEvictionElement * GetInstance(PRCList * qp) {
        return (nsMemoryEvictionElement*)((char*)qp -
                                          offsetof(nsMemoryEvictionElement, link));
    }

    PRCList        link;
    nsCacheEntry * entry;
    //** need backpointers from entry, or more sophisticated structure to optimize
    //** finding the eviction element quickly.  Otherwise we just walk the list :-(
};


#endif // _nsMemoryCacheDevice_h_
