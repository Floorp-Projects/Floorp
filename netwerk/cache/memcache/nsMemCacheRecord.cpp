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

#include "nsMemCache.h"
#include "nsMemCacheRecord.h"
#include "nsMemCacheChannel.h"
#include "nsMemory.h"
#include "nsStorageStream.h"

static NS_DEFINE_IID(kINetDataCacheRecord, NS_INETDATACACHERECORD_IID);

nsMemCacheRecord::nsMemCacheRecord()
    : mKey(0), mKeyLength(0), mMetaData(0), mMetaDataLength(0), mNumTransports(0)
{
    NS_INIT_REFCNT();
}

nsMemCacheRecord::~nsMemCacheRecord()
{
    if (mMetaData)
        delete[] mMetaData;
    if (mKey)
        delete[] mKey;
}	
     
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemCacheRecord, nsINetDataCacheRecord)

NS_IMETHODIMP
nsMemCacheRecord::GetKey(PRUint32 *aLength, char **aResult)
{
    NS_ENSURE_ARG(aResult);
    *aResult = (char *)nsMemory::Alloc(mKeyLength + 1);
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    memcpy(*aResult, mKey, mKeyLength);
	(*aResult)[mKeyLength] = '\0';	// null terminate because this is going to be used in a string key
    *aLength = mKeyLength;
    return NS_OK;
}

nsresult
nsMemCacheRecord::Init(const char *aKey, PRUint32 aKeyLength,
                       PRUint32 aRecordID, nsMemCache *aCache)
{
    nsresult rv;

    NS_ASSERTION(!mKey, "Memory cache record key set multiple times");

    rv = NS_NewStorageStream(MEM_CACHE_SEGMENT_SIZE, MEM_CACHE_MAX_ENTRY_SIZE,
                             getter_AddRefs(mStorageStream));
    if (NS_FAILED(rv)) return rv;

    mKey = new char[aKeyLength + 1];
    if (!mKey)
        return NS_ERROR_OUT_OF_MEMORY;
    memcpy(mKey, aKey, aKeyLength);
	mKey[aKeyLength] = '\0';	// null terminate because we're going to use this for a hash key
    mKeyLength = aKeyLength;
    mRecordID = aRecordID;
    mCache = aCache;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::GetRecordID(PRInt32 *aRecordID)
{
    NS_ENSURE_ARG(aRecordID);
    *aRecordID = mRecordID;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::GetMetaData(PRUint32 *aLength, char **aResult)
{
    NS_ENSURE_ARG(aResult);

    *aResult = 0;
    if (mMetaDataLength) {
        *aResult = (char*)nsMemory::Alloc(mMetaDataLength);
        if (!*aResult)
            return NS_ERROR_OUT_OF_MEMORY;
        memcpy(*aResult, mMetaData, mMetaDataLength);
    }
    *aLength = mMetaDataLength;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::SetMetaData(PRUint32 aLength, const char *aData)
{
    if (mMetaData)
        delete[] mMetaData;
    mMetaData = new char[aLength];
    if (!mMetaData)
        return NS_ERROR_OUT_OF_MEMORY;
    memcpy(mMetaData, aData, aLength);
    mMetaDataLength = aLength;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::GetStoredContentLength(PRUint32 *aStoredContentLength)
{
    NS_ENSURE_ARG(aStoredContentLength);
    return mStorageStream->GetLength(aStoredContentLength);
}

NS_IMETHODIMP
nsMemCacheRecord::SetStoredContentLength(PRUint32 aStoredContentLength)
{
    PRUint32 before, after;
    mStorageStream->GetLength(&before);
    nsresult rv = mStorageStream->SetLength(aStoredContentLength);
    if (NS_FAILED(rv)) return rv;
    mStorageStream->GetLength(&after);
    mCache->mOccupancy -= (before - after);
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::GetSecurityInfo (nsISupports ** o_SecurityInfo)
{
    NS_ENSURE_ARG(o_SecurityInfo);
    *o_SecurityInfo = mSecurityInfo;
    if (*o_SecurityInfo)
        NS_ADDREF (*o_SecurityInfo);

    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::SetSecurityInfo (nsISupports  * o_SecurityInfo)
{
    mSecurityInfo = o_SecurityInfo;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCacheRecord::Delete(void)
{
    if (mNumTransports)
        return NS_ERROR_NOT_AVAILABLE;
  
    return mCache->Delete(this);
}

NS_IMETHODIMP
nsMemCacheRecord::GetFile(nsIFile* *aFilename)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemCacheRecord::NewTransport(nsILoadGroup *aLoadGroup, nsITransport* *aResult)
{
    NS_ENSURE_ARG(aResult);

    nsMemCacheTransport* transport = new nsMemCacheTransport(this, aLoadGroup);
    if (!transport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(transport);
    *aResult = NS_STATIC_CAST(nsITransport*, transport);
    return NS_OK;

}
