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
    nsMemoryCacheDevice();
    virtual ~nsMemoryCacheDevice();

    nsresult  Init();

    static nsresult  Create(nsCacheDevice **result);


    virtual const char *  GetDeviceID(void);

    virtual nsresult ActivateEntryIfFound( nsCacheEntry * entry );
    virtual nsresult DeactivateEntry( nsCacheEntry * entry );
    virtual nsresult BindEntry( nsCacheEntry * entry );

    virtual nsresult GetTransportForEntry( nsCacheEntry * entry,
                                           nsITransport **transport );

    virtual nsresult OnDataSizeChanged( nsCacheEntry * entry );
 
private:
    nsCacheEntryHashTable   mInactiveEntries;

    PRUint32                mTemporaryMaxLimit;
    PRUint32                mNormalMaxLimit;
    PRUint32                mCurrentTotal;

    PRCList                 mEvictionList;

    //** what other stats do we want to keep?
};

#if 0
class nsMemoryCacheEntry
{
public:


    nsMemoryCacheEntry(nsCacheEntry *entry)
    {
        PR_INIT_CLIST(&mListLink);
        entry->GetKey(&mKey);
        GetFetchCount(&mFetchCount);
        GetLastFetched(&mLastFetched);
        GetLastValidated(&mLastValidated);
        GetExpirationTime(&mExpirationTime);
        //** flags
        GetDataSize(&mDataSize);
        GetMetaDataSize(&mMetaSize);
        GetData(getter_AddRefs(mData)); //** check about ownership
        GetMetaData
    }

    ~nsMemoryCacheEntry() {}

    PRCList *                        GetListNode(void)         { return &mListLink; }
    static nsMemoryCacheEntry * GetInstance(PRCList * qp) {
        return (nsMemoryCacheEntry*)((char*)qp -
                                     offsetof(nsMemoryCacheEntry, mListLink));
    }


private:
    PRCList                mListLink;       // 8
    nsCString *            mKey;            // 4  //** ask scc about const'ness
    PRUint32               mFetchCount;     // 4
    PRTime                 mLastFetched;    // 8
    PRTime                 mLastValidated;  // 8
    PRTime                 mExpirationTime; // 8
    PRUint32               mFlags;          // 4
    PRUint32               mDataSize;       // 4
    PRUint32               mMetaSize;       // 4
    nsCOMPtr<nsISupports>  mData;           // 4 //** issues with updating/replacing
    nsCacheMetaData *      mMetaData;       // 4
};
#endif

#endif // _nsMemoryCacheDevice_h_
