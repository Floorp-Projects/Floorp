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

#ifndef _nsMemCacheRecord_h_
#define _nsMemCacheRecord_h_

#include "nsINetDataCacheRecord.h"
#include "nsIStorageStream.h"
#include "nsCOMPtr.h"

class nsMemCache;

class nsMemCacheRecord : public nsINetDataCacheRecord
{

public:
    // Declare interface methods
    NS_DECL_ISUPPORTS
    NS_DECL_NSINETDATACACHERECORD

protected:
    // Constructors and Destructor
    nsMemCacheRecord();
    virtual ~nsMemCacheRecord();

    nsresult Init(const char *aKey, PRUint32 aKeyLength,
                  PRUint32 aRecordID, nsMemCache *aCache);

    char*       mKey;              // opaque database key for this record
    PRUint32    mKeyLength;        // length, in bytes, of mKey

    PRInt32     mRecordID;         // An alternate key for this record

    char*       mMetaData;         // opaque URI metadata
    PRUint32    mMetaDataLength;   // length, in bytes, of mMetaData

    nsMemCache* mCache;            // weak pointer to the cache database
                                   //  that this record inhabits

    nsCOMPtr<nsIStorageStream> mStorageStream;
    PRUint32    mNumTransports;      // Count un-Release'ed nsITransports

    friend class nsMemCache;
    friend class nsMemCacheTransport;

private:
    nsCOMPtr<nsISupports>   mSecurityInfo;
};

#endif // _nsMemCacheRecord_h_
