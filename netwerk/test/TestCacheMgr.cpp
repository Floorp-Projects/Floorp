/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIStreamListener.h"
#include "nsIStreamObserver.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"

#include "nsINetDataCache.h"
#include "nsINetDataCacheManager.h"
#include "nsICachedNetData.h"

#include "nsCRT.h"
// Number of test entries to be placed in the cache
// FIXME - temporary
#define NUM_CACHE_ENTRIES  25

// Cache content stream length will have random length between zero and
// MAX_CONTENT_LENGTH bytes
#define MAX_CONTENT_LENGTH 20000

// Limits, converted to KB
#define DISK_CACHE_CAPACITY  ((MAX_CONTENT_LENGTH * 4) >> 10)
#define MEM_CACHE_CAPACITY   ((MAX_CONTENT_LENGTH * 3) >> 10)

// Length of random-data cache entry URI key
#define CACHE_KEY_LENGTH 13

// Length of random-data cache entry secondary key
#define CACHE_SECONDARY_KEY_LENGTH 10

// Length of random-data cache entry meta-data
#define CACHE_PROTOCOL_PRIVATE_LENGTH 10

// Mapping from test case number to RecordID
static PRInt32 recordID[NUM_CACHE_ENTRIES];

static PRInt32
mapRecordIdToTestNum(PRInt32 aRecordID)
{
    int i;
    for (i = 0; i < NUM_CACHE_ENTRIES; i++) {
        if (recordID[i] == aRecordID)
            return i;
    }
    return -1;
}

// A supply of stream data to either store or compare with
class nsITestDataStream {
public:
    virtual ~nsITestDataStream() {};
    virtual PRUint32 Next() = 0;
    virtual void Read(char* aBuf, PRUint32 aCount) = 0;
    virtual void ReadString(char* aBuf, PRUint32 aCount) = 0;

    virtual PRBool Match(char* aBuf, PRUint32 aCount) = 0;
    virtual PRBool MatchString(char* aBuf, PRUint32 aCount) = 0;
    virtual void Skip(PRUint32 aCount) = 0;
    virtual void SkipString(PRUint32 aCount) = 0;
};

// A reproducible stream of random data.
class RandomStream : public nsITestDataStream {
public:
    RandomStream(PRUint32 aSeed) {
        mStartSeed = mState = aSeed;
    }
    
    PRUint32 GetStartSeed() {
        return mStartSeed;
    }
    
    PRUint32 Next() {
        mState = 1103515245 * mState + 12345 ^ (mState >> 16);
        return mState;
    }

    PRUint8 NextChar() {
        PRUint8 c;
        do {
            c = Next();
        } while (!isalnum(c));
        return c;
    }

    void Read(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            *aBuf++ = Next();
        }
    }

    // Same as Read(), but using only printable chars and
    //   with a terminating NUL
    void ReadString(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            *aBuf++ = NextChar();
        }
        *aBuf = 0;
    }

    PRBool
    Match(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            if (*aBuf++ != (char)(Next() & 0xff))
                return PR_FALSE;
        }
        return PR_TRUE;
    }

    PRBool
    MatchString(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            if (*aBuf++ != (char)(NextChar() & 0xff))
                return PR_FALSE;
        }
        
        // Check for terminating NUL character
        if (*aBuf)
            return PR_FALSE;
        return PR_TRUE;
    }

    void
    Skip(PRUint32 aCount) {
        while (aCount--)
            Next();
    }

    void
    SkipString(PRUint32 aCount) {
        while (aCount--)
            NextChar();
    }

protected:
    
    PRUint32 mState;
    PRUint32 mStartSeed;
};

static int gNumReaders = 0;
static PRUint32 gTotalBytesRead = 0;
static PRUint32 gTotalBytesWritten = 0;
static PRUint32 gTotalDuration = 0;

class nsReader : public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    nsReader()
        : mStartTime(0), mBytesRead(0)
    {
        NS_INIT_REFCNT();
        gNumReaders++;
    }

    virtual ~nsReader() {
        delete mTestDataStream;
        gNumReaders--;
    }

    nsresult 
    Init(nsITestDataStream* aRandomStream, PRUint32 aExpectedStreamLength) {
        mTestDataStream = aRandomStream;
        mExpectedStreamLength = aExpectedStreamLength;
        mRefCnt = 1;
        return NS_OK;
    }

    NS_IMETHOD OnStartRequest(nsIChannel* channel,
                              nsISupports* context) {
        mStartTime = PR_IntervalNow();
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIChannel* channel, 
                               nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength) {
        char buf[1025];
        while (aLength > 0) {
            PRUint32 amt;
            PRBool match;
            aIStream->Read(buf, sizeof buf, &amt);
            if (amt == 0) break;
            aLength -= amt;
            mBytesRead += amt;
            match = mTestDataStream->Match(buf, amt);
            NS_ASSERTION(match, "Stored data was corrupted on read");
        }
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                             nsresult aStatus, const PRUnichar* aStatusArg) {
        PRIntervalTime endTime;
        PRIntervalTime duration;
        
        endTime = PR_IntervalNow();
        duration = (endTime - mStartTime);

        if (NS_FAILED(aStatus)) printf("channel failed.\n");
        //        printf("read %d bytes\n", mBytesRead);

        //FIXME        NS_ASSERTION(mBytesRead == mExpectedStreamLength,
        // "Stream in cache is wrong length");

        gTotalBytesRead += mBytesRead;
        gTotalDuration += duration;

        return NS_OK;
    }

protected:
    PRIntervalTime       mStartTime;
    PRUint32             mBytesRead;
    nsITestDataStream*   mTestDataStream;
    PRUint32             mExpectedStreamLength;
};

NS_IMPL_ISUPPORTS2(nsReader, nsIStreamListener, nsIStreamObserver)

static nsIEventQueue* eventQueue;

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

nsresult
InitQueue() {
    nsresult rv;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get event queue service");

    rv = eventQService->CreateThreadEventQueue();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create event queue");
  
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQueue);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get event queue for main thread");

    return NS_OK;
}

// Process events until all streams are OnStopRequest'ed
nsresult
WaitForEvents() {
    while (gNumReaders) {
        eventQueue->ProcessPendingEvents();
    }
    return NS_OK;
}

// Read data for a single cache record and compare against testDataStream
nsresult
TestReadStream(nsICachedNetData *cacheEntry, nsITestDataStream *testDataStream,
               PRUint32 expectedStreamLength)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv;
    PRUint32 actualContentLength;

    rv = cacheEntry->NewChannel(0, getter_AddRefs(channel));
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");

    rv = cacheEntry->GetStoredContentLength(&actualContentLength);
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");
    // FIXME    NS_ASSERTION(actualContentLength == expectedStreamLength,
    //                 "nsICachedNetData::GetContentLength() busted ?");
    
    nsReader *reader = new nsReader;
    reader->AddRef();
    rv = reader->Init(testDataStream, expectedStreamLength);
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");
    
    rv = channel->AsyncRead(0, reader);
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");
    reader->Release();

    return NS_OK;
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
    LL_L2I(t64, aSeconds);
    LL_MUL(t64, t64, 1000000);
    return t64;
}

// Read the test data that was written in FillCache(), checking for
// corruption, truncation.
nsresult
TestRead(nsINetDataCacheManager *aCache, PRUint32 aFlags)
{
    nsresult rv;
    PRBool inCache;
    nsCOMPtr<nsICachedNetData> cacheEntry;
    RandomStream *randomStream;
    char uriCacheKey[CACHE_KEY_LENGTH];
    char secondaryCacheKey[CACHE_SECONDARY_KEY_LENGTH];
    char *storedUriKey;
    PRUint32 testNum;

    gTotalBytesRead = 0;
    gTotalDuration = 0;
    for (testNum = 0; testNum < NUM_CACHE_ENTRIES; testNum++) {
        randomStream = new RandomStream(testNum);
        randomStream->ReadString(uriCacheKey, sizeof uriCacheKey - 1);
        randomStream->Read(secondaryCacheKey, sizeof secondaryCacheKey);

        // Ensure that entry is in the cache
        rv = aCache->Contains(uriCacheKey,
                              secondaryCacheKey, sizeof secondaryCacheKey,
                              aFlags, &inCache);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(inCache, "nsINetDataCacheManager::Contains error");
        
        rv = aCache->GetCachedNetData(uriCacheKey,
                                      secondaryCacheKey, sizeof secondaryCacheKey,
                                      aFlags,
                                      getter_AddRefs(cacheEntry));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        // Test GetUriSpec() method
        rv = cacheEntry->GetUriSpec(&storedUriKey);
        NS_ASSERTION(NS_SUCCEEDED(rv) &&
                     !memcmp(storedUriKey, &uriCacheKey[0], sizeof uriCacheKey),
                     "nsICachedNetData::GetKey failed");
        nsMemory::Free(storedUriKey);

        // Test GetSecondaryKey() method
        PRUint32 storedSecondaryKeyLength;
        char* storedSecondaryKey;
        rv = cacheEntry->GetSecondaryKey(&storedSecondaryKeyLength, &storedSecondaryKey);
        NS_ASSERTION(NS_SUCCEEDED(rv) &&
                     !memcmp(storedSecondaryKey, &secondaryCacheKey[0],
                             sizeof secondaryCacheKey),
                     "nsICachedNetData::GetSecondaryKey failed");

        // Compare against stored protocol data
        char *storedProtocolData;
        PRUint32 storedProtocolDataLength;
        rv = cacheEntry->GetAnnotation("test data", &storedProtocolDataLength, &storedProtocolData);
        NS_ASSERTION(NS_SUCCEEDED(rv) &&
                     storedProtocolDataLength == CACHE_PROTOCOL_PRIVATE_LENGTH,
                     "nsICachedNetData::GetAnnotation() failed");
        randomStream->Match(storedProtocolData, storedProtocolDataLength);

        // Test GetAllowPartial()
        PRBool allowPartial;
        rv = cacheEntry->GetAllowPartial(&allowPartial);
        NS_ASSERTION(NS_SUCCEEDED(rv) &&
                     (allowPartial == (PRBool)(randomStream->Next() & 1)),
                     "nsICachedNetData::GetAllowPartial() failed");

        // Test GetExpirationTime()
        PRTime expirationTime;
        PRTime expectedExpirationTime = convertSecondsToPRTime(randomStream->Next() & 0xffffff);
        rv = cacheEntry->GetExpirationTime(&expirationTime);

        NS_ASSERTION(NS_SUCCEEDED(rv) && LL_EQ(expirationTime, expectedExpirationTime),
                     "nsICachedNetData::GetExpirationTime() failed");

        PRUint32 expectedStreamLength = randomStream->Next() % MAX_CONTENT_LENGTH;

        TestReadStream(cacheEntry, randomStream, expectedStreamLength);
    }

    WaitForEvents();

    // Compute rate in MB/s
    double rate = gTotalBytesRead / PR_IntervalToMilliseconds(gTotalDuration);
    rate *= NUM_CACHE_ENTRIES;
    rate *= 1000;
    rate /= (1024 * 1024);
    printf("Read  %7d bytes at a rate of %5.1f MB per second \n",
           gTotalBytesRead, rate);

    return NS_OK;
}

// Create entries in the network data cache, using random data for the
// key, the meta-data and the stored content data.
nsresult
FillCache(nsINetDataCacheManager *aCache, PRUint32 aFlags, PRUint32 aCacheCapacity)
{
    nsresult rv;
    PRBool inCache;
    nsCOMPtr<nsICachedNetData> cacheEntry;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIOutputStream> outStream;
    nsCOMPtr<nsINetDataCache> containingCache;
    char buf[1000];
    PRUint32 protocolDataLength;
    char cacheKey[CACHE_KEY_LENGTH];
    char secondaryCacheKey[CACHE_SECONDARY_KEY_LENGTH];
    char protocolData[CACHE_PROTOCOL_PRIVATE_LENGTH];
    PRUint32 testNum;
    RandomStream *randomStream;

    gTotalBytesWritten = 0;
    PRIntervalTime startTime = PR_IntervalNow();
    
    for (testNum = 0; testNum < NUM_CACHE_ENTRIES; testNum++) {
        randomStream = new RandomStream(testNum);
        randomStream->ReadString(cacheKey, sizeof cacheKey - 1);
        randomStream->Read(secondaryCacheKey, sizeof secondaryCacheKey);

        // No entry should be in cache until we add it
        rv = aCache->Contains(cacheKey,
                              secondaryCacheKey, sizeof secondaryCacheKey,
                              aFlags, &inCache);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(!inCache, "nsINetDataCacheManager::Contains error");
        
        rv = aCache->GetCachedNetData(cacheKey,
                                      secondaryCacheKey, sizeof secondaryCacheKey,
                                      aFlags,
                                      getter_AddRefs(cacheEntry));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't access cacheEntry via cache key");

        // Test nsINetDataCacheManager::GetNumEntries()
        PRUint32 numEntries = (PRUint32)-1;
        rv = aCache->GetNumEntries(&numEntries);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get number of cache entries");
        NS_ASSERTION(numEntries == testNum + 1, "GetNumEntries failure");

        // Record meta-data should be initially empty
        char *protocolDatap;
        rv = cacheEntry->GetAnnotation("test data", &protocolDataLength, &protocolDatap);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        if ((protocolDataLength != 0) || (protocolDatap != 0))
            return NS_ERROR_FAILURE;

        // Store random data as meta-data
        randomStream->Read(protocolData, sizeof protocolData);
        cacheEntry->SetAnnotation("test data", sizeof protocolData, protocolData);

        // Store random data as allow-partial flag
        PRBool allowPartial = randomStream->Next() & 1;
        rv = cacheEntry->SetAllowPartial(allowPartial);
        NS_ASSERTION(NS_SUCCEEDED(rv),
                     "nsICachedNetData::SetAllowPartial() failed");

        // Store random data as expiration time
        PRTime expirationTime = convertSecondsToPRTime(randomStream->Next() & 0xffffff);
        rv = cacheEntry->SetExpirationTime(expirationTime);
        NS_ASSERTION(NS_SUCCEEDED(rv),
                     "nsICachedNetData::SetExpirationTime() failed");

        // Cache manager complains if expiration set without setting last-modified time
        rv = cacheEntry->SetLastModifiedTime(expirationTime);

        rv = cacheEntry->NewChannel(0, getter_AddRefs(channel));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        rv = cacheEntry->GetCache(getter_AddRefs(containingCache));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        rv = channel->OpenOutputStream(getter_AddRefs(outStream));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        
        int streamLength = randomStream->Next() % MAX_CONTENT_LENGTH;
        int remaining = streamLength;
        while (remaining) {
            PRUint32 numWritten;
            int amount = PR_MIN(sizeof buf, remaining);
            randomStream->Read(buf, amount);

            rv = outStream->Write(buf, amount, &numWritten);
            NS_ASSERTION(NS_SUCCEEDED(rv), " ");
            NS_ASSERTION(numWritten == (PRUint32)amount, "Write() bug?");
            
            remaining -= amount;

            PRUint32 storageInUse;
            rv = containingCache->GetStorageInUse(&storageInUse);
            NS_ASSERTION(NS_SUCCEEDED(rv) && (storageInUse <= aCacheCapacity),
                         "Cache manager failed to limit cache growth");
        }
        outStream->Close();
        gTotalBytesWritten += streamLength;

        // *Now* there should be an entry in the cache
        rv = aCache->Contains(cacheKey,
                              secondaryCacheKey, sizeof secondaryCacheKey,
                              aFlags, &inCache);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(inCache, "nsINetDataCacheManager::Contains error");

        delete randomStream;
    }

    PRIntervalTime endTime = PR_IntervalNow();

    // Compute rate in MB/s
    double rate = gTotalBytesWritten / PR_IntervalToMilliseconds(endTime - startTime);
    rate *= 1000;
    rate /= (1024 * 1024);
    printf("Wrote %7d bytes at a rate of %5.1f MB per second \n",
           gTotalBytesWritten, rate);

    return NS_OK;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                                 NULL /* default */);
  return rv;
}

nsresult
Test(nsINetDataCacheManager *aCache, PRUint32 aFlags, PRUint32 aCacheCapacity)
{
    nsresult rv;

    rv = aCache->RemoveAll();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't clear cache");

    PRUint32 numEntries = (PRUint32)-1;
    rv = aCache->GetNumEntries(&numEntries);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get number of cache entries");
    NS_ASSERTION(numEntries == 0, "Couldn't clear cache");

    rv = FillCache(aCache, aFlags, aCacheCapacity);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't fill cache with random test data");

    rv = TestRead(aCache, aFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't read random test data from cache");

    rv = aCache->RemoveAll();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't clear cache");

    rv = aCache->GetNumEntries(&numEntries);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get number of cache entries");
    NS_ASSERTION(numEntries == 0, "Couldn't clear cache");

    return 0;
}
int
main(int argc, char* argv[])
{
    nsresult rv;
    nsCOMPtr<nsINetDataCacheManager> cache;

    rv = NS_AutoregisterComponents();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't register XPCOM components");

    rv = nsComponentManager::CreateInstance(NS_NETWORK_CACHE_MANAGER_CONTRACTID,
                                            nsnull,
                                            NS_GET_IID(nsINetDataCacheManager),
                                            getter_AddRefs(cache));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create cache manager factory") ;

    cache->SetDiskCacheCapacity(DISK_CACHE_CAPACITY);
    cache->SetMemCacheCapacity(MEM_CACHE_CAPACITY);

    InitQueue();
    
    Test(cache, nsINetDataCacheManager::BYPASS_PERSISTENT_CACHE, MEM_CACHE_CAPACITY);
    Test(cache, nsINetDataCacheManager::BYPASS_MEMORY_CACHE, DISK_CACHE_CAPACITY);
    return 0;
}

