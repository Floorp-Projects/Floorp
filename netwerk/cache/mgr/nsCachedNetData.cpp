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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Furman, fur@netscape.com
 */

#include "nsISupportsUtils.h"
#include "nsCachedNetData.h"
#include "nsCacheManager.h"
#include "nsCacheEntryChannel.h"
#include "nsINetDataCache.h"
#include "nsIStreamListener.h"
#include "nsIStreamAsFile.h"
#include "nsIStorageStream.h"
#include "nsIStringStream.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsISupportsArray.h"
#include "nsIArena.h"
#include "nsCRT.h"
#include "nsString.h"

extern PRBool gCacheManagerNeedToEvict;
extern nsCacheManager* gCacheManager;
// Version of the cache record meta-data format.  If this version doesn't match
// the one in the database, an error is signaled when the record is read.
#define CACHE_MANAGER_VERSION 1

// Other than nsISupports methods, no public methods can operate on a deleted
// cache entry or one that has been Release'ed by everyone but the cache manager.
#define CHECK_AVAILABILITY()                                                  \
{                                                                             \
    if (GetFlag((Flag)(RECYCLED | DORMANT))) {                                \
        NS_ASSERTION(0, "Illegal cache entry state");                         \
        return NS_ERROR_NOT_AVAILABLE;                                        \
    }                                                                         \
}

class StDeallocator
{
public:
	StDeallocator( void* memory): mMemory( memory ){};
	~StDeallocator()
	{
		if (mMemory)
        nsMemory::Free(mMemory);
	 }
private:
	void* mMemory;
};

// Placement new for arena-allocation of nsCachedNetData instances
void *
nsCachedNetData::operator new (size_t aSize, nsIArena *aArena)
{
    nsCachedNetData* entry = (nsCachedNetData*)aArena->Alloc(aSize);
    if (!entry)
        return 0;
    nsCRT::zero(entry, aSize);
    return entry;
}

// Placement new for recycling of arena-based nsCachedNetData
// instances, clears all instance variables.
void *
nsCachedNetData::operator new (size_t aSize, nsCachedNetData *aEntry)
{
    nsCRT::zero(aEntry, aSize);
    return aEntry;
}

// One element in a linked list of nsIStreamAsFileObserver's
class StreamAsFileObserverClosure
{
public:
    StreamAsFileObserverClosure( /* nsIStreamAsFile *aStreamAsFile,*/
                                nsIStreamAsFileObserver *aObserver):
        /* mStreamAsFile(aStreamAsFile),*/ mObserver(aObserver), mNext(0) {}

    ~StreamAsFileObserverClosure() { delete mNext; }

    // Weak link to nsIStreamAsFile which, indirectly, holds a strong link
    //   to this instance
 //   nsIStreamAsFile*           mStreamAsFile;
    nsIStreamAsFileObserver*   mObserver;        

    // Link to next in list
    StreamAsFileObserverClosure* mNext;
};

// nsIStreamAsFile is implemented as an XPCOM tearoff to avoid the cost of an
//   extra vtable pointer in nsCachedNetData
class StreamAsFile : public nsIStreamAsFile {
public:    
    StreamAsFile(nsCachedNetData* cacheEntry): mCacheEntry(cacheEntry) {
        NS_IF_ADDREF(mCacheEntry);
        NS_INIT_REFCNT();
    }
    virtual ~StreamAsFile() {
        NS_IF_RELEASE(mCacheEntry);
    };

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetFile(nsIFile* *aFileSpec) {
        return mCacheEntry->GetFile(aFileSpec);
    }

    NS_IMETHOD AddObserver(nsIStreamAsFileObserver *aObserver) {
        return mCacheEntry->AddObserver(this, aObserver);
    }
    
    NS_IMETHOD RemoveObserver(nsIStreamAsFileObserver *aObserver) {
        return mCacheEntry->RemoveObserver(aObserver);
    } 

protected:
    nsCachedNetData* mCacheEntry;
};

NS_IMPL_ADDREF(StreamAsFile)
NS_IMPL_RELEASE(StreamAsFile)

// QueryInterface delegates back to the nsICachedNetData that spawned this instance
NS_IMETHODIMP
StreamAsFile::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
    if ( !aInstancePtr )
        return NS_ERROR_NULL_POINTER;
    nsISupports* foundInterface;

    if ( aIID.Equals(NS_GET_IID(nsIStreamAsFile)) ) {
        foundInterface = NS_STATIC_CAST(nsIStreamAsFile*, this);
        NS_ADDREF(foundInterface);
        *aInstancePtr = foundInterface;
        return NS_OK;
    } else {
        return mCacheEntry->QueryInterface(aIID, aInstancePtr);
    }
}

// External clients of the cache can attach tagged chunks of meta-data to each
// cache-entry, which are stored in instances of this class.  The tag is a
// NUL-terminated string.
class CacheMetaData {
    CacheMetaData(const char *aTag):
	    mTag(nsCRT::strdup(aTag)), mOpaqueBytes(0), mLength(0), mNext(0) {}

    ~CacheMetaData() {
    		if( mTag )
    			nsMemory::Free( mTag );
    			
        if (mOpaqueBytes)
            nsMemory::Free(mOpaqueBytes);
        delete mNext;
    }

protected:
    nsresult Set(PRUint32 aLength, const char* aOpaqueBytes) {
        char* newOpaqueBytes = 0;
        if (aOpaqueBytes) {
            newOpaqueBytes = (char*)nsMemory::Alloc(aLength);
            if (!newOpaqueBytes)
                return NS_ERROR_OUT_OF_MEMORY;
            memcpy(newOpaqueBytes, aOpaqueBytes, aLength);
        }
        
        if (mOpaqueBytes)
            nsMemory::Free(mOpaqueBytes);

        mOpaqueBytes = newOpaqueBytes;
        mLength = aLength;
        return NS_OK;
    }

    nsresult Get(PRUint32 *aLength, char* *aOpaqueBytes) {
        char *copyOpaqueBytes = 0;
        if (mOpaqueBytes) {
            copyOpaqueBytes = (char*)nsMemory::Alloc(mLength);
            if (!copyOpaqueBytes)
                return NS_ERROR_OUT_OF_MEMORY;
            memcpy(copyOpaqueBytes, mOpaqueBytes, mLength);
        }

        *aOpaqueBytes = copyOpaqueBytes;
        *aLength = mLength;
    
        return NS_OK;
    }

    char*			      mTag;           // Descriptive tag, e.g. "http headers"
    char*           mOpaqueBytes;   // The meta-data itself
    PRUint32        mLength;        // Length of mOpaqueBytes
    CacheMetaData*  mNext;          // Next in chain for this cache entry

    friend class nsCachedNetData;
};

NS_IMPL_ADDREF(nsCachedNetData)
NS_IMETHODIMP
nsCachedNetData::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
    if ( !aInstancePtr )
        return NS_ERROR_NULL_POINTER;
    nsISupports* foundInterface;

    if ( aIID.Equals(NS_GET_IID(nsICachedNetData)) ) {
         foundInterface = NS_STATIC_CAST(nsICachedNetData*, this);
    } else if ( aIID.Equals(NS_GET_IID(nsIStreamAsFile)) ) {
         foundInterface = new StreamAsFile(this);
         if (!foundInterface)
             return NS_ERROR_OUT_OF_MEMORY;
    } else if ( aIID.Equals(NS_GET_IID(nsISupports)) ) {
        foundInterface = NS_STATIC_CAST(nsISupports*, this);
    } else {
        foundInterface = 0;
    }

    nsresult status;
    if ( !foundInterface )
        status = NS_NOINTERFACE;
    else {
        NS_ADDREF(foundInterface);
        status = NS_OK;
    }
    *aInstancePtr = foundInterface;
    return status;
}

// A customized version of Release() that slims down the memory consumption
// when the ref-count drops to one, i.e. only the cache manager holds a
// reference to the cache entry.
NS_IMETHODIMP_(nsrefcnt)
nsCachedNetData::Release(void)
{
    nsrefcnt count;
    NS_PRECONDITION(1 != mRefCnt, "dup release");
    count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
    NS_LOG_RELEASE(this, count, "nsCachedNetData");
    if (count == 1) {

        nsCacheManager::NoteDormant(this);
        
        if (!GetFlag(RECYCLED)) {
            // Clear flag, in case the protocol handler forgot to
            mFlags &= ~UPDATE_IN_PROGRESS;

            // First, flush any altered cache entry data to the database
            Commit();
        }
        
        SetFlag(DORMANT);
        
        // Free up some storage
        delete mObservers;
        mObservers = 0;
        PRInt32 recordID;
        mRecord->GetRecordID(&recordID);
        NS_RELEASE(mRecord);
        mRecord = 0;
        mRecordID = recordID;
        delete mMetaData;
        mMetaData = 0;
    }
    return count;
}

nsresult 
nsCachedNetData::GetRecord(nsINetDataCacheRecord* *aRecord)
{
    if (GetFlag(DORMANT)) {
        return mCache->GetCachedNetDataByID(mRecordID, aRecord);
    } else {
        NS_ADDREF(mRecord);
        *aRecord = mRecord;
        return NS_OK;
    }
}

nsresult 
nsCachedNetData::GetRecordID(PRInt32 *aRecordID)
{
    if (GetFlag(DORMANT)) {
        *aRecordID = mRecordID;
        return NS_OK;
    } else {
        return mRecord->GetRecordID(aRecordID);
    }
}

// Convert PRTime to unix-style time_t, i.e. seconds since the epoch
static PRUint32
convertPRTimeToSeconds(PRTime aTime64)
{
    double fpTime;
    LL_L2D(fpTime, aTime64);
    return (PRUint32)(fpTime * 1e-6 + 0.5);
}

// Convert unix-style time_t, i.e. seconds since the epoch, to PRTime
static PRTime
convertSecondsToPRTime(PRUint32 aSeconds)
{
    PRInt64 t64;
    LL_I2L(t64, aSeconds);
    PRInt64 mil;
    LL_I2L(mil, 1000000);
    LL_MUL(t64, t64, mil);
    return t64;
}

void
nsCachedNetData::NoteAccess()
{
    PRUint32 now;

    now = convertPRTimeToSeconds(PR_Now());

    // Don't record accesses that occur more than once per second
    if (mAccessTime[0] == now)
        return;

    // Saturate access count at 16-bit limit
    if (mNumAccesses < 0xFF)
        mNumAccesses++;

    // Update array of recent access times
    for (int i = MAX_K - 1; i >= 1; i--) {
        mAccessTime[i] = mAccessTime[i - 1];
    }
    mAccessTime[0] = now;

    SetDirty();
}

void
nsCachedNetData::NoteUpdate()
{
    ClearFlag(VESTIGIAL);

    // We only keep track of last-modified time or update time, not both
    if (GetFlag(LAST_MODIFIED_KNOWN))
        return;
    mLastUpdateTime = convertPRTimeToSeconds(PR_Now());
    SetDirty();
}

nsresult
nsCachedNetData::Init(nsINetDataCacheRecord *aRecord, nsINetDataCache *aCache)
{
    mRecord = aRecord;
    NS_ADDREF(aRecord);
    mCache = aCache;
    
    return Deserialize(PR_TRUE);
}

nsresult
nsCachedNetData::Resurrect(nsINetDataCacheRecord *aRecord)
{
    ClearFlag(DORMANT);
    mRecord = aRecord;
    NS_ADDREF(aRecord);
    
    return Deserialize(PR_TRUE);
}

// Set a boolean flag for the cache entry
nsresult 
nsCachedNetData::SetFlag(PRBool aValue, Flag aFlag)
{
    if (mFlags & RECYCLED)
        return NS_ERROR_NOT_AVAILABLE;
    NS_ASSERTION(aValue == 1 || aValue == 0, "Illegal argument");
    PRUint16 newFlags;
    newFlags = mFlags & ~aFlag;
    newFlags |= (-aValue & aFlag);

    // Mark record as dirty if any non-transient flag has changed
    if ((newFlags & ~TRANSIENT_FLAGS) != (mFlags & ~TRANSIENT_FLAGS)) {
        newFlags |= DIRTY;
    }
        
    mFlags = newFlags;

    // Once a record has become dormant, only its flags can change
    if ((newFlags & DIRTY) && (newFlags & DORMANT))
        CommitFlags();

    return NS_OK;
}

nsresult
nsCachedNetData::CommitFlags()
{
    nsresult rv;
    nsCachedNetData* thisAlias = this;

    NS_ASSERTION(GetFlag(DORMANT) && GetFlag(DIRTY),
                 "Invalid state");

    nsCOMPtr<nsINetDataCacheRecord> record;
    rv = GetRecord(getter_AddRefs(record));
    if (NS_FAILED(rv)) return rv;

    PRUint16 saveFlags = mFlags;

    rv = Resurrect(record);
    if (NS_FAILED(rv)) return rv;
    NS_ADDREF(thisAlias);

    mFlags = saveFlags &~ DORMANT;
    
    NS_RELEASE(thisAlias);
    return rv;
}

// Retrieve the opaque meta-data stored in the cache database record, and
// extract its components, namely the protocol-specific meta-data and the
// protocol-independent cache manager meta-data.
nsresult
nsCachedNetData::Deserialize(PRBool aDeserializeFlags)
{
    nsresult rv;
    PRUint32 metaDataLength;
    char* metaData;

    // Either this is the first time the cache entry has ever been read or
    // we're resurrecting a record that has been previously serialized to the
    // cache database.
    
    nsCOMPtr<nsINetDataCacheRecord> record;
    rv = GetRecord(getter_AddRefs(record));
    if (NS_FAILED(rv)) return rv;

    rv = record->GetMetaData(&metaDataLength, &metaData);
    if (NS_FAILED(rv)) return rv;

    // No meta-data means a virgin record
    if (!metaDataLength)
        return NS_OK;
#if 0
    nsCString metaDataCStr(metaData, metaDataLength);
    if (metaData)
        nsMemory::Free(metaData);

    nsCOMPtr<nsISupports> stringStreamSupports;
    rv = NS_NewCStringInputStream(getter_AddRefs(stringStreamSupports), metaDataCStr);
    if (NS_FAILED(rv)) return rv;
#else
		nsCOMPtr<nsISupports> stringStreamSupports;
		StDeallocator metaDataDeallocator( metaData );
		 rv = NS_NewByteInputStream(getter_AddRefs(stringStreamSupports), metaData, metaDataLength);
    if (NS_FAILED(rv)) return rv;
#endif
    nsCOMPtr<nsIInputStream> inputStream = do_QueryInterface(stringStreamSupports);

    nsCOMPtr<nsIBinaryInputStream> binaryStream;
    rv = NS_NewBinaryInputStream(getter_AddRefs(binaryStream), inputStream);
    if (NS_FAILED(rv)) return rv;

    // Verify that the record meta-data was serialized by this version of the
    // cache manager code.
    PRUint8 version;
    rv = binaryStream->Read8(&version);
    if (NS_FAILED(rv)) return rv;
    if (version != CACHE_MANAGER_VERSION)
        return NS_ERROR_FAILURE;

    while (1) {
        char* tag;
        rv = binaryStream->ReadStringZ(&tag);
        
        if (NS_FAILED(rv)) return rv;

				StDeallocator deallocator( tag );
        // Last meta-data chunk is indicated by empty tag
        if (*tag == 0)
            break;

        CacheMetaData *annotation;
        annotation = new CacheMetaData(tag);
        
        if (!annotation)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = binaryStream->Read32(&annotation->mLength);
        if (NS_FAILED(rv)) return rv;
    
        rv = binaryStream->ReadBytes(&annotation->mOpaqueBytes, annotation->mLength);
        if (NS_FAILED(rv)) return rv;

        annotation->mNext = mMetaData;
        mMetaData = annotation;
    }

    PRUint16 flags;
    rv = binaryStream->Read16(&flags);
    if (NS_FAILED(rv)) return rv;
    if (aDeserializeFlags)
        mFlags = flags;

    rv = binaryStream->Read8(&mNumAccesses);
    if (NS_FAILED(rv)) return rv;

    for (int i = 0; i < MAX_K; i++) {
        rv = binaryStream->Read32(&mAccessTime[i]);
        if (NS_FAILED(rv)) return rv;
    }

    rv = binaryStream->Read32(&mLastUpdateTime);
    if (NS_FAILED(rv)) return rv;

    rv = binaryStream->Read32(&mLastModifiedTime);
    if (NS_FAILED(rv)) return rv;

    rv = binaryStream->Read32(&mExpirationTime);
    if (NS_FAILED(rv)) return rv;

    rv = binaryStream->ReadFloat(&mDownloadRate);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetUriSpec(char* *aUriSpec)
{
    CHECK_AVAILABILITY();

    char* key;
    PRUint32 keyLength;
    nsresult rv;

    *aUriSpec = 0;

    rv = mRecord->GetKey(&keyLength, &key);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(keyLength >=1, "Bogus record key");

    // The URI spec is stored as the first of the two components that make up
    // the nsINetDataCacheRecord key and is separated from the second component
    // by a NUL character, so we can use plain 'ol strdrup().
    *aUriSpec = nsCRT::strdup(key);

    nsMemory::Free(key);

    if (!*aUriSpec)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetSecondaryKey(PRUint32 *aLength, char **aSecondaryKey)
{
    CHECK_AVAILABILITY();

    char* key;
    char* secondaryKey;
    PRUint32 keyLength;
    nsresult rv;

    *aSecondaryKey = 0;

    rv = mRecord->GetKey(&keyLength, &key);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(keyLength >=1, "Bogus record key");

    // The URI spec is stored as the second of the two components that make up
    // the nsINetDataCacheRecord key and is separated from the first component
    // by a NUL character.
    for(secondaryKey = key; *secondaryKey; secondaryKey++)
        keyLength--;

    // Account for NUL character
    keyLength--;
    secondaryKey++;
    
    if (keyLength) {
        char* copy = (char*)nsMemory::Alloc(keyLength);
        if (!copy) {
            nsMemory::Free(key);
            return NS_ERROR_OUT_OF_MEMORY;
        }
        memcpy(copy, secondaryKey, keyLength);
        *aSecondaryKey = copy;
    }

    nsMemory::Free(key);
    *aLength = keyLength;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetAllowPartial(PRBool *aAllowPartial)
{
    return GetFlag(aAllowPartial, ALLOW_PARTIAL);
}

NS_IMETHODIMP
nsCachedNetData::SetAllowPartial(PRBool aAllowPartial)
{
    return SetFlag(aAllowPartial, ALLOW_PARTIAL);
}

NS_IMETHODIMP
nsCachedNetData::GetPartialFlag(PRBool *aPartial)
{
    return GetFlag(aPartial, TRUNCATED_CONTENT);
}

NS_IMETHODIMP
nsCachedNetData::GetUpdateInProgress(PRBool *aUpdateInProgress)
{
    return GetFlag(aUpdateInProgress, UPDATE_IN_PROGRESS);
}

NS_IMETHODIMP
nsCachedNetData::GetInUse(PRBool *aInUse)
{
    nsresult rv;

    rv = GetUpdateInProgress(aInUse);
    if (NS_FAILED(rv)) return rv;
    if (!*aInUse)
        *aInUse = (mChannelCount != 0);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::SetUpdateInProgress(PRBool aUpdateInProgress)
{
    return SetFlag(aUpdateInProgress, UPDATE_IN_PROGRESS);
}

NS_IMETHODIMP
nsCachedNetData::GetLastModifiedTime(PRTime *aLastModifiedTime)
{
    CHECK_AVAILABILITY();
    NS_ENSURE_ARG_POINTER(aLastModifiedTime);
    if (GetFlag(LAST_MODIFIED_KNOWN))
        *aLastModifiedTime = convertSecondsToPRTime(mLastModifiedTime);
    else
        *aLastModifiedTime = LL_ZERO;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::SetLastModifiedTime(PRTime aLastModifiedTime)
{
    CHECK_AVAILABILITY();
    mLastModifiedTime = convertPRTimeToSeconds(aLastModifiedTime);
    SetDirty();
    SetFlag(LAST_MODIFIED_KNOWN);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetExpirationTime(PRTime *aExpirationTime)
{
    CHECK_AVAILABILITY();
    NS_ENSURE_ARG_POINTER(aExpirationTime);
    if (GetFlag(EXPIRATION_KNOWN))
        *aExpirationTime = convertSecondsToPRTime(mExpirationTime);
    else
        *aExpirationTime = LL_ZERO;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::SetExpirationTime(PRTime aExpirationTime)
{
    CHECK_AVAILABILITY();

    // Only expiration time or stale time can be set, not both
    if (GetFlag(STALE_TIME_KNOWN))
        return NS_ERROR_NOT_AVAILABLE;

    mExpirationTime = convertPRTimeToSeconds(aExpirationTime);
    SetDirty();
    SetFlag(EXPIRATION_KNOWN);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetStaleTime(PRTime *aStaleTime)
{
    CHECK_AVAILABILITY();
    NS_ENSURE_ARG_POINTER(aStaleTime);
    if (GetFlag(STALE_TIME_KNOWN)) {
        *aStaleTime = convertSecondsToPRTime(mStaleTime);
    } else {
        *aStaleTime = LL_ZERO;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::SetStaleTime(PRTime aStaleTime)
{
    CHECK_AVAILABILITY();

    // Only expiration time or stale time can be set, not both
    if (GetFlag(EXPIRATION_KNOWN))
        return NS_ERROR_NOT_AVAILABLE;

    mStaleTime = convertPRTimeToSeconds(aStaleTime);
    SetDirty(); 
    SetFlag(STALE_TIME_KNOWN);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetLastAccessTime(PRTime *aLastAccessTime)
{
    CHECK_AVAILABILITY();
    NS_ENSURE_ARG_POINTER(aLastAccessTime);
    *aLastAccessTime = convertSecondsToPRTime(mAccessTime[0]);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetLastUpdateTime(PRTime *aLastUpdateTime)
{
    CHECK_AVAILABILITY();
    NS_ENSURE_ARG_POINTER(aLastUpdateTime);
    *aLastUpdateTime = convertSecondsToPRTime(mLastUpdateTime);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetNumberAccesses(PRUint16 *aNumberAccesses)
{
    CHECK_AVAILABILITY();
    NS_ENSURE_ARG_POINTER(aNumberAccesses);
    *aNumberAccesses = mNumAccesses;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::Commit(void)
{
    nsresult rv;
    char *serializedData;
    PRUint32 serializedDataLength, bytesRead;
    nsCOMPtr<nsIInputStream> inputStream;
    nsCOMPtr<nsIOutputStream> outputStream;
    nsCOMPtr<nsIBinaryOutputStream> binaryStream;

    if (GetFlag(RECYCLED)) {
        NS_ASSERTION(0, "Illegal cache manager state");
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsINetDataCacheRecord> record;
    rv = GetRecord(getter_AddRefs(record));
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
    NS_ASSERTION(!GetFlag(UPDATE_IN_PROGRESS),
                 "Protocol handler forgot to clear UPDATE_IN_PROGRESS flag");
#endif

    // Check to see if any data changed.  If not, nothing to do.
    if (!GetFlag(DIRTY))
        return NS_OK;

    // Must clear dirty flag early so record is not stored with dirty flag set
    ClearFlag(DIRTY);

    int i;

    nsCOMPtr<nsIStorageStream> storageStream;
    // Init: (block size, maximum length)
    rv = NS_NewStorageStream(256, (PRUint32)-1, getter_AddRefs(storageStream));
    if (NS_FAILED(rv)) goto error;

    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) goto error;

    rv = NS_NewBinaryOutputStream(getter_AddRefs(binaryStream), outputStream);

    // Prepend version of record meta-data to ensure against version mismatches
    rv = binaryStream->Write8(CACHE_MANAGER_VERSION);
    if (NS_FAILED(rv)) goto error;

    CacheMetaData *annotation;
    for (annotation = mMetaData; annotation; annotation = annotation->mNext) {
        rv = binaryStream->WriteStringZ(annotation->mTag);
        if (NS_FAILED(rv)) return rv;

        rv = binaryStream->Write32(annotation->mLength);
        if (NS_FAILED(rv)) return rv;
    
        rv = binaryStream->WriteBytes(annotation->mOpaqueBytes, annotation->mLength);
        if (NS_FAILED(rv)) return rv;
    }
    // Write terminating null for last meta-data chunk
    rv = binaryStream->WriteStringZ("");
    if (NS_FAILED(rv)) return rv;

    rv = binaryStream->Write16(mFlags);
    if (NS_FAILED(rv)) goto error;

    rv = binaryStream->Write8(mNumAccesses);
    if (NS_FAILED(rv)) goto error;

    for (i = 0; i < MAX_K; i++) {
        rv = binaryStream->Write32(mAccessTime[i]);
        if (NS_FAILED(rv)) goto error;
    }

    rv = binaryStream->Write32(mLastUpdateTime);
    if (NS_FAILED(rv)) goto error;

    rv = binaryStream->Write32(mLastModifiedTime);
    if (NS_FAILED(rv)) goto error;

    rv = binaryStream->Write32(mExpirationTime);
    if (NS_FAILED(rv)) goto error;

    rv = binaryStream->WriteFloat(mDownloadRate);
    if (NS_FAILED(rv)) goto error;

    rv = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
    if (NS_FAILED(rv)) goto error;

    inputStream->Available(&serializedDataLength);

    serializedData = (char*)nsMemory::Alloc(serializedDataLength);
    if (!serializedData) goto error;
    inputStream->Read(serializedData, serializedDataLength, &bytesRead);
    if (NS_FAILED(rv)) {
        nsMemory::Free(serializedData);
        goto error;
    }

    rv = record->SetMetaData(serializedDataLength, serializedData);
    nsMemory::Free(serializedData);
    return rv;

 error:
    SetDirty();
    return rv;
}

CacheMetaData*
nsCachedNetData::FindTaggedMetaData(const char* aTag, PRBool aCreate)
{
    CacheMetaData* metaData;
    CacheMetaData** metaDatap;
    metaDatap = &mMetaData;

    while ((metaData = *metaDatap)!= 0) {
        if (!strcmp(aTag, metaData->mTag))
            return metaData;
        metaDatap = &metaData->mNext;
    }
    if (!aCreate)
        return NULL;
    *metaDatap = new CacheMetaData(aTag);
    return *metaDatap;
}

NS_IMETHODIMP
nsCachedNetData::GetAnnotation(const char* aKey,
                               PRUint32* aProtocolDataLength,
                               char* *aProtocolData)
{
    CHECK_AVAILABILITY();

    NS_ENSURE_ARG_POINTER(aProtocolDataLength);
    NS_ENSURE_ARG_POINTER(aProtocolData);

    CacheMetaData* metaData;
    metaData = FindTaggedMetaData(aKey, PR_FALSE);
    if (!metaData) {
			*aProtocolDataLength = 0;
			*aProtocolData = 0;
			return NS_OK;
	}
    
    return metaData->Get(aProtocolDataLength, aProtocolData);
}

NS_IMETHODIMP
nsCachedNetData::SetAnnotation(const char* aTag,
                               PRUint32 aLength,
                               const char *aProtocolData)
{
    nsresult rv;
    CacheMetaData* metaData;

    CHECK_AVAILABILITY();

    NS_ENSURE_ARG(aTag);
    if (!*aTag)
        return NS_ERROR_INVALID_ARG;

    metaData = FindTaggedMetaData(aTag, PR_TRUE);
    if (!metaData)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = metaData->Set(aLength, aProtocolData);

    SetDirty();

    return rv;
}

nsresult
nsCachedNetData::AddObserver(nsIStreamAsFile* aStreamAsFile, nsIStreamAsFileObserver* aObserver) 
{
    StreamAsFileObserverClosure *closure;

    CHECK_AVAILABILITY();
    NS_ENSURE_ARG(aObserver);
    
    closure = new StreamAsFileObserverClosure(/*aStreamAsFile,*/ aObserver);
    if (!closure)
        return NS_ERROR_OUT_OF_MEMORY;

    closure->mNext = mObservers;
    mObservers = closure;
    return NS_OK;
}

nsresult
nsCachedNetData::RemoveObserver(nsIStreamAsFileObserver *aObserver)
{
    StreamAsFileObserverClosure** closurep;
    StreamAsFileObserverClosure* closure;

    CHECK_AVAILABILITY();
    NS_ENSURE_ARG(aObserver);
    if (!mObservers)
        return NS_ERROR_FAILURE;
    
    for (closurep = &mObservers; (closure = *closurep)!=0; closurep = &(*closurep)->mNext) {
        if (closure->mObserver == aObserver) {
            *closurep = closure->mNext;
            closure->mNext = 0;
            delete closure;
        }
    }
    
    return NS_ERROR_FAILURE;
} 

NS_IMETHODIMP
nsCachedNetData::GetStoredContentLength(PRUint32 *aStoredContentLength)
{
    CHECK_AVAILABILITY();
    return mRecord->GetStoredContentLength(aStoredContentLength);
}

NS_IMETHODIMP
nsCachedNetData::SetStoredContentLength(PRUint32 aStoredContentLength)
{
    CHECK_AVAILABILITY();
    return mRecord->SetStoredContentLength(aStoredContentLength);
}

NS_IMETHODIMP
nsCachedNetData::GetLogicalLength(PRUint32 *aLogicalLength)
{
    CHECK_AVAILABILITY();
    if (mLogicalLength)
        *aLogicalLength = mLogicalLength;
    else
        *aLogicalLength = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::GetSecurityInfo (nsISupports ** o_SecurityInfo)
{
    CHECK_AVAILABILITY();
    return mRecord -> GetSecurityInfo (o_SecurityInfo);
}

NS_IMETHODIMP
nsCachedNetData::SetSecurityInfo (nsISupports  * o_SecurityInfo)
{
    CHECK_AVAILABILITY();
    return mRecord -> SetSecurityInfo (o_SecurityInfo);
}

// Notify all cache entry observers
nsresult
nsCachedNetData::Notify(PRUint32 aMessage, nsresult aError)
{
    nsresult rv;
    StreamAsFileObserverClosure *closure;
    nsIStreamAsFileObserver *observer;
    closure = mObservers;
    while (closure) {
        observer = closure->mObserver;
        nsCOMPtr<nsIStreamAsFile> streamAsFile( do_QueryInterface(this) );
        rv = observer->ObserveStreamAsFile( streamAsFile, aMessage, aError);
        if (NS_FAILED(rv)) return rv;
        closure = closure->mNext;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::Delete(void)
{
    if (GetFlag(RECYCLED))
        return NS_OK;

    // Tell observers about the deletion, so that they can release their
    // references to this cache object.
    Notify(nsIStreamAsFileObserver::REQUEST_DELETION, NS_OK);

    // Can only delete if all references are dropped excepting, of course, the
    // one from the cache manager and the caller of this method.
    if (mRefCnt <= 2) {

        nsresult rv;
        nsCOMPtr<nsINetDataCacheRecord> record;

        rv = GetRecord(getter_AddRefs(record));
        if (NS_SUCCEEDED(rv) && record)
        {
            // Delete the record if we can get a record.
            record->Delete();
            // Ignore error from delete as the file might not have been created
            // on disk for performance reasons.
        }

        // Now record is available for recycling
        SetFlag(RECYCLED);
        return NS_OK;
    }
        
    // Unable to delete because cache entry is still active
    return NS_ERROR_FAILURE;
}

// Truncate the content data for a cache entry, so as to make space in the
// cache for incoming data for another entry.
//
// Deals with error case where the underlying recored is non-existent. In such
// a situation, we delete the entry.
nsresult
nsCachedNetData::Evict(PRUint32 aTruncatedContentLength)
{
    nsresult rv = NS_ERROR_FAILURE;
    // Tell observers about the eviction, so that they can release their
    // references to this cache object.
    Notify(nsIStreamAsFileObserver::REQUEST_DELETION, NS_OK);

    // Can only delete if all references are dropped excepting, of course, the
    // one from the cache manager.
    if (mRefCnt == 1) {

        // If the protocol handler for this cache entry can't cope with a
        // truncated record then we might as well blow it away completely to
        // recover the space
        if (!GetFlag(ALLOW_PARTIAL))
            aTruncatedContentLength = 0;

        // Tell the record about the eviction
        nsCOMPtr<nsINetDataCacheRecord> record;
        rv = GetRecord(getter_AddRefs(record));

        if (NS_SUCCEEDED(rv) && record)
        {
            rv = record->SetStoredContentLength(aTruncatedContentLength);
            if (NS_FAILED(rv))
                return rv;
        }

        if (aTruncatedContentLength == 0) {
            SetFlag(VESTIGIAL);
        }
        SetFlag(TRUNCATED_CONTENT);
        return NS_OK;
    }

    // Unable to delete
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedNetData::GetCache(nsINetDataCache* *aCache)
{
    NS_ENSURE_ARG_POINTER(aCache);
    
    *aCache = mCache;
    NS_ADDREF(mCache);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedNetData::NewChannel(nsILoadGroup* aLoadGroup, nsIChannel* *aChannel)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    CHECK_AVAILABILITY();

    rv =  mRecord->NewChannel(0, getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    nsCacheEntryChannel *cacheEntryChannel;
    cacheEntryChannel = new nsCacheEntryChannel(this, channel, aLoadGroup);
    if (!cacheEntryChannel)
        return NS_ERROR_OUT_OF_MEMORY;

    *aChannel = cacheEntryChannel;
    NS_ADDREF(*aChannel);

    return NS_OK;
}

nsresult
nsCachedNetData::GetFile(nsIFile* *aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);
    return mRecord->GetFile( aFile );
}

class InterceptStreamListener : public nsIStreamListener,
                                public nsIInputStream
{
public:

    InterceptStreamListener(nsCachedNetData *aCacheEntry,
                            nsIStreamListener *aOriginalListener):
        mCacheEntry(aCacheEntry), 
        mOriginalListener(aOriginalListener) 
    {
        NS_INIT_REFCNT(); 
        NS_IF_ADDREF(mCacheEntry);
    }
    
    virtual ~InterceptStreamListener() {
        mCacheEntry->ClearFlag(nsCachedNetData::UPDATE_IN_PROGRESS);
        NS_IF_RELEASE(mCacheEntry);
    };

    nsresult Init(PRUint32 aStartingOffset) {
        nsresult rv;

        // Just in case the protocol handler forgot to set this flag...
        mCacheEntry->SetFlag(nsCachedNetData::UPDATE_IN_PROGRESS);

        rv = mCacheEntry->NewChannel(0, getter_AddRefs(mChannel));
        if (NS_FAILED(rv)) return rv;

        rv = mChannel->SetTransferOffset(aStartingOffset);
        if (NS_FAILED(rv)) return rv;

        return mChannel->OpenOutputStream(getter_AddRefs(mCacheStream));
    }

    NS_DECL_ISUPPORTS

    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
        return mOriginalListener->OnStartRequest(channel, ctxt);
    }

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                             nsresult aStatus, const PRUnichar* aStatusArg) {
        if (NS_FAILED(aStatus)) {
            mCacheEntry->SetFlag(nsCachedNetData::TRUNCATED_CONTENT);
        } else {
            mCacheEntry->ClearFlag(nsCachedNetData::TRUNCATED_CONTENT);
        }
    
        mCacheEntry->ClearFlag(nsCachedNetData::VESTIGIAL);
        mCacheEntry->ClearFlag(nsCachedNetData::UPDATE_IN_PROGRESS);
        
        gCacheManager->LimitDiskCacheSize(gCacheManagerNeedToEvict);

        if (mCacheStream )
            mCacheStream->Close();
        // Tell any stream-as-file observers that the file has been completely written
        mCacheEntry->Notify(nsIStreamAsFileObserver::NOTIFY_AVAILABLE, NS_OK);

        return mOriginalListener->OnStopRequest(channel, ctxt, aStatus, aStatusArg);
    }

    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                               nsIInputStream *inStr, PRUint32 sourceOffset,
                               PRUint32 count) {
        mOriginalStream = inStr;
        return mOriginalListener->OnDataAvailable(channel, ctxt, 
                                                  NS_STATIC_CAST(nsIInputStream*, this),
                                                  sourceOffset, count);
    }

    NS_IMETHOD Close(void) {
        return mOriginalStream->Close();
    }

    NS_IMETHOD Available(PRUint32 *aBytesAvailable) {
        return mOriginalStream->Available(aBytesAvailable);
    }

    NS_IMETHOD Read(char* buf, PRUint32 count, PRUint32 *aActualBytes) {
        nsresult rv;
        rv = mOriginalStream->Read(buf, count, aActualBytes);
        if (NS_FAILED(rv)) return rv;

        rv = write(buf, *aActualBytes);
        
        // If the cache fills up, mark entry as partial content
        if (NS_FAILED(rv)) 
            mCacheEntry->SetFlag(nsCachedNetData::TRUNCATED_CONTENT);
        return NS_OK;
    }

    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("ReadSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("GetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {
        NS_NOTREACHED("GetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {
        NS_NOTREACHED("SetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

private:
    nsresult
    write(char* aBuf, PRUint32 aNumBytes) {
        PRUint32 actualBytes;
        return mCacheStream->Write(aBuf, aNumBytes, &actualBytes);
    }

private:
    nsCachedNetData*            mCacheEntry;
    nsCOMPtr<nsIStreamListener>  mOriginalListener;
    nsCOMPtr<nsIOutputStream>    mCacheStream;

    nsCOMPtr<nsIInputStream>     mOriginalStream;
    nsCOMPtr<nsIChannel>         mChannel;
};


NS_IMPL_THREADSAFE_ISUPPORTS3(InterceptStreamListener, nsIInputStream, nsIStreamListener, nsIStreamObserver)

NS_IMETHODIMP
nsCachedNetData::InterceptAsyncRead(nsIStreamListener *aOriginalListener,
                                    PRUint32 aStartingOffset,
                                    nsIStreamListener **aResult)
{
    nsresult rv;
    InterceptStreamListener* interceptListener;

    interceptListener = new InterceptStreamListener(this, aOriginalListener);
    if (!interceptListener)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(interceptListener); // for return
    
    rv = interceptListener->Init(aStartingOffset);
    if (NS_FAILED(rv)) {
        NS_RELEASE(interceptListener);
        return rv;
    }

    *aResult = interceptListener; // addref above
    return NS_OK;
}
