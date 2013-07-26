/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prtypes.h"
#include "pldhash.h"
#include "nsXPCOMStrings.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/scache/StartupCache.h"

#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIClassInfo.h"
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIStartupCache.h"
#include "nsIStorageStream.h"
#include "nsIStreamBufferAccess.h"
#include "nsIStringStream.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsIZipWriter.h"
#include "nsIZipReader.h"
#include "nsWeakReference.h"
#include "nsZipArchive.h"
#include "mozilla/Omnijar.h"
#include "prenv.h"
#include "mozilla/Telemetry.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsIProtocolHandler.h"

#ifdef IS_BIG_ENDIAN
#define SC_ENDIAN "big"
#else
#define SC_ENDIAN "little"
#endif

#if PR_BYTES_PER_WORD == 4
#define SC_WORDSIZE "4"
#else
#define SC_WORDSIZE "8"
#endif

namespace mozilla {
namespace scache {

static int64_t
GetStartupCacheMappingSize()
{
    mozilla::scache::StartupCache* sc = mozilla::scache::StartupCache::GetSingleton();
    return sc ? sc->SizeOfMapping() : 0;
}

NS_MEMORY_REPORTER_IMPLEMENT(StartupCacheMapping,
    "explicit/startup-cache/mapping",
    KIND_NONHEAP,
    nsIMemoryReporter::UNITS_BYTES,
    GetStartupCacheMappingSize,
    "Memory used to hold the mapping of the startup cache from file.  This "
    "memory is likely to be swapped out shortly after start-up.")

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(StartupCacheDataMallocSizeOf)

static int64_t
GetStartupCacheDataSize()
{
    mozilla::scache::StartupCache* sc = mozilla::scache::StartupCache::GetSingleton();
    return sc ? sc->HeapSizeOfIncludingThis(StartupCacheDataMallocSizeOf) : 0;
}

NS_MEMORY_REPORTER_IMPLEMENT(StartupCacheData,
    "explicit/startup-cache/data",
    KIND_HEAP,
    nsIMemoryReporter::UNITS_BYTES,
    GetStartupCacheDataSize,
    "Memory used by the startup cache for things other than the file mapping.")

static const char sStartupCacheName[] = "startupCache." SC_WORDSIZE "." SC_ENDIAN;
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

StartupCache*
StartupCache::GetSingleton() 
{
  if (!gStartupCache) {
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
      return nullptr;
    }

    StartupCache::InitSingleton();
  }

  return StartupCache::gStartupCache;
}

void
StartupCache::DeleteSingleton()
{
  delete StartupCache::gStartupCache;
}

nsresult
StartupCache::InitSingleton() 
{
  nsresult rv;
  StartupCache::gStartupCache = new StartupCache();

  rv = StartupCache::gStartupCache->Init();
  if (NS_FAILED(rv)) {
    delete StartupCache::gStartupCache;
    StartupCache::gStartupCache = nullptr;
  }
  return rv;
}

StartupCache* StartupCache::gStartupCache;
bool StartupCache::gShutdownInitiated;
bool StartupCache::gIgnoreDiskCache;
enum StartupCache::TelemetrifyAge StartupCache::gPostFlushAgeAction = StartupCache::IGNORE_AGE;

StartupCache::StartupCache() 
  : mArchive(NULL), mStartupWriteInitiated(false), mWriteThread(NULL),
    mMappingMemoryReporter(nullptr), mDataMemoryReporter(nullptr) { }

StartupCache::~StartupCache() 
{
  if (mTimer) {
    mTimer->Cancel();
  }

  // Generally, the in-memory table should be empty here,
  // but an early shutdown means either mTimer didn't run 
  // or the write thread is still running.
  WaitOnWriteThread();

  // If we shutdown quickly timer wont have fired. Instead of writing
  // it on the main thread and block the shutdown we simply wont update
  // the startup cache. Always do this if the file doesn't exist since
  // we use it part of the packge step.
  if (!mArchive) {
    WriteToDisk();
  }

  gStartupCache = nullptr;
  (void)::NS_UnregisterMemoryReporter(mMappingMemoryReporter);
  (void)::NS_UnregisterMemoryReporter(mDataMemoryReporter);
  mMappingMemoryReporter = nullptr;
  mDataMemoryReporter = nullptr;
}

nsresult
StartupCache::Init() 
{
  // workaround for bug 653936
  nsCOMPtr<nsIProtocolHandler> jarInitializer(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "jar"));
  
  nsresult rv;
  mTable.Init();
#ifdef DEBUG
  mWriteObjectMap.Init();
#endif

  // This allows to override the startup cache filename
  // which is useful from xpcshell, when there is no ProfLDS directory to keep cache in.
  char *env = PR_GetEnv("MOZ_STARTUP_CACHE");
  if (env) {
    rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(env), false, getter_AddRefs(mFile));
  } else {
    nsCOMPtr<nsIFile> file;
    rv = NS_GetSpecialDirectory("ProfLDS",
                                getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      // return silently, this will fail in mochitests's xpcshell process.
      return rv;
    }

    nsCOMPtr<nsIFile> profDir;
    NS_GetSpecialDirectory("ProfDS", getter_AddRefs(profDir));
    if (profDir) {
      bool same;
      if (NS_SUCCEEDED(profDir->Equals(file, &same)) && !same) {
        // We no longer store the startup cache in the main profile
        // directory, so we should cleanup the old one.
        if (NS_SUCCEEDED(
              profDir->AppendNative(NS_LITERAL_CSTRING("startupCache")))) {
          profDir->Remove(true);
        }
      }
    }

    rv = file->AppendNative(NS_LITERAL_CSTRING("startupCache"));
    NS_ENSURE_SUCCESS(rv, rv);

    // Try to create the directory if it's not there yet
    rv = file->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)
      return rv;

    rv = file->AppendNative(NS_LITERAL_CSTRING(sStartupCacheName));

    NS_ENSURE_SUCCESS(rv, rv);
    
    mFile = do_QueryInterface(file);
  }

  NS_ENSURE_TRUE(mFile, NS_ERROR_UNEXPECTED);

  mObserverService = do_GetService("@mozilla.org/observer-service;1");
  
  if (!mObserverService) {
    NS_WARNING("Could not get observerService.");
    return NS_ERROR_UNEXPECTED;
  }
  
  mListener = new StartupCacheListener();  
  rv = mObserverService->AddObserver(mListener, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                     false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mObserverService->AddObserver(mListener, "startupcache-invalidate",
                                     false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = LoadArchive(RECORD_AGE);
  
  // Sometimes we don't have a cache yet, that's ok.
  // If it's corrupted, just remove it and start over.
  if (gIgnoreDiskCache || (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)) {
    NS_WARNING("Failed to load startupcache file correctly, removing!");
    InvalidateCache();
  }

  mMappingMemoryReporter = new NS_MEMORY_REPORTER_NAME(StartupCacheMapping);
  mDataMemoryReporter    = new NS_MEMORY_REPORTER_NAME(StartupCacheData);
  (void)::NS_RegisterMemoryReporter(mMappingMemoryReporter);
  (void)::NS_RegisterMemoryReporter(mDataMemoryReporter);

  return NS_OK;
}

/** 
 * LoadArchive can be called from the main thread or while reloading cache on write thread.
 */
nsresult
StartupCache::LoadArchive(enum TelemetrifyAge flag)
{
  if (gIgnoreDiskCache)
    return NS_ERROR_FAILURE;

  bool exists;
  mArchive = NULL;
  nsresult rv = mFile->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return NS_ERROR_FILE_NOT_FOUND;
  
  mArchive = new nsZipArchive();
  rv = mArchive->OpenArchive(mFile);
  if (NS_FAILED(rv) || flag == IGNORE_AGE)
    return rv;

  nsCString comment;
  if (!mArchive->GetComment(comment)) {
    return rv;
  }

  const char *data;
  size_t len = NS_CStringGetData(comment, &data);
  PRTime creationStamp;
  // We might not have a comment if the startup cache file was created
  // before we started recording creation times in the comment.
  if (len == sizeof(creationStamp)) {
    memcpy(&creationStamp, data, len);
    PRTime current = PR_Now();
    int64_t diff = current - creationStamp;

    // We can't use AccumulateTimeDelta here because we have no way of
    // reifying a TimeStamp from creationStamp.
    int64_t usec_per_hour = PR_USEC_PER_SEC * int64_t(3600);
    int64_t hour_diff = (diff + usec_per_hour - 1) / usec_per_hour;
    mozilla::Telemetry::Accumulate(Telemetry::STARTUP_CACHE_AGE_HOURS,
                                   hour_diff);
  }

  return rv;
}

namespace {

nsresult
GetBufferFromZipArchive(nsZipArchive *zip, bool doCRC, const char* id,
                        char** outbuf, uint32_t* length)
{
  if (!zip)
    return NS_ERROR_NOT_AVAILABLE;

  nsZipItemPtr<char> zipItem(zip, id, doCRC);
  if (!zipItem)
    return NS_ERROR_NOT_AVAILABLE;

  *outbuf = zipItem.Forget();
  *length = zipItem.Length();
  return NS_OK;
}

} /* anonymous namespace */

// NOTE: this will not find a new entry until it has been written to disk!
// Consumer should take ownership of the resulting buffer.
nsresult
StartupCache::GetBuffer(const char* id, char** outbuf, uint32_t* length) 
{
  NS_ASSERTION(NS_IsMainThread(), "Startup cache only available on main thread");
  WaitOnWriteThread();
  if (!mStartupWriteInitiated) {
    CacheEntry* entry; 
    nsDependentCString idStr(id);
    mTable.Get(idStr, &entry);
    if (entry) {
      *outbuf = new char[entry->size];
      memcpy(*outbuf, entry->data, entry->size);
      *length = entry->size;
      return NS_OK;
    }
  }

  nsresult rv = GetBufferFromZipArchive(mArchive, true, id, outbuf, length);
  if (NS_SUCCEEDED(rv))
    return rv;

  nsRefPtr<nsZipArchive> omnijar = mozilla::Omnijar::GetReader(mozilla::Omnijar::APP);
  // no need to checksum omnijarred entries
  rv = GetBufferFromZipArchive(omnijar, false, id, outbuf, length);
  if (NS_SUCCEEDED(rv))
    return rv;

  omnijar = mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
  // no need to checksum omnijarred entries
  return GetBufferFromZipArchive(omnijar, false, id, outbuf, length);
}

// Makes a copy of the buffer, client retains ownership of inbuf.
nsresult
StartupCache::PutBuffer(const char* id, const char* inbuf, uint32_t len) 
{
  NS_ASSERTION(NS_IsMainThread(), "Startup cache only available on main thread");
  WaitOnWriteThread();
  if (StartupCache::gShutdownInitiated) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoArrayPtr<char> data(new char[len]);
  memcpy(data, inbuf, len);

  nsDependentCString idStr(id);
  // Cache it for now, we'll write all together later.
  CacheEntry* entry; 
  
#ifdef DEBUG
  mTable.Get(idStr, &entry);
  NS_ASSERTION(entry == nullptr, "Existing entry in StartupCache.");
  
  if (mArchive) {
    nsZipItem* zipItem = mArchive->GetItem(id);
    NS_ASSERTION(zipItem == nullptr, "Existing entry in disk StartupCache.");
  }
#endif
  
  entry = new CacheEntry(data.forget(), len);
  mTable.Put(idStr, entry);
  return ResetStartupWriteTimer();
}

size_t
StartupCache::SizeOfMapping() 
{
    return mArchive ? mArchive->SizeOfMapping() : 0;
}

size_t
StartupCache::HeapSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
    // This function could measure more members, but they haven't been found by
    // DMD to be significant.  They can be added later if necessary.
    return aMallocSizeOf(this) +
           mTable.SizeOfExcludingThis(SizeOfEntryExcludingThis, aMallocSizeOf);
}

/* static */ size_t
StartupCache::SizeOfEntryExcludingThis(const nsACString& key, const nsAutoPtr<CacheEntry>& data,
                                       mozilla::MallocSizeOf mallocSizeOf, void *)
{
    return data->SizeOfExcludingThis(mallocSizeOf);
}

struct CacheWriteHolder
{
  nsCOMPtr<nsIZipWriter> writer;
  nsCOMPtr<nsIStringInputStream> stream;
  PRTime time;
};

PLDHashOperator
CacheCloseHelper(const nsACString& key, nsAutoPtr<CacheEntry>& data, 
                 void* closure) 
{
  nsresult rv;
 
  CacheWriteHolder* holder = (CacheWriteHolder*) closure;  
  nsIStringInputStream* stream = holder->stream;
  nsIZipWriter* writer = holder->writer;

  stream->ShareData(data->data, data->size);

#ifdef DEBUG
  bool hasEntry;
  rv = writer->HasEntry(key, &hasEntry);
  NS_ASSERTION(NS_SUCCEEDED(rv) && hasEntry == false, 
               "Existing entry in disk StartupCache.");
#endif
  rv = writer->AddEntryStream(key, holder->time, true, stream, false);
  
  if (NS_FAILED(rv)) {
    NS_WARNING("cache entry deleted but not written to disk.");
  }
  return PL_DHASH_REMOVE;
}


/** 
 * WriteToDisk writes the cache out to disk. Callers of WriteToDisk need to call WaitOnWriteThread
 * to make sure there isn't a write happening on another thread
 */
void
StartupCache::WriteToDisk() 
{
  nsresult rv;
  mStartupWriteInitiated = true;

  if (!mTable.IsInitialized() || mTable.Count() == 0)
    return;

  nsCOMPtr<nsIZipWriter> zipW = do_CreateInstance("@mozilla.org/zipwriter;1");
  if (!zipW)
    return;

  rv = zipW->Open(mFile, PR_RDWR | PR_CREATE_FILE);
  if (NS_FAILED(rv)) {
    NS_WARNING("could not open zipfile for write");
    return;
  } 

  // If we didn't have an mArchive member, that means that we failed to
  // open the startup cache for reading.  Therefore, we need to record
  // the time of creation in a zipfile comment; this will be useful for
  // Telemetry statistics.
  PRTime now = PR_Now();
  if (!mArchive) {
    nsCString comment;
    comment.Assign((char *)&now, sizeof(now));
    zipW->SetComment(comment);
  }

  nsCOMPtr<nsIStringInputStream> stream 
    = do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Couldn't create string input stream.");
    return;
  }

  CacheWriteHolder holder;
  holder.stream = stream;
  holder.writer = zipW;
  holder.time = now;

  mTable.Enumerate(CacheCloseHelper, &holder);

  // Close the archive so Windows doesn't choke.
  mArchive = NULL;
  zipW->Close();

  // We succesfully wrote the archive to disk; mark the disk file as trusted
  gIgnoreDiskCache = false;

  // Our reader's view of the archive is outdated now, reload it.
  LoadArchive(gPostFlushAgeAction);
  
  return;
}

void
StartupCache::InvalidateCache() 
{
  WaitOnWriteThread();
  mTable.Clear();
  mArchive = NULL;
  nsresult rv = mFile->Remove(false);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND) {
    gIgnoreDiskCache = true;
    mozilla::Telemetry::Accumulate(Telemetry::STARTUP_CACHE_INVALID, true);
    return;
  }
  gIgnoreDiskCache = false;
  LoadArchive(gPostFlushAgeAction);
}

void
StartupCache::IgnoreDiskCache()
{
  gIgnoreDiskCache = true;
  if (gStartupCache)
    gStartupCache->InvalidateCache();
}

/*
 * WaitOnWriteThread() is called from a main thread to wait for the worker
 * thread to finish. However since the same code is used in the worker thread and
 * main thread, the worker thread can also call WaitOnWriteThread() which is a no-op.
 */
void
StartupCache::WaitOnWriteThread()
{
  NS_ASSERTION(NS_IsMainThread(), "Startup cache should only wait for io thread on main thread");
  if (!mWriteThread || mWriteThread == PR_GetCurrentThread())
    return;

  PR_JoinThread(mWriteThread);
  mWriteThread = NULL;
}

void 
StartupCache::ThreadedWrite(void *aClosure)
{
  PR_SetCurrentThreadName("StartupCache");
  gStartupCache->WriteToDisk();
}

/*
 * The write-thread is spawned on a timeout(which is reset with every write). This
 * can avoid a slow shutdown. After writing out the cache, the zipreader is
 * reloaded on the worker thread.
 */
void
StartupCache::WriteTimeout(nsITimer *aTimer, void *aClosure)
{
  gStartupCache->mWriteThread = PR_CreateThread(PR_USER_THREAD,
                                                StartupCache::ThreadedWrite,
                                                NULL,
                                                PR_PRIORITY_NORMAL,
                                                PR_LOCAL_THREAD,
                                                PR_JOINABLE_THREAD,
                                                0);
}

// We don't want to refcount StartupCache, so we'll just
// hold a ref to this and pass it to observerService instead.
NS_IMPL_ISUPPORTS1(StartupCacheListener, nsIObserver)

nsresult
StartupCacheListener::Observe(nsISupports *subject, const char* topic, const PRUnichar* data)
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc)
    return NS_OK;

  if (strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    // Do not leave the thread running past xpcom shutdown
    sc->WaitOnWriteThread();
    StartupCache::gShutdownInitiated = true;
  } else if (strcmp(topic, "startupcache-invalidate") == 0) {
    sc->InvalidateCache();
  }
  return NS_OK;
} 

nsresult
StartupCache::GetDebugObjectOutputStream(nsIObjectOutputStream* aStream,
                                         nsIObjectOutputStream** aOutStream) 
{
  NS_ENSURE_ARG_POINTER(aStream);
#ifdef DEBUG
  StartupCacheDebugOutputStream* stream
    = new StartupCacheDebugOutputStream(aStream, &mWriteObjectMap);
  NS_ADDREF(*aOutStream = stream);
#else
  NS_ADDREF(*aOutStream = aStream);
#endif
  
  return NS_OK;
}

nsresult
StartupCache::ResetStartupWriteTimer()
{
  mStartupWriteInitiated = false;
  nsresult rv;
  if (!mTimer)
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  else
    rv = mTimer->Cancel();
  NS_ENSURE_SUCCESS(rv, rv);
  // Wait for 10 seconds, then write out the cache.
  mTimer->InitWithFuncCallback(StartupCache::WriteTimeout, this, 60000,
                               nsITimer::TYPE_ONE_SHOT);
  return NS_OK;
}

nsresult
StartupCache::RecordAgesAlways()
{
  gPostFlushAgeAction = RECORD_AGE;
  return NS_OK;
}

// StartupCacheDebugOutputStream implementation
#ifdef DEBUG
NS_IMPL_ISUPPORTS3(StartupCacheDebugOutputStream, nsIObjectOutputStream, 
                   nsIBinaryOutputStream, nsIOutputStream)

bool
StartupCacheDebugOutputStream::CheckReferences(nsISupports* aObject)
{
  nsresult rv;
  
  nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aObject);
  if (!classInfo) {
    NS_ERROR("aObject must implement nsIClassInfo");
    return false;
  }
  
  uint32_t flags;
  rv = classInfo->GetFlags(&flags);
  NS_ENSURE_SUCCESS(rv, false);
  if (flags & nsIClassInfo::SINGLETON)
    return true;
  
  nsISupportsHashKey* key = mObjectMap->GetEntry(aObject);
  if (key) {
    NS_ERROR("non-singleton aObject is referenced multiple times in this" 
                  "serialization, we don't support that.");
    return false;
  }

  mObjectMap->PutEntry(aObject);
  return true;
}

// nsIObjectOutputStream implementation
nsresult
StartupCacheDebugOutputStream::WriteObject(nsISupports* aObject, bool aIsStrongRef)
{
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
  
  NS_ASSERTION(rootObject.get() == aObject,
               "bad call to WriteObject -- call WriteCompoundObject!");
  bool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteObject(aObject, aIsStrongRef);
}

nsresult
StartupCacheDebugOutputStream::WriteSingleRefObject(nsISupports* aObject)
{
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
  
  NS_ASSERTION(rootObject.get() == aObject,
               "bad call to WriteSingleRefObject -- call WriteCompoundObject!");
  bool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteSingleRefObject(aObject);
}

nsresult
StartupCacheDebugOutputStream::WriteCompoundObject(nsISupports* aObject,
                                                const nsIID& aIID,
                                                bool aIsStrongRef)
{
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
  
  nsCOMPtr<nsISupports> roundtrip;
  rootObject->QueryInterface(aIID, getter_AddRefs(roundtrip));
  NS_ASSERTION(roundtrip.get() == aObject,
               "bad aggregation or multiple inheritance detected by call to "
               "WriteCompoundObject!");

  bool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteCompoundObject(aObject, aIID, aIsStrongRef);
}

nsresult
StartupCacheDebugOutputStream::WriteID(nsID const& aID) 
{
  return mBinaryStream->WriteID(aID);
}

char*
StartupCacheDebugOutputStream::GetBuffer(uint32_t aLength, uint32_t aAlignMask)
{
  return mBinaryStream->GetBuffer(aLength, aAlignMask);
}

void
StartupCacheDebugOutputStream::PutBuffer(char* aBuffer, uint32_t aLength)
{
  mBinaryStream->PutBuffer(aBuffer, aLength);
}
#endif //DEBUG

StartupCacheWrapper* StartupCacheWrapper::gStartupCacheWrapper = nullptr;

NS_IMPL_ISUPPORTS1(StartupCacheWrapper, nsIStartupCache)

StartupCacheWrapper* StartupCacheWrapper::GetSingleton() 
{
  if (!gStartupCacheWrapper)
    gStartupCacheWrapper = new StartupCacheWrapper();

  NS_ADDREF(gStartupCacheWrapper);
  return gStartupCacheWrapper;
}

nsresult 
StartupCacheWrapper::GetBuffer(const char* id, char** outbuf, uint32_t* length) 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return sc->GetBuffer(id, outbuf, length);
}

nsresult
StartupCacheWrapper::PutBuffer(const char* id, const char* inbuf, uint32_t length) 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return sc->PutBuffer(id, inbuf, length);
}

nsresult
StartupCacheWrapper::InvalidateCache() 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  sc->InvalidateCache();
  return NS_OK;
}

nsresult
StartupCacheWrapper::IgnoreDiskCache()
{
  StartupCache::IgnoreDiskCache();
  return NS_OK;
}

nsresult 
StartupCacheWrapper::GetDebugObjectOutputStream(nsIObjectOutputStream* stream,
                                                nsIObjectOutputStream** outStream) 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return sc->GetDebugObjectOutputStream(stream, outStream);
}

nsresult
StartupCacheWrapper::StartupWriteComplete(bool *complete)
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  sc->WaitOnWriteThread();
  *complete = sc->mStartupWriteInitiated && sc->mTable.Count() == 0;
  return NS_OK;
}

nsresult
StartupCacheWrapper::ResetStartupWriteTimer()
{
  StartupCache* sc = StartupCache::GetSingleton();
  return sc ? sc->ResetStartupWriteTimer() : NS_ERROR_NOT_INITIALIZED;
}

nsresult
StartupCacheWrapper::GetObserver(nsIObserver** obv) {
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ADDREF(*obv = sc->mListener);
  return NS_OK;
}

nsresult
StartupCacheWrapper::RecordAgesAlways() {
  StartupCache *sc = StartupCache::GetSingleton();
  return sc ? sc->RecordAgesAlways() : NS_ERROR_NOT_INITIALIZED;
}

} // namespace scache
} // namespace mozilla
