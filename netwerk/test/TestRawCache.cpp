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
#include "nsString.h"
#include <stdio.h>

#include "nsINetDataCache.h"
#include "nsINetDataCacheRecord.h"
#include "nsMemCacheCID.h"
// file cache include
#include "nsNetDiskCacheCID.h"
#include "nsIPref.h"
#include "prenv.h"
#include "nsIFileStreams.h"
#include "nsIFileSpec.h"

// Number of test entries to be placed in the cache
#define NUM_CACHE_ENTRIES  250

// Cache content stream length will have random length between zero and
// MAX_CONTENT_LENGTH bytes
#define MAX_CONTENT_LENGTH 20000

// Length of random-data cache entry key
#define CACHE_KEY_LENGTH 15

// Length of random-data cache entry meta-data
#define CACHE_METADATA_LENGTH 100

static NS_DEFINE_CID(kMemCacheCID, NS_MEM_CACHE_FACTORY_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// file cache cid
static NS_DEFINE_CID(kDiskCacheCID, NS_NETDISKCACHE_CID) ;
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);

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

    virtual PRBool Match(char* aBuf, PRUint32 aCount) = 0;
    virtual void Skip(PRUint32 aCount) = 0;
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
        mState = 1103515245 * mState + 12345;
        return mState;
    }

    void Read(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            *aBuf++ = Next();
        }
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

    void
    Skip(PRUint32 aCount) {
        while (aCount--)
            Next();
    }

protected:
    
    PRUint32 mState;
    PRUint32 mStartSeed;
};

// A stream of data that increments on each byte that is read, modulo 256
class CounterStream : public nsITestDataStream {
public:
    CounterStream(PRUint32 aSeed) {
        mStartSeed = mState = aSeed;
    }
    
    PRUint32 GetStartSeed() {
        return mStartSeed;
    }
    
    PRUint32 Next() {
        mState += 1;
        mState &= 0xff;
        return mState;
    }

    void Read(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            *aBuf++ = Next();
        }
    }

    PRBool
    Match(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            if (*aBuf++ != (char)Next())
                return PR_FALSE;
        }
        return PR_TRUE;
    }

    void
    Skip(PRUint32 aCount) {
        mState += aCount;
        mState &= 0xff;
    }

protected:
    
    PRUint32 mState;
    PRUint32 mStartSeed;
};

static int gNumReaders = 0;
static PRUint32 gTotalBytesRead = 0;
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

        NS_ASSERTION(mBytesRead == mExpectedStreamLength,
                     "Stream in cache is wrong length");

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
TestReadStream(nsINetDataCacheRecord *record, nsITestDataStream *testDataStream,
               PRUint32 expectedStreamLength)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv;
    PRUint32 actualContentLength;

    rv = record->NewChannel(0, getter_AddRefs(channel));
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");

    rv = record->GetStoredContentLength(&actualContentLength);
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");
    NS_ASSERTION(actualContentLength == expectedStreamLength,
                 "nsINetDataCacheRecord::GetContentLength() busted ?");
    
    nsReader *reader = new nsReader;
    rv = reader->Init(testDataStream, expectedStreamLength);
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");
    
    rv = channel->AsyncRead(0, reader);
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");
    reader->Release();

    return NS_OK;
}

// Check that records can be retrieved using their record-ID, in addition
// to using the opaque key.
nsresult
TestRecordID(nsINetDataCache *cache)
{
    nsresult rv;
    nsCOMPtr<nsINetDataCacheRecord> record;
    RandomStream *randomStream;
    PRUint32 metaDataLength;
    char cacheKey[CACHE_KEY_LENGTH];
    char *metaData;
    PRUint32 testNum;
    PRBool match;

    for (testNum = 0; testNum < NUM_CACHE_ENTRIES; testNum++) {
        randomStream = new RandomStream(testNum);
        randomStream->Read(cacheKey, sizeof cacheKey);

        rv = cache->GetCachedNetDataByID(recordID[testNum], getter_AddRefs(record));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't obtain record using record ID");

        // Match against previously stored meta-data
        rv = record->GetMetaData(&metaDataLength, &metaData);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get record meta-data");
        match = randomStream->Match(metaData, metaDataLength);
        NS_ASSERTION(match, "Meta-data corrupted or incorrect");

        nsMemory::Free(metaData);
        delete randomStream;
    }
    return NS_OK;
}

// Check that all cache entries in the database are enumerated and that
// no duplicates appear.
nsresult
TestEnumeration(nsINetDataCache *cache)
{
    nsresult rv;
    nsCOMPtr<nsINetDataCacheRecord> record;
    nsCOMPtr<nsISupports> tempISupports;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    RandomStream *randomStream;
    PRUint32 metaDataLength;
    char cacheKey[CACHE_KEY_LENGTH];
    char *metaData;
    PRUint32 testNum;
    PRBool match;
    PRInt32 recID;

    int numRecords = 0;

    // Iterate over all records in the cache
    rv = cache->NewCacheEntryIterator(getter_AddRefs(iterator));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create new cache entry iterator");

    PRBool notDone;
    while (1) {

        // Done iterating ?
        rv = iterator->HasMoreElements(&notDone);
        if (NS_FAILED(rv)) return rv;
        if (!notDone)
            break;

        // Get next record in iteration
        rv = iterator->GetNext(getter_AddRefs(tempISupports));
        NS_ASSERTION(NS_SUCCEEDED(rv), "iterator bustage");
        record = do_QueryInterface(tempISupports);

        numRecords++;

        // Get record ID
        rv = record->GetRecordID(&recID);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get Record ID");
        testNum = mapRecordIdToTestNum(recID);
        NS_ASSERTION(testNum != -1, "Corrupted Record ID ?");

        // Erase mapping from table, so that duplicate enumerations are detected
        recordID[testNum] = -1;

        // Make sure stream matches test data
        randomStream = new RandomStream(testNum);
        randomStream->Read(cacheKey, sizeof cacheKey);

        // Match against previously stored meta-data
        rv = record->GetMetaData(&metaDataLength, &metaData);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get record meta-data");
        match = randomStream->Match(metaData, metaDataLength);
        NS_ASSERTION(match, "Meta-data corrupted or incorrect");
        nsMemory::Free(metaData);

        delete randomStream;
    }

    NS_ASSERTION(numRecords == NUM_CACHE_ENTRIES, "Iteration bug");

    return NS_OK;
}

// Read the test data that was written in FillCache(), checking for
// corruption, truncation.
nsresult
TestRead(nsINetDataCache *cache)
{
    nsresult rv;
    PRBool inCache;
    nsCOMPtr<nsINetDataCacheRecord> record;
    RandomStream *randomStream;
    PRUint32 metaDataLength;
    char cacheKey[CACHE_KEY_LENGTH];
    char *metaData, *storedCacheKey;
    PRUint32 testNum, storedCacheKeyLength;
    PRBool match;

    for (testNum = 0; testNum < NUM_CACHE_ENTRIES; testNum++) {
        randomStream = new RandomStream(testNum);
        randomStream->Read(cacheKey, sizeof cacheKey);

        // Ensure that entry is in the cache
        rv = cache->Contains(cacheKey, sizeof cacheKey, &inCache);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(inCache, "nsINetDataCache::Contains error");
        
        rv = cache->GetCachedNetData(cacheKey, sizeof cacheKey, getter_AddRefs(record));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        // Match against previously stored meta-data
        match = record->GetMetaData(&metaDataLength, &metaData);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        match = randomStream->Match(metaData, metaDataLength);
        NS_ASSERTION(match, "Meta-data corrupted or incorrect");
        nsMemory::Free(metaData);

        // Test GetKey() method
        rv = record->GetKey(&storedCacheKeyLength, &storedCacheKey);
        NS_ASSERTION(NS_SUCCEEDED(rv) &&
                     (storedCacheKeyLength == sizeof cacheKey) &&
                     !memcmp(storedCacheKey, &cacheKey[0], sizeof cacheKey),
                     "nsINetDataCacheRecord::GetKey failed");
        nsMemory::Free(storedCacheKey);

        PRUint32 expectedStreamLength = randomStream->Next() % MAX_CONTENT_LENGTH;

        TestReadStream(record, randomStream, expectedStreamLength);
    }

    WaitForEvents();

    // Compute rate in MB/s
    double rate = gTotalBytesRead / PR_IntervalToMilliseconds(gTotalDuration);
    rate *= NUM_CACHE_ENTRIES;
    rate *= 1000;
    rate /= (1024 * 1024);
    printf("Read %d bytes at a rate of %5.1f MB per second \n",
           gTotalBytesRead, rate);

    return NS_OK;
}

// Repeatedly call SetStoredContentLength() on a cache entry and make
// read the stream's data to ensure that it's not corrupted by the effect
nsresult
TestTruncation(nsINetDataCache *cache)
{
    nsresult rv;
    nsCOMPtr<nsINetDataCacheRecord> record;
    RandomStream *randomStream;
    char cacheKey[CACHE_KEY_LENGTH];

    randomStream = new RandomStream(0);
    randomStream->Read(cacheKey, sizeof cacheKey);

    rv = cache->GetCachedNetData(cacheKey, sizeof cacheKey, getter_AddRefs(record));
    NS_ASSERTION(NS_SUCCEEDED(rv), " ");

    randomStream->Skip(CACHE_METADATA_LENGTH);
    PRUint32 initialStreamLength = randomStream->Next() % MAX_CONTENT_LENGTH;
    delete randomStream;

    PRUint32 i;
    PRUint32 delta = initialStreamLength / 64;
    for (i = initialStreamLength; i >= delta; i -= delta) {
        PRUint32 expectedStreamLength = i;

        // Do the truncation
        record->SetStoredContentLength(expectedStreamLength);
        randomStream = new RandomStream(0);
        randomStream->Skip(CACHE_KEY_LENGTH + CACHE_METADATA_LENGTH + 1);

        PRUint32 afterContentLength;
        rv = record->GetStoredContentLength(&afterContentLength);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        NS_ASSERTION(afterContentLength == expectedStreamLength,
                     "nsINetDataCacheRecord::SetContentLength() failed to truncate record");

        TestReadStream(record, randomStream, expectedStreamLength);
        WaitForEvents();
    }

    return NS_OK;
}

// Write known data to random offsets in a single cache entry and test
// resulting stream for correctness.
nsresult
TestOffsetWrites(nsINetDataCache *cache)
{
    nsresult rv;
    nsCOMPtr<nsINetDataCacheRecord> record;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIOutputStream> outStream;
    char buf[512];
    char cacheKey[CACHE_KEY_LENGTH];
    RandomStream *randomStream;

    randomStream = new RandomStream(0);
    randomStream->Read(cacheKey, sizeof cacheKey);

    rv = cache->GetCachedNetData(cacheKey, sizeof cacheKey, getter_AddRefs(record));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't access record via opaque cache key");


    // Write buffer-fulls of data at random offsets into the cache entry.
    // Data written is (offset % 0xff)
    PRUint32 startingOffset;
    PRUint32 streamLength = 0;
    CounterStream *counterStream;
    int i;
    for (i = 0; i < 100; i++) {
        rv = record->NewChannel(0, getter_AddRefs(channel));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        startingOffset = streamLength ? streamLength - (randomStream->Next() % sizeof buf): 0;
        rv = channel->SetTransferOffset(startingOffset);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetTransferOffset failed");
        rv = channel->OpenOutputStream(getter_AddRefs(outStream));
        NS_ASSERTION(NS_SUCCEEDED(rv), "OpenOutputStream failed");
        
        counterStream = new CounterStream(startingOffset);
        counterStream->Read(buf, sizeof buf);

        PRUint32 numWritten;
        rv = outStream->Write(buf, sizeof buf, &numWritten);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(numWritten == sizeof buf, "Write() bug?");
        streamLength = startingOffset + sizeof buf;

        rv = outStream->Close();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't close channel");
        delete counterStream;
    }
    
    delete randomStream;

    counterStream = new CounterStream(0);
    TestReadStream(record, counterStream, streamLength);
    WaitForEvents();

    return NS_OK;
}

// Create entries in the network data cache, using random data for the
// key, the meta-data and the stored content data.
nsresult
FillCache(nsINetDataCache *cache)
{
    nsresult rv;
    PRBool inCache;
    nsCOMPtr<nsINetDataCacheRecord> record;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIOutputStream> outStream;
    char buf[1000];
    PRUint32 metaDataLength;
    char cacheKey[CACHE_KEY_LENGTH];
    char metaData[CACHE_METADATA_LENGTH];
    PRUint32 testNum;
    char *data;
    RandomStream *randomStream;
    PRUint32 totalBytesWritten = 0;

    PRIntervalTime startTime = PR_IntervalNow();
    
    for (testNum = 0; testNum < NUM_CACHE_ENTRIES; testNum++) {
        randomStream = new RandomStream(testNum);
        randomStream->Read(cacheKey, sizeof cacheKey);

        // No entry should be in cache until we add it
        rv = cache->Contains(cacheKey, sizeof cacheKey, &inCache);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(!inCache, "nsINetDataCache::Contains error");
        
        rv = cache->GetCachedNetData(cacheKey, sizeof cacheKey, getter_AddRefs(record));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't access record via opaque cache key");

        // Test nsINetDataCacheRecord::GetRecordID()
        rv = record->GetRecordID(&recordID[testNum]);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get Record ID");

        // Test nsINetDataCache::GetNumEntries()
        PRUint32 numEntries = (PRUint32)-1;
        rv = cache->GetNumEntries(&numEntries);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get number of cache entries");
        NS_ASSERTION(numEntries == testNum + 1, "GetNumEntries failure");

        // Record meta-data should be initially empty
        rv = record->GetMetaData(&metaDataLength, &data);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        if ((metaDataLength != 0) || (data != 0))
            return NS_ERROR_FAILURE;

        // Store random data as meta-data
        randomStream->Read(metaData, sizeof metaData);
        record->SetMetaData(sizeof metaData, metaData);

        rv = record->NewChannel(0, getter_AddRefs(channel));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");

        rv = channel->OpenOutputStream(getter_AddRefs(outStream));
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        
        PRUint32 beforeOccupancy;
        rv = cache->GetStorageInUse(&beforeOccupancy);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get cache occupancy");

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
        }
        outStream->Close();
        totalBytesWritten += streamLength;

        PRUint32 afterOccupancy;
        rv = cache->GetStorageInUse(&afterOccupancy);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get cache occupancy");
        PRUint32 streamLengthInKB = streamLength >> 10;
        NS_ASSERTION((afterOccupancy - beforeOccupancy) >= streamLengthInKB,
                     "nsINetDataCache::GetStorageInUse() is busted");
        

        // *Now* there should be an entry in the cache
        rv = cache->Contains(cacheKey, sizeof cacheKey, &inCache);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(inCache, "nsINetDataCache::Contains error");

        delete randomStream;
    }

    PRIntervalTime endTime = PR_IntervalNow();

    // Compute rate in MB/s
    double rate = totalBytesWritten / PR_IntervalToMilliseconds(endTime - startTime);
    rate *= 1000;
    rate /= (1024 * 1024);
    printf("Wrote %7d bytes at a rate of %5.1f MB per second \n",
           totalBytesWritten, rate);

    return NS_OK;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                                 NULL /* default */);
  return rv;
}

PRBool initPref ()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefPtr, kPrefCID, &rv);
    if (NS_FAILED(rv))
        return false;
               
    nsCOMPtr<nsIFileSpec> fileSpec;
    rv = NS_NewFileSpec (getter_AddRefs(fileSpec));
    if (NS_FAILED(rv))
        return false;
                            
    nsCString defaultPrefFile(PR_GetEnv ("MOZILLA_FIVE_HOME"));
    if (defaultPrefFile.Length())
        defaultPrefFile += "/";
    else
        defaultPrefFile = "./";
    defaultPrefFile += "default_prefs.js";
                                                 
    fileSpec->SetUnixStyleFilePath (defaultPrefFile.GetBuffer());
                                                    
    PRBool exists = false;
    fileSpec->Exists(&exists);
    if (exists)
        prefPtr->ReadUserPrefsFrom(fileSpec);
    else
        return false;
    return true;
}

int
main(int argc, char* argv[])
{
    nsresult rv;

    if(argc <2) {
      printf(" %s -f to test filecache\n", argv[0]) ;
      printf(" %s -m to test memcache\n", argv[0]) ;
      return -1 ;
    }

    nsCOMPtr<nsINetDataCache> cache;

    rv = NS_AutoregisterComponents();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't register XPCOM components");

    if (PL_strcasecmp(argv[1], "-m") == 0) {
        rv = nsComponentManager::CreateInstance(kMemCacheCID, nsnull,
                                            NS_GET_IID(nsINetDataCache),
                                            getter_AddRefs(cache));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create memory cache factory");
    } else if (PL_strcasecmp(argv[1], "-f") == 0) {
        // initialize pref
        initPref() ;

        rv = nsComponentManager::CreateInstance(kDiskCacheCID, nsnull,
                                            NS_GET_IID(nsINetDataCache),
                                             getter_AddRefs(cache));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create disk cache factory") ;

    } else {
      printf("  %s -f to test filecache\n", argv[0]) ;
      printf(" %s -m to test memcache\n", argv[0]) ;
      return -1 ;
    }

    InitQueue();

    PRUnichar* description;
    rv = cache->GetDescription(&description);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get cache description");
    nsCAutoString descStr; descStr.AssignWithConversion(description);
    printf("Testing: %s\n", descStr.GetBuffer());

    rv = cache->RemoveAll();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't clear cache");

    PRUint32 startOccupancy;
    rv = cache->GetStorageInUse(&startOccupancy);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get cache occupancy");

    PRUint32 numEntries = (PRUint32)-1;
    rv = cache->GetNumEntries(&numEntries);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get number of cache entries");
    NS_ASSERTION(numEntries == 0, "Couldn't clear cache");

    rv = FillCache(cache);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't fill cache with random test data");

    rv = TestRead(cache);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't read random test data from cache");

    rv = TestRecordID(cache);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't index records using record ID");

    rv = TestEnumeration(cache);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't successfully enumerate records");

    rv = TestTruncation(cache);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't successfully truncate records");

    rv = TestOffsetWrites(cache);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't successfully write to records using non-zero offsets");

    rv = cache->RemoveAll();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't clear cache");
    rv = cache->GetNumEntries(&numEntries);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get number of cache entries");
    NS_ASSERTION(numEntries == 0, "Couldn't clear cache");

    PRUint32 endOccupancy;
    rv = cache->GetStorageInUse(&endOccupancy);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get cache occupancy");

    NS_ASSERTION(startOccupancy == endOccupancy, "Cache occupancy not correctly computed ?");

    return 0;
}

