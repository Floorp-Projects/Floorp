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
 * The Original Code is nsCacheRequest.h, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#ifndef _nsCacheRequest_h_
#define _nsCacheRequest_h_

#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsICache.h"
#include "nsICacheListener.h"


class nsCacheRequest
{
private:
    friend class nsCacheService;
    friend class nsCacheEntry;

    nsCacheRequest( nsCString *           key, 
                    nsICacheListener *    listener,
                    nsCacheAccessMode     accessRequested,
                    PRBool                streamBased,
                    nsCacheStoragePolicy  storagePolicy)

        : mKey(key),
          mListener(listener),
          mAccessRequested(accessRequested),
          mStreamBased(streamBased),
          mStoragePolicy(storagePolicy)
    {
        mRequestThread = PR_GetCurrentThread();

        PR_INIT_CLIST(&mListLink);
    }
    
    ~nsCacheRequest()
    {
        delete mKey;
        NS_ASSERTION(PR_CLIST_IS_EMPTY(&mListLink), "request still on a list");
    }
    

    PRCList*                GetListNode(void)    { return &mListLink;   }
    static nsCacheRequest*  GetInstance(PRCList* qp) {
        return (nsCacheRequest*)
            ((char*)qp - offsetof(nsCacheRequest, mListLink));
    }


    nsCString *                mKey;
    nsCOMPtr<nsICacheListener> mListener;
    nsCacheAccessMode          mAccessRequested;
    PRBool                     mStreamBased;
    nsCacheStoragePolicy       mStoragePolicy;
    PRThread *                 mRequestThread;
    PRCList                    mListLink;
};

#endif // _nsCacheRequest_h_
