/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#ifndef _nsMemCacheChannel_h_
#define _nsMemCacheChannel_h_

#include "nsMemCacheRecord.h"
#include "nsITransport.h"
#include "nsIRequest.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"


class AsyncReadStreamAdaptor;

class nsMemCacheTransport : public nsITransport, public nsITransportRequest
{
public:
    // Constructors and Destructor
    nsMemCacheTransport(nsMemCacheRecord *aRecord, nsILoadGroup *aLoadGroup);
    virtual ~nsMemCacheTransport();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSITRANSPORTREQUEST
    NS_DECL_NSITRANSPORT

protected:
    void NotifyStorageInUse(PRInt32 aBytesUsed);

    nsCOMPtr<nsMemCacheRecord>   mRecord;
    nsCOMPtr<nsIInputStream>     mInputStream;
    nsCOMPtr<nsIRequest>         mCurrentReadRequest;
    AsyncReadStreamAdaptor*      mAsyncReadStream; // non-owning pointer
    PRUint32                     mStartOffset;
    nsresult                     mStatus;

    friend class MemCacheWriteStreamWrapper;
    friend class AsyncReadStreamAdaptor;
};

#endif // _nsMemCacheChannel_h_
