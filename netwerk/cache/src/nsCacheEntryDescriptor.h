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
 * The Original Code is nsCacheEntryDescriptor.h, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */


#ifndef _nsCacheEntryDescriptor_h_
#define _nsCacheEntryDescriptor_h_

#include "nsICacheEntryDescriptor.h"
#include "nsCacheEntry.h"

class nsCacheEntryDescriptor :
    public nsICacheEntryDescriptor,
    public nsITransport
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT
    NS_DECL_NSICACHEENTRYDESCRIPTOR
    
    nsCacheEntryDescriptor(nsCacheEntry * entry, nsCacheAccessMode  mode);
    virtual ~nsCacheEntryDescriptor();

    static nsresult Create(nsCacheEntry * entry, nsCacheAccessMode  accessGranted,
                           nsICacheEntryDescriptor ** result);

    /**
     *  routines for manipulating descriptors in PRCLists
     */
    PRCList*                        GetListNode(void)    { return &mListLink;   }
    static nsCacheEntryDescriptor*  GetInstance(PRCList* qp) {
        return (nsCacheEntryDescriptor*)
            ((char*)qp - offsetof(nsCacheEntryDescriptor, mListLink));
    }

    /**
     * utility method to attempt changing data size of associated entry
     */
    nsresult  RequestDataSizeChange(PRInt32 deltaSize);

    /**
     * methods callbacks for nsCacheService
     */
    nsCacheEntry * CacheEntry(void) { return mCacheEntry; }    

protected:
    
    PRCList                 mListLink;
    nsCacheEntry          * mCacheEntry; // we are a child of the entry
    nsCacheAccessMode       mAccessGranted;
    nsCOMPtr<nsITransport>  mTransport;
};


#endif // _nsCacheEntryDescriptor_h_
