/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "PLDHashTable.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/AutoMemMap.h"
#include "mozilla/IOBuffers.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MemUtils.h"
#include "mozilla/MmapFaultHandler.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheChild.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ipc/MemMapSnapshot.h"

#include "nsClassHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIClassInfo.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsIOutputStream.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "mozilla/Omnijar.h"
#include "prenv.h"
#include "mozilla/Telemetry.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsIProtocolHandler.h"
#include "GeckoProfiler.h"
#include "nsAppRunner.h"

#if defined(XP_WIN)
#  include <windows.h>
#endif

#ifdef IS_BIG_ENDIAN
#  define SC_ENDIAN "big"
#else
#  define SC_ENDIAN "little"
#endif

#if PR_BYTES_PER_WORD == 4
#  define SC_WORDSIZE "4"
#else
#  define SC_WORDSIZE "8"
#endif

#define DOC_ELEM_INSERTED_TOPIC "document-element-inserted"
#define CONTENT_DOCUMENT_LOADED_TOPIC "content-document-loaded"

using namespace mozilla::Compression;

namespace mozilla {
namespace scache {

// This is included here, rather than as a member of the StartupCache
// singleton, because including it there would mean we need to pull
// in the chromium IPC headers everywhere we reference StartupCache.h
static ipc::FileDescriptor sSharedDataFD;

MOZ_DEFINE_MALLOC_SIZE_OF(StartupCacheMallocSizeOf)

NS_IMETHODIMP
StartupCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
  MutexAutoLock lock(mLock);
  if (XRE_IsParentProcess()) {
    MOZ_COLLECT_REPORT(
        "explicit/startup-cache/mapping", KIND_NONHEAP, UNITS_BYTES,
        mCacheData.nonHeapSizeOfExcludingThis(),
        "Memory used to hold the mapping of the startup cache from file. "
        "This memory is likely to be swapped out shortly after start-up.");
  } else {
    // In the child process, mCacheData actually points to the parent's
    // mSharedData. We just want to report this once as explicit under
    // the parent process's report.
    MOZ_COLLECT_REPORT(
        "startup-cache-shared-mapping", KIND_NONHEAP, UNITS_BYTES,
        mCacheData.nonHeapSizeOfExcludingThis(),
        "Memory used to hold the decompressed contents of part of the "
        "startup cache, to be used by child processes. This memory is "
        "likely to be swapped out shortly after start-up.");
  }

  MOZ_COLLECT_REPORT("explicit/startup-cache/data", KIND_HEAP, UNITS_BYTES,
                     HeapSizeOfIncludingThis(StartupCacheMallocSizeOf),
                     "Memory used by the startup cache for things other than "
                     "the file mapping.");

  MOZ_COLLECT_REPORT(
      "explicit/startup-cache/shared-mapping", KIND_NONHEAP, UNITS_BYTES,
      mSharedData.nonHeapSizeOfExcludingThis(),
      "Memory used to hold the decompressed contents of part of the "
      "startup cache, to be used by child processes. This memory is "
      "likely to be swapped out shortly after start-up.");

  return NS_OK;
}

static const uint8_t MAGIC[] = "startupcache0002";
// This is a heuristic value for how much to reserve for mTable to avoid
// rehashing. This is not a hard limit in release builds, but it is in
// debug builds as it should be stable. If we exceed this number we should
// just increase it.
static const size_t STARTUP_CACHE_RESERVE_CAPACITY = 450;
// This is a hard limit which we will assert on, to ensure that we don't
// have some bug causing runaway cache growth.
static const size_t STARTUP_CACHE_MAX_CAPACITY = 5000;

// Not const because we change it for gtests.
static uint8_t STARTUP_CACHE_WRITE_TIMEOUT = 60;

#define STARTUP_CACHE_NAME "startupCache." SC_WORDSIZE "." SC_ENDIAN

static inline Result<Ok, nsresult> Write(PRFileDesc* fd, const void* data,
                                         int32_t len) {
  if (PR_Write(fd, data, len) != len) {
    return Err(NS_ERROR_FAILURE);
  }
  return Ok();
}

static inline Result<Ok, nsresult> Seek(PRFileDesc* fd, int32_t offset) {
  if (PR_Seek(fd, offset, PR_SEEK_SET) == -1) {
    return Err(NS_ERROR_FAILURE);
  }
  return Ok();
}

static nsresult MapLZ4ErrorToNsresult(size_t aError) {
  return NS_ERROR_FAILURE;
}

StartupCache* StartupCache::GetSingleton() {
  return StartupCache::gStartupCache;
}

void StartupCache::DeleteSingleton() { StartupCache::gStartupCache = nullptr; }

nsresult StartupCache::PartialInitSingleton(nsIFile* aProfileLocalDir) {
#ifdef MOZ_DISABLE_STARTUPCACHE
  return NS_OK;
#else
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  nsresult rv;
  StartupCache::gStartupCache = new StartupCache();

  rv = StartupCache::gStartupCache->PartialInit(aProfileLocalDir);
  if (NS_FAILED(rv)) {
    StartupCache::gStartupCache = nullptr;
  }
  return rv;
#endif
}

nsresult StartupCache::InitChildSingleton(char* aScacheHandleStr,
                                          char* aScacheSizeStr) {
#ifdef MOZ_DISABLE_STARTUPCACHE
  return NS_OK;
#else
  MOZ_ASSERT(!XRE_IsParentProcess());

  nsresult rv;
  StartupCache::gStartupCache = new StartupCache();

  rv = StartupCache::gStartupCache->ParseStartupCacheCmdLineArgs(
      aScacheHandleStr, aScacheSizeStr);
  if (NS_FAILED(rv)) {
    StartupCache::gStartupCache = nullptr;
  }
  return rv;
#endif
}

nsresult StartupCache::FullyInitSingleton() {
  if (!StartupCache::gStartupCache) {
    return NS_OK;
  }
  return StartupCache::gStartupCache->FullyInit();
}

StaticRefPtr<StartupCache> StartupCache::gStartupCache;
ProcessType sProcessType;
bool StartupCache::gIgnoreDiskCache;
bool StartupCache::gFoundDiskCacheOnInit;

NS_IMPL_ISUPPORTS(StartupCache, nsIMemoryReporter)

StartupCache::StartupCache()
    : mLock("StartupCache::mLock"),
      mDirty(false),
      mWrittenOnce(false),
      mCurTableReferenced(false),
      mLoaded(false),
      mFullyInitialized(false),
      mRequestedCount(0),
      mPrefetchSize(0),
      mSharedDataSize(0),
      mCacheEntriesBaseOffset(0),
      mPrefetchThread(nullptr) {}

StartupCache::~StartupCache() { UnregisterWeakMemoryReporter(this); }

nsresult StartupCache::InitChild(StartupCacheChild* cacheChild) {
  mChildActor = cacheChild;
  sProcessType =
      GetChildProcessType(dom::ContentChild::GetSingleton()->GetRemoteType());

  mObserverService = do_GetService("@mozilla.org/observer-service;1");

  if (!mObserverService) {
    NS_WARNING("Could not get observerService.");
    return NS_ERROR_UNEXPECTED;
  }

  mListener = new StartupCacheListener();
  nsresult rv = mObserverService->AddObserver(
      mListener, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mObserverService->AddObserver(mListener, "startupcache-invalidate",
                                     false);

  NS_ENSURE_SUCCESS(rv, rv);

  if (mChildActor) {
    if (sProcessType == ProcessType::PrivilegedAbout) {
      // Since we control all of the documents loaded in the privileged
      // content process, we can increase the window of active time for the
      // StartupCache to include the scripts that are loaded until the
      // first document finishes loading.
      mContentStartupFinishedTopic.AssignLiteral(CONTENT_DOCUMENT_LOADED_TOPIC);
    } else {
      // In the child process, we need to freeze the startup cache before any
      // untrusted code has been executed. The insertion of the first DOM
      // document element may sometimes be earlier than is ideal, but at
      // least it should always be safe.
      mContentStartupFinishedTopic.AssignLiteral(DOC_ELEM_INSERTED_TOPIC);
    }

    mObserverService->AddObserver(mListener, mContentStartupFinishedTopic.get(),
                                  false);
  }

  mFullyInitialized = true;
  RegisterWeakMemoryReporter(this);
  return NS_OK;
}

nsresult StartupCache::PartialInit(nsIFile* aProfileLocalDir) {
  MutexAutoLock lock(mLock);
  // workaround for bug 653936
  nsCOMPtr<nsIProtocolHandler> jarInitializer(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "jar"));

  sProcessType = ProcessType::Parent;
  nsresult rv;

  if (mozilla::RunningGTest()) {
    STARTUP_CACHE_WRITE_TIMEOUT = 3;
  }

  // This allows to override the startup cache filename
  // which is useful from xpcshell, when there is no ProfLDS directory to keep
  // cache in.
  char* env = PR_GetEnv("MOZ_STARTUP_CACHE");
  if (env && *env) {
    rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(env), false,
                         getter_AddRefs(mFile));
  } else if (aProfileLocalDir) {
    nsCOMPtr<nsIFile> file;
    rv = aProfileLocalDir->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      // return silently, this will fail in mochitests's xpcshell process.
      return rv;
    }

    rv = file->AppendNative("startupCache"_ns);
    NS_ENSURE_SUCCESS(rv, rv);

    // Try to create the directory if it's not there yet
    rv = file->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) return rv;

    rv = file->AppendNative(nsLiteralCString(STARTUP_CACHE_NAME));

    NS_ENSURE_SUCCESS(rv, rv);

    mFile = file;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_TRUE(mFile, NS_ERROR_UNEXPECTED);

  mDecompressionContext = MakeUnique<LZ4FrameDecompressionContext>(true);

  Unused << mCacheData.init(mFile);
  auto result = LoadArchive();
  rv = result.isErr() ? result.unwrapErr() : NS_OK;

  gFoundDiskCacheOnInit = rv != NS_ERROR_FILE_NOT_FOUND;

  // Sometimes we don't have a cache yet, that's ok.
  // If it's corrupted, just remove it and start over.
  if (gIgnoreDiskCache || (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)) {
    NS_WARNING("Failed to load startupcache file correctly, removing!");
    InvalidateCacheImpl();
  }

  return NS_OK;
}

nsresult StartupCache::FullyInit() {
  mObserverService = do_GetService("@mozilla.org/observer-service;1");

  if (!mObserverService) {
    NS_WARNING("Could not get observerService.");
    return NS_ERROR_UNEXPECTED;
  }

  mListener = new StartupCacheListener();
  nsresult rv = mObserverService->AddObserver(
      mListener, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = mObserverService->AddObserver(mListener, "startupcache-invalidate",
                                     false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RegisterWeakMemoryReporter(this);
  mFullyInitialized = true;
  return NS_OK;
}

void StartupCache::InitContentChild(dom::ContentParent& parent) {
  auto* cache = GetSingleton();
  if (!cache) {
    return;
  }

  Unused << parent.SendPStartupCacheConstructor();
}

void StartupCache::AddStartupCacheCmdLineArgs(
    mozilla::ipc::GeckoChildProcessHost& procHost,
    const nsACString& aRemoteType, std::vector<std::string>& aExtraOpts) {
#ifndef ANDROID
  // Don't send original cache data to new processes if the cache has been
  // invalidated.
  if (!mSharedData.initialized() || gIgnoreDiskCache) {
    auto processType = GetChildProcessType(aRemoteType);
    bool wantScriptData = !mInitializedProcesses.contains(processType);
    mInitializedProcesses += processType;
    if (!wantScriptData) {
      aExtraOpts.push_back("-noScache");
    }
    return;
  }

  // Formats a pointer or pointer-sized-integer as a string suitable for
  // passing in an arguments list.
  auto formatPtrArg = [](auto arg) {
    return nsPrintfCString("%zu", uintptr_t(arg));
  };
  if (!mSharedDataHandle) {
    auto fd = mSharedData.cloneHandle();
    MOZ_ASSERT(fd.IsValid());

    mSharedDataHandle = fd.TakePlatformHandle();
  }

#  if defined(XP_WIN)
  procHost.AddHandleToShare(mSharedDataHandle.get());
  aExtraOpts.push_back("-scacheHandle");
  aExtraOpts.push_back(formatPtrArg(mSharedDataHandle.get()).get());
#  else
  procHost.AddFdToRemap(mSharedDataHandle.get(), kStartupCacheFd);
#  endif

  aExtraOpts.push_back("-scacheSize");
  aExtraOpts.push_back(formatPtrArg(mSharedDataSize).get());
#endif
}

nsresult StartupCache::ParseStartupCacheCmdLineArgs(char* aScacheHandleStr,
                                                    char* aScacheSizeStr) {
  // Parses an arg containing a pointer-sized-integer.
  auto parseUIntPtrArg = [](char*& aArg) {
    // ContentParent uses %zu to print a word-sized unsigned integer. So
    // even though strtoull() returns a long long int, it will fit in a
    // uintptr_t.
    return uintptr_t(strtoull(aArg, &aArg, 10));
  };

  MutexAutoLock lock(mLock);
  if (aScacheHandleStr) {
#ifdef XP_WIN
    auto parseHandleArg = [&](char*& aArg) {
      return HANDLE(parseUIntPtrArg(aArg));
    };

    sSharedDataFD = FileDescriptor(parseHandleArg(aScacheHandleStr));
#endif

#ifdef XP_UNIX
    sSharedDataFD = FileDescriptor(UniqueFileHandle(kStartupCacheFd));
#endif
    MOZ_TRY(mCacheData.initWithHandle(sSharedDataFD,
                                      parseUIntPtrArg(aScacheSizeStr)));
    mCacheData.setPersistent();
  }

  nsresult rv;
  auto result = LoadArchive();
  rv = result.isErr() ? result.unwrapErr() : NS_OK;

  // Sometimes we don't have a cache yet, that's ok.
  // If it's corrupted, just remove it and start over.
  if (gIgnoreDiskCache || (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)) {
    NS_WARNING("Failed to load startupcache file correctly, removing!");
    InvalidateCacheImpl();
  }

  return NS_OK;
}

ProcessType StartupCache::GetChildProcessType(const nsACString& remoteType) {
  if (remoteType == EXTENSION_REMOTE_TYPE) {
    return ProcessType::Extension;
  }
  if (remoteType == PRIVILEGEDABOUT_REMOTE_TYPE) {
    return ProcessType::PrivilegedAbout;
  }
  return ProcessType::Web;
}

void StartupCache::StartPrefetchMemoryThread() {
  // XXX: It would be great for this to not create its own thread, unfortunately
  // there doesn't seem to be an existing thread that makes sense for this, so
  // barring a coordinated global scheduling system this is the best we get.
  mPrefetchThread = PR_CreateThread(
      PR_USER_THREAD, StartupCache::ThreadedPrefetch, this, PR_PRIORITY_NORMAL,
      PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 256 * 1024);
}

Result<Ok, nsresult> StartupCache::LoadEntriesOffDisk() {
  mLock.AssertCurrentThreadOwns();
  auto size = mCacheData.size();

  uint32_t headerSize;
  uint32_t prefetchSize;
  size_t headerOffset =
      sizeof(MAGIC) + sizeof(headerSize) + sizeof(prefetchSize);
  if (size < headerOffset) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto data = mCacheData.get<uint8_t>();
  auto end = data + size;

  auto cleanup = MakeScopeExit([&]() {
    WaitOnPrefetchThread();
    mTable.clear();
    mCacheData.reset();
    mSharedDataSize = 0;
  });

  MMAP_FAULT_HANDLER_BEGIN_BUFFER(data.get(), size)

  if (memcmp(MAGIC, data.get(), sizeof(MAGIC))) {
    return Err(NS_ERROR_UNEXPECTED);
  }
  data += sizeof(MAGIC);

  headerSize = LittleEndian::readUint32(data.get());
  data += sizeof(headerSize);

  prefetchSize = LittleEndian::readUint32(data.get());
  data += sizeof(prefetchSize);

  if (size < prefetchSize) {
    MOZ_ASSERT(false, "StartupCache file is corrupt.");
    return Err(NS_ERROR_UNEXPECTED);
  }

  if (size < headerOffset + headerSize) {
    MOZ_ASSERT(false, "StartupCache file is corrupt.");
    return Err(NS_ERROR_UNEXPECTED);
  }

  if (CanPrefetchMemory()) {
    mPrefetchSize = prefetchSize;
    StartPrefetchMemoryThread();
  }

  Range<uint8_t> header(data, data + headerSize);
  data += headerSize;

  uint32_t numSharedEntries = 0;

  mCacheEntriesBaseOffset = headerOffset + headerSize;
  {
    if (!mTable.reserve(STARTUP_CACHE_RESERVE_CAPACITY)) {
      return Err(NS_ERROR_UNEXPECTED);
    }
    loader::InputBuffer buf(header);

    uint32_t currentOffset = 0;
    mSharedDataSize = sizeof(MAGIC) + sizeof(numSharedEntries);
    while (!buf.finished()) {
      uint32_t offset = 0;
      uint32_t compressedSize = 0;
      uint32_t uncompressedSize = 0;
      uint8_t sharedToChild = 0;
      uint16_t keyLenWithNullTerm;
      buf.codeUint32(offset);
      buf.codeUint32(compressedSize);
      buf.codeUint32(uncompressedSize);
      buf.codeUint8(sharedToChild);
      buf.codeUint16(keyLenWithNullTerm);
      if (!buf.checkCapacity(keyLenWithNullTerm)) {
        MOZ_ASSERT(false, "StartupCache file is corrupt.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      const char* keyBuf =
          reinterpret_cast<const char*>(buf.read(keyLenWithNullTerm));
      if (keyBuf[keyLenWithNullTerm - 1] != 0) {
        MOZ_ASSERT(false, "StartupCache file is corrupt.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      MaybeOwnedCharPtr key(MakeUnique<char[]>(keyLenWithNullTerm));
      memcpy(key.get(), keyBuf, keyLenWithNullTerm);

      if (offset + compressedSize > end - data) {
        if (XRE_IsParentProcess()) {
          MOZ_ASSERT(false, "StartupCache file is corrupt.");
          return Err(NS_ERROR_UNEXPECTED);
        }

        // If we're not the parent process, then we received a buffer with
        // only data that was already prefetched. If that's the case, then
        // just break because we've hit the end of that range.
        break;
      }

      EnumSet<StartupCacheEntryFlags> entryFlags;
      if (sharedToChild) {
        // We encode the key as a u16 indicating the length, followed by the
        // contents of the string, plus a null terminator (to simplify things
        // by using existing c-string hashers). We encode the uncompressed data
        // as simply a u32 length, followed by the contents of the string.
        size_t sharedEntrySize = sizeof(uint16_t) + keyLenWithNullTerm +
                                 sizeof(uint32_t) + uncompressedSize;
        numSharedEntries++;
        mSharedDataSize += sharedEntrySize;
        entryFlags += StartupCacheEntryFlags::Shared;
      }

      // Make sure offsets match what we'd expect based on script ordering and
      // size, as a basic sanity check.
      if (offset != currentOffset) {
        MOZ_ASSERT(false, "StartupCache file is corrupt.");
        return Err(NS_ERROR_UNEXPECTED);
      }
      currentOffset += compressedSize;

      // We could use mTable.putNew if we knew the file we're loading weren't
      // corrupt. However, we don't know that, so check if the key already
      // exists. If it does, we know the file must be corrupt.
      decltype(mTable)::AddPtr p = mTable.lookupForAdd(key.get());
      if (p) {
        MOZ_ASSERT(false, "StartupCache file is corrupt.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      if (!mTable.add(p, std::move(key),
                      StartupCacheEntry(offset, compressedSize,
                                        uncompressedSize, entryFlags))) {
        MOZ_ASSERT(false, "StartupCache file is corrupt.");
        return Err(NS_ERROR_UNEXPECTED);
      }
    }

    if (buf.error()) {
      MOZ_ASSERT(false, "StartupCache file is corrupt.");
      return Err(NS_ERROR_UNEXPECTED);
    }

    // We only get to init mSharedData once - we can't edit it after we created
    // it, otherwise we'd be pulling memory out from underneath our child
    // processes.
    if (!mSharedData.initialized()) {
      ipc::MemMapSnapshot mem;
      if (mem.Init(mSharedDataSize).isErr()) {
        MOZ_ASSERT(false, "Failed to init shared StartupCache data.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      auto ptr = mem.Get<uint8_t>();
      Range<uint8_t> ptrRange(ptr, ptr + mSharedDataSize);
      loader::PreallocatedOutputBuffer sharedBuf(ptrRange);

      if (sizeof(MAGIC) > sharedBuf.remainingCapacity()) {
        MOZ_ASSERT(false);
        return Err(NS_ERROR_UNEXPECTED);
      }
      uint8_t* sharedMagic = sharedBuf.write(sizeof(MAGIC));
      memcpy(sharedMagic, MAGIC, sizeof(MAGIC));
      sharedBuf.codeUint32(numSharedEntries);

      for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
        const auto& key = iter.get().key();
        auto& value = iter.get().value();

        if (!value.mFlags.contains(StartupCacheEntryFlags::Shared)) {
          continue;
        }

        uint16_t keyLenWithNullTerm =
            strnlen(key.get(), kStartupCacheKeyLengthCap) + 1;
        if (keyLenWithNullTerm >= kStartupCacheKeyLengthCap) {
          MOZ_ASSERT(false, "StartupCache key too large or not terminated.");
          return Err(NS_ERROR_UNEXPECTED);
        }
        sharedBuf.codeUint16(keyLenWithNullTerm);

        if (!sharedBuf.checkCapacity(keyLenWithNullTerm)) {
          MOZ_ASSERT(false, "Exceeded shared StartupCache buffer size.");
          return Err(NS_ERROR_UNEXPECTED);
        }
        uint8_t* keyBuffer = sharedBuf.write(keyLenWithNullTerm);
        memcpy(keyBuffer, key.get(), keyLenWithNullTerm);

        sharedBuf.codeUint32(value.mUncompressedSize);

        if (value.mUncompressedSize > sharedBuf.remainingCapacity()) {
          MOZ_ASSERT(false);
          return Err(NS_ERROR_UNEXPECTED);
        }
        value.mSharedDataOffset = sharedBuf.cursor();
        value.mData = MaybeOwnedCharPtr(
            (char*)(sharedBuf.write(value.mUncompressedSize)));

        if (sharedBuf.error()) {
          MOZ_ASSERT(false, "Failed serializing to shared memory.");
          return Err(NS_ERROR_UNEXPECTED);
        }

        MOZ_TRY(DecompressEntry(value));
        value.mData = nullptr;
      }

      if (mem.Finalize(mSharedData).isErr()) {
        MOZ_ASSERT(false, "Failed to freeze shared StartupCache data.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      mSharedData.setPersistent();
    }
  }

  MMAP_FAULT_HANDLER_CATCH(Err(NS_ERROR_UNEXPECTED))
  cleanup.release();

  return Ok();
}

Result<Ok, nsresult> StartupCache::LoadEntriesFromSharedMemory() {
  mLock.AssertCurrentThreadOwns();
  auto size = mCacheData.size();
  auto start = mCacheData.get<uint8_t>();
  auto end = start + size;

  Range<uint8_t> range(start, end);
  loader::InputBuffer buf(range);

  if (sizeof(MAGIC) > buf.remainingCapacity()) {
    MOZ_ASSERT(false, "Bad shared StartupCache buffer.");
    return Err(NS_ERROR_UNEXPECTED);
  }

  const uint8_t* magic = buf.read(sizeof(MAGIC));
  if (memcmp(magic, MAGIC, sizeof(MAGIC)) != 0) {
    MOZ_ASSERT(false, "Bad shared StartupCache buffer.");
    return Err(NS_ERROR_UNEXPECTED);
  }

  uint32_t numEntries = 0;
  if (!buf.codeUint32(numEntries) || numEntries > STARTUP_CACHE_MAX_CAPACITY) {
    MOZ_ASSERT(false, "Bad number of entries in shared StartupCache buffer.");
    return Err(NS_ERROR_UNEXPECTED);
  }

  if (!mTable.reserve(STARTUP_CACHE_RESERVE_CAPACITY)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  uint32_t entriesSeen = 0;
  while (!buf.finished()) {
    entriesSeen++;
    if (entriesSeen > numEntries) {
      MOZ_ASSERT(false, "More entries than expected in StartupCache buffer.");
      return Err(NS_ERROR_UNEXPECTED);
    }

    uint16_t keyLenWithNullTerm = 0;
    buf.codeUint16(keyLenWithNullTerm);
    // NOTE: this will fail if the above codeUint16 call failed.
    if (!buf.checkCapacity(keyLenWithNullTerm)) {
      MOZ_ASSERT(false, "Corrupt StartupCache shared buffer.");
      return Err(NS_ERROR_UNEXPECTED);
    }
    MaybeOwnedCharPtr key(const_cast<char*>(
        reinterpret_cast<const char*>(buf.read(keyLenWithNullTerm))));
    if (key.get()[keyLenWithNullTerm - 1] != 0) {
      MOZ_ASSERT(false, "StartupCache key was not null-terminated");
      return Err(NS_ERROR_UNEXPECTED);
    }

    uint32_t uncompressedSize = 0;
    buf.codeUint32(uncompressedSize);

    if (!buf.checkCapacity(uncompressedSize)) {
      MOZ_ASSERT(false,
                 "StartupCache entry larger than remaining buffer size.");
      return Err(NS_ERROR_UNEXPECTED);
    }
    const char* data =
        reinterpret_cast<const char*>(buf.read(uncompressedSize));

    StartupCacheEntry entry(0, 0, uncompressedSize,
                            StartupCacheEntryFlags::Shared);
    // The const cast is unfortunate here. We have runtime guarantees via memory
    // protections that mData will not be modified, but we do not have compile
    // time guarantees.
    entry.mData = MaybeOwnedCharPtr(const_cast<char*>(data));

    if (!mTable.putNew(std::move(key), std::move(entry))) {
      return Err(NS_ERROR_UNEXPECTED);
    }
  }

  return Ok();
}

/**
 * LoadArchive can only be called from the main thread.
 */
Result<Ok, nsresult> StartupCache::LoadArchive() {
  mLock.AssertCurrentThreadOwns();

  if (gIgnoreDiskCache) {
    return Err(NS_ERROR_FAILURE);
  }

  mLoaded = true;

  if (!mCacheData.initialized()) {
    return Err(NS_ERROR_FILE_NOT_FOUND);
  }

  if (XRE_IsParentProcess()) {
    MOZ_TRY(LoadEntriesOffDisk());
  } else {
    MOZ_TRY(LoadEntriesFromSharedMemory());
  }

  return Ok();
}

Result<Ok, nsresult> StartupCache::DecompressEntry(StartupCacheEntry& aEntry) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mLock.AssertCurrentThreadOwns();

  size_t totalRead = 0;
  size_t totalWritten = 0;
  Span<const char> compressed = Span(
      mCacheData.get<char>().get() + mCacheEntriesBaseOffset + aEntry.mOffset,
      aEntry.mCompressedSize);
  Span<char> uncompressed = Span(aEntry.mData.get(), aEntry.mUncompressedSize);
  bool finished = false;
  while (!finished) {
    auto result = mDecompressionContext->Decompress(
        uncompressed.From(totalWritten), compressed.From(totalRead));
    if (NS_WARN_IF(result.isErr())) {
      aEntry.mData = nullptr;
      InvalidateCacheImpl();
      return Err(NS_ERROR_UNEXPECTED);
    }
    auto decompressionResult = result.unwrap();
    totalRead += decompressionResult.mSizeRead;
    totalWritten += decompressionResult.mSizeWritten;
    finished = decompressionResult.mFinished;
  }

  return Ok();
}

bool StartupCache::HasEntry(const char* id) {
  AUTO_PROFILER_LABEL("StartupCache::HasEntry", OTHER);

  MOZ_ASSERT(
      strnlen(id, kStartupCacheKeyLengthCap) + 1 < kStartupCacheKeyLengthCap,
      "StartupCache key too large or not terminated.");

  // The lock could be held here by the write thread, which could take a while.
  // scache reads/writes are never critical enough to block waiting for the
  // entire cache to be written to disk, so if we can't acquire the lock, we
  // just exit.
  Maybe<MutexAutoLock> tryLock;
  if (!MutexAutoLock::TryMake(mLock, tryLock)) {
    return false;
  }

  bool result = mTable.has(id);
  return result;
}

nsresult StartupCache::GetBuffer(const char* id, const char** outbuf,
                                 uint32_t* length) {
  Telemetry::LABELS_STARTUP_CACHE_REQUESTS label =
      Telemetry::LABELS_STARTUP_CACHE_REQUESTS::Miss;
  auto telemetry =
      MakeScopeExit([&label] { Telemetry::AccumulateCategorical(label); });

  // The lock could be held here by the write thread, which could take a while.
  // scache reads/writes are never critical enough to block waiting for the
  // entire cache to be written to disk, so if we can't acquire the lock, we
  // just exit.
  Maybe<MutexAutoLock> tryLock;
  if (!MutexAutoLock::TryMake(mLock, tryLock)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mLoaded) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(
      strnlen(id, kStartupCacheKeyLengthCap) + 1 < kStartupCacheKeyLengthCap,
      "StartupCache key too large or not terminated.");
  decltype(mTable)::Ptr p = mTable.lookup(id);
  if (!p) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Track that something holds a reference into mTable, so we know to hold
  // onto it in case the cache is invalidated.
  mCurTableReferenced = true;
  auto& value = p->value();
  if (value.mData) {
    label = Telemetry::LABELS_STARTUP_CACHE_REQUESTS::HitMemory;
  } else if (value.mSharedDataOffset != kStartupcacheEntryNotInSharedData) {
    if (!XRE_IsParentProcess() || !mSharedData.initialized()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    value.mData = MaybeOwnedCharPtr(mSharedData.get<char>().get() +
                                    value.mSharedDataOffset);
    label = Telemetry::LABELS_STARTUP_CACHE_REQUESTS::HitMemory;
  } else {
    // We do not decompress entries in child processes: we receive them
    // uncompressed in shared memory.
    if (!XRE_IsParentProcess() || !mCacheData.initialized()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    value.mData = MaybeOwnedCharPtr(value.mUncompressedSize);
    MMAP_FAULT_HANDLER_BEGIN_BUFFER(value.mData.get(), value.mUncompressedSize)

    auto decompressionResult = DecompressEntry(value);
    if (decompressionResult.isErr()) {
      return decompressionResult.unwrapErr();
    }

    MMAP_FAULT_HANDLER_CATCH(NS_ERROR_FAILURE)

    label = Telemetry::LABELS_STARTUP_CACHE_REQUESTS::HitDisk;
  }

  if (value.mRequestedOrder == kStartupCacheEntryNotRequested) {
    value.mRequestedOrder = ++mRequestedCount;
    MOZ_ASSERT(mRequestedCount <= mTable.count(),
               "Somehow we requested more StartupCache items than exist.");
    ResetStartupWriteTimerCheckingReadCount();
  }

  *outbuf = value.mData.get();
  *length = value.mUncompressedSize;

  if (value.mData.IsOwned()) {
    // We're returning a raw reference to this entry's owned buffer, so we can
    // no longer free that buffer.
    value.mFlags += StartupCacheEntryFlags::DoNotFree;
  }

  return NS_OK;
}

// Client gives ownership of inbuf.
nsresult StartupCache::PutBuffer(const char* id, UniquePtr<char[]>&& inbuf,
                                 uint32_t len, bool isFromChildProcess) {
  // We use this to update the RequestedByChild flag.
  MOZ_RELEASE_ASSERT(
      inbuf || isFromChildProcess,
      "Null input to PutBuffer should only be used by child actor.");

  if (!XRE_IsParentProcess() && !mChildActor && mFullyInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If we've already written to disk, just go ahead and exit. There's no point
  // in putting something in the cache since we're not going to write again.
  if (mWrittenOnce) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The lock could be held here by the write thread, which could take a while.
  // scache reads/writes are never critical enough to block waiting for the
  // entire cache to be written to disk, so if we can't acquire the lock, we
  // just exit.
  Maybe<MutexAutoLock> tryLock;
  if (!MutexAutoLock::TryMake(mLock, tryLock)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mLoaded) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t keyLenWithNullTerm = strnlen(id, kStartupCacheKeyLengthCap) + 1;
  if (keyLenWithNullTerm >= kStartupCacheKeyLengthCap) {
    MOZ_ASSERT(false, "StartupCache key too large or not terminated.");
    return NS_ERROR_UNEXPECTED;
  }
  decltype(mTable)::AddPtr p = mTable.lookupForAdd(id);

  if (p) {
    if (isFromChildProcess) {
      auto& value = p->value();
      value.mFlags += StartupCacheEntryFlags::RequestedByChild;
      if (value.mRequestedOrder == kStartupCacheEntryNotRequested) {
        value.mRequestedOrder = ++mRequestedCount;
        MOZ_ASSERT(mRequestedCount <= mTable.count(),
                   "Somehow we requested more StartupCache items than exist.");
        ResetStartupWriteTimerCheckingReadCount();
      }
    } else {
      NS_WARNING("Existing entry in StartupCache.");
    }

    // Double-caching is undesirable but not an error.
    return NS_OK;
  }

  if (!inbuf) {
    MOZ_ASSERT(false,
               "Should only PutBuffer with null buffer for existing entries.");
    return NS_ERROR_UNEXPECTED;
  }

  EnumSet<StartupCacheEntryFlags> flags =
      StartupCacheEntryFlags::AddedThisSession;
  if (isFromChildProcess) {
    flags += StartupCacheEntryFlags::RequestedByChild;
  }

  UniquePtr<char[]> key = MakeUnique<char[]>(keyLenWithNullTerm);
  memcpy(key.get(), id, keyLenWithNullTerm);
  // add returns false on alloc failure - in the very unlikely event we hit
  // that and aren't going to crash elsewhere, there's no reason we need to
  // crash here.
  if (mTable.add(
          p, MaybeOwnedCharPtr(std::move(key)),
          StartupCacheEntry(std::move(inbuf), len, ++mRequestedCount, flags))) {
    return ResetStartupWriteTimerImpl();
  }
  MOZ_DIAGNOSTIC_ASSERT(mTable.count() < STARTUP_CACHE_MAX_CAPACITY,
                        "Too many StartupCache entries.");
  return NS_OK;
}

size_t StartupCache::HeapSizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // This function could measure more members, but they haven't been found by
  // DMD to be significant.  They can be added later if necessary.

  size_t n = aMallocSizeOf(this);

  n += mTable.shallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
    if (iter.get().value().mData) {
      n += aMallocSizeOf(iter.get().value().mData.get());
    }
    n += iter.get().key().SizeOfExcludingThis(aMallocSizeOf);
  }

  return n;
}

/**
 * WriteToDisk writes the cache out to disk. Callers of WriteToDisk need to call
 * WaitOnWriteComplete to make sure there isn't a write
 * happening on another thread
 */
Result<Ok, nsresult> StartupCache::WriteToDisk() {
  MOZ_ASSERT(XRE_IsParentProcess());
  mLock.AssertCurrentThreadOwns();

  if (!mDirty || mWrittenOnce) {
    return Ok();
  }

  if (!mFile) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = mFile->Clone(getter_AddRefs(tmpFile));
  if (NS_FAILED(rv)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  nsAutoCString leafName;
  mFile->GetNativeLeafName(leafName);
  tmpFile->SetNativeLeafName(leafName + "-tmp"_ns);

  {
    AutoFDClose fd;
    MOZ_TRY(tmpFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                      0644, &fd.rwget()));

    nsTArray<std::pair<const MaybeOwnedCharPtr*, StartupCacheEntry*>> entries;

    for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
      entries.AppendElement(
          std::make_pair(&iter.get().key(), &iter.get().value()));
    }

    if (entries.IsEmpty()) {
      return Ok();
    }

    entries.Sort(StartupCacheEntry::Comparator());
    loader::OutputBuffer buf;
    for (auto& e : entries) {
      const auto* key = e.first;
      auto* value = e.second;
      auto uncompressedSize = value->mUncompressedSize;
      uint8_t sharedToChild =
          value->mFlags.contains(StartupCacheEntryFlags::RequestedByChild);
      // Set the mHeaderOffsetInFile so we can go back and edit the offset.
      value->mHeaderOffsetInFile = buf.cursor();
      // Write a 0 offset/compressed size as a placeholder until we get the real
      // offset after compressing.
      buf.codeUint32(0);
      buf.codeUint32(0);
      buf.codeUint32(uncompressedSize);
      buf.codeUint8(sharedToChild);
      uint32_t keyLenWithNullTerm =
          strnlen(key->get(), kStartupCacheKeyLengthCap) + 1;
      if (keyLenWithNullTerm >= kStartupCacheKeyLengthCap) {
        MOZ_ASSERT(false, "StartupCache key too large or not terminated.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      buf.codeUint16(keyLenWithNullTerm);
      char* keyBuf = reinterpret_cast<char*>(buf.write(keyLenWithNullTerm));
      memcpy(keyBuf, key->get(), keyLenWithNullTerm);
    }

    uint8_t headerSize[4];
    LittleEndian::writeUint32(headerSize, buf.cursor());

    MOZ_TRY(Write(fd, MAGIC, sizeof(MAGIC)));
    MOZ_TRY(Write(fd, headerSize, sizeof(headerSize)));
    size_t prefetchSizeOffset = sizeof(MAGIC) + sizeof(headerSize);
    size_t headerStart = prefetchSizeOffset + sizeof(uint32_t);
    size_t dataStart = headerStart + buf.cursor();
    MOZ_TRY(Seek(fd, dataStart));

    size_t offset = 0;
    size_t prefetchSize = 0;

    const size_t chunkSize = 1024 * 16;
    LZ4FrameCompressionContext ctx(6,         /* aCompressionLevel */
                                   chunkSize, /* aReadBufLen */
                                   true,      /* aChecksum */
                                   true);     /* aStableSrc */
    size_t writeBufLen = ctx.GetRequiredWriteBufferLength();
    auto writeBuffer = MaybeOwnedCharPtr(writeBufLen);
    auto writeSpan = Span(writeBuffer.get(), writeBufLen);

    for (auto& e : entries) {
      auto* value = e.second;

      // If this is the first entry which was not requested, set the prefetch
      // size to the offset before we include this item. We will then prefetch
      // all items up to but not including this one during our next startup.
      if (value->mRequestedOrder == kStartupCacheEntryNotRequested &&
          prefetchSize == 0) {
        prefetchSize = dataStart + offset;
      }

      // Reuse the existing compressed entry if possible
      if (mCacheData.initialized() && value->mCompressedSize) {
        Span<const char> compressed =
            Span(mCacheData.get<char>().get() + mCacheEntriesBaseOffset +
                     value->mOffset,
                 value->mCompressedSize);
        MOZ_TRY(Write(fd, compressed.Elements(), compressed.Length()));
        value->mOffset = offset;
        offset += compressed.Length();
      } else {
        value->mOffset = offset;
        Span<const char> result;
        MOZ_TRY_VAR(result, ctx.BeginCompressing(writeSpan).mapErr(
                                MapLZ4ErrorToNsresult));
        MOZ_TRY(Write(fd, result.Elements(), result.Length()));
        offset += result.Length();

        for (size_t i = 0; i < value->mUncompressedSize; i += chunkSize) {
          size_t size = std::min(chunkSize, value->mUncompressedSize - i);
          char* uncompressed = value->mData.get() + i;
          MOZ_TRY_VAR(result, ctx.ContinueCompressing(Span(uncompressed, size))
                                  .mapErr(MapLZ4ErrorToNsresult));
          MOZ_TRY(Write(fd, result.Elements(), result.Length()));
          offset += result.Length();
        }

        MOZ_TRY_VAR(result, ctx.EndCompressing().mapErr(MapLZ4ErrorToNsresult));
        MOZ_TRY(Write(fd, result.Elements(), result.Length()));
        offset += result.Length();
        value->mCompressedSize = offset - value->mOffset;
      }
    }

    if (prefetchSize == 0) {
      prefetchSize = dataStart + offset;
    }

    for (auto& e : entries) {
      auto* value = e.second;
      uint8_t* headerEntry = buf.Get() + value->mHeaderOffsetInFile;
      LittleEndian::writeUint32(headerEntry, value->mOffset);
      LittleEndian::writeUint32(headerEntry + sizeof(value->mOffset),
                                value->mCompressedSize);
    }

    uint8_t prefetchSizeBuf[4];
    LittleEndian::writeUint32(prefetchSizeBuf, prefetchSize);

    MOZ_TRY(Seek(fd, prefetchSizeOffset));
    MOZ_TRY(Write(fd, prefetchSizeBuf, sizeof(prefetchSizeBuf)));
    MOZ_TRY(Write(fd, buf.Get(), buf.cursor()));
  }

  mDirty = false;
  mWrittenOnce = true;
  mCacheData.reset();
  tmpFile->MoveToNative(nullptr, leafName);

  // If we're shutting down, we do not care about freeing up memory right now.
  if (AppShutdown::IsShuttingDown()) {
    return Ok();
  }

  // We've just written our buffers to disk, and we're 60 seconds out from
  // startup, so they're unlikely to be needed again any time soon; let's
  // clean up what we can.
  for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
    auto& value = iter.get().value();
    if (!value.mFlags.contains(StartupCacheEntryFlags::DoNotFree)) {
      value.mData = nullptr;
    }
  }

  return Ok();
}

void StartupCache::InvalidateCache() {
  MutexAutoLock lock(mLock);
  InvalidateCacheImpl();
}

void StartupCache::InvalidateCacheImpl(bool memoryOnly) {
  mLock.AssertCurrentThreadOwns();

  WaitOnPrefetchThread();
  mWrittenOnce = false;
  if (memoryOnly) {
    // This should only be called in tests.
    auto writeResult = WriteToDisk();
    if (NS_WARN_IF(writeResult.isErr())) {
      gIgnoreDiskCache = true;
      return;
    }
  }

  if (mCurTableReferenced) {
    // There should be no way for this assert to fail other than a user
    // manually sending startupcache-invalidate messages through the Browser
    // Toolbox.
    MOZ_DIAGNOSTIC_ASSERT(xpc::IsInAutomation() || mOldTables.Length() < 10,
                          "Startup cache invalidated too many times.");
    mOldTables.AppendElement(std::move(mTable));
    mCurTableReferenced = false;
  } else {
    mTable.clear();
  }

  if (!XRE_IsParentProcess()) {
    return;
  }
  mRequestedCount = 0;
  if (!XRE_IsParentProcess()) {
    gIgnoreDiskCache = true;
    return;
  }

  if (!memoryOnly) {
    mCacheData.reset();
    nsresult rv = mFile->Remove(false);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
        rv != NS_ERROR_FILE_NOT_FOUND) {
      gIgnoreDiskCache = true;
      return;
    }
  }
  gIgnoreDiskCache = false;
  Unused << mCacheData.init(mFile);
  auto result = LoadArchive();
  if (NS_WARN_IF(result.isErr())) {
    gIgnoreDiskCache = true;
  }
}

void StartupCache::MaybeInitShutdownWrite() {
  if (!XRE_IsParentProcess()) {
    return;
  }
  MutexAutoLock lock(mLock);
  if (mWriteTimer) {
    mWriteTimer->Cancel();
  }

  MaybeWriteOffMainThread();
}

void StartupCache::EnsureShutdownWriteComplete() {
  if (!XRE_IsParentProcess()) {
    return;
  }
  // Otherwise, ensure the write happens. The timer should have been cancelled
  // already in MaybeInitShutdownWrite.
  if (!mLock.TryLock()) {
    // Uh oh, we're writing away from the main thread. Wait to gain the lock,
    // to ensure the write completes.
    mLock.Lock();
  } else {
    // We got the lock. Keep the following in sync with
    // MaybeWriteOffMainThread:

    // If we've already written or there's nothing to write,
    // we don't need to do anything. This is the common case.
    if (mWrittenOnce || (mCacheData.initialized() && !ShouldCompactCache())) {
      mLock.Unlock();
      return;
    }

    WaitOnPrefetchThread();
    mDirty = true;
    // Most of this should be redundant given MaybeWriteOffMainThread should
    // have run before now.

    auto writeResult = WriteToDisk();
    Unused << NS_WARN_IF(writeResult.isErr());
    // We've had the lock, and `WriteToDisk()` sets mWrittenOnce and mDirty
    // when done, and checks for them when starting, so we don't need to do
    // anything else.
  }
  mLock.Unlock();
}

void StartupCache::IgnoreDiskCache() {
  gIgnoreDiskCache = true;
  if (gStartupCache) {
    gStartupCache->InvalidateCache();
  }
}

void StartupCache::WaitOnPrefetchThread() {
  if (!mPrefetchThread || mPrefetchThread == PR_GetCurrentThread()) return;

  PR_JoinThread(mPrefetchThread);
  mPrefetchThread = nullptr;
}

void StartupCache::ThreadedPrefetch(void* aClosure) {
  AUTO_PROFILER_REGISTER_THREAD("StartupCache");
  NS_SetCurrentThreadName("StartupCache");
  mozilla::IOInterposer::RegisterCurrentThread();
  StartupCache* startupCacheObj = static_cast<StartupCache*>(aClosure);
  uint8_t* buf = startupCacheObj->mCacheData.get<uint8_t>().get();
  size_t size = startupCacheObj->mPrefetchSize;
  MMAP_FAULT_HANDLER_BEGIN_BUFFER(buf, size)
  PrefetchMemory(buf, size);
  MMAP_FAULT_HANDLER_CATCH()
  mozilla::IOInterposer::UnregisterCurrentThread();
}

bool StartupCache::ShouldCompactCache() {
  mLock.AssertCurrentThreadOwns();
  // If we've requested less than 4/5 of the startup cache, then we should
  // probably compact it down. This can happen quite easily after the first run,
  // which seems to request quite a few more things than subsequent runs.
  CheckedInt<uint32_t> threshold = CheckedInt<uint32_t>(mTable.count()) * 4 / 5;
  MOZ_RELEASE_ASSERT(threshold.isValid(), "Runaway StartupCache size");
  return mRequestedCount < threshold.value();
}

/*
 * The write-thread is spawned on a timeout(which is reset with every write).
 * This can avoid a slow shutdown.
 */
void StartupCache::WriteTimeout(nsITimer* aTimer, void* aClosure) {
  /*
   * It is safe to use the pointer passed in aClosure to reference the
   * StartupCache object because the timer's lifetime is tightly coupled to
   * the lifetime of the StartupCache object; this timer is canceled in the
   * StartupCache destructor, guaranteeing that this function runs if and only
   * if the StartupCache object is valid.
   */
  StartupCache* startupCacheObj = static_cast<StartupCache*>(aClosure);
  MutexAutoLock lock(startupCacheObj->mLock);
  startupCacheObj->MaybeWriteOffMainThread();
}

void StartupCache::SendEntriesTimeout(nsITimer* aTimer, void* aClosure) {
  StartupCache* sc = static_cast<StartupCache*>(aClosure);
  MutexAutoLock lock(sc->mLock);
  ((StartupCacheChild*)sc->mChildActor)->SendEntriesAndFinalize(sc->mTable);
}

/*
 * See StartupCache::WriteTimeout above - this is just the non-static body.
 */
void StartupCache::MaybeWriteOffMainThread() {
  mLock.AssertCurrentThreadOwns();
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (mWrittenOnce) {
    return;
  }

  if (mCacheData.initialized() && !ShouldCompactCache()) {
    return;
  }

  // Keep this code in sync with EnsureShutdownWriteComplete.
  WaitOnPrefetchThread();
  mDirty = true;

  RefPtr<StartupCache> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("StartupCache::Write", [self]() mutable {
        MutexAutoLock lock(self->mLock);
        auto result = self->WriteToDisk();
        Unused << NS_WARN_IF(result.isErr());
      });
  NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_EVENT_MAY_BLOCK);
}

// We don't want to refcount StartupCache, so we'll just
// hold a ref to this and pass it to observerService instead.
NS_IMPL_ISUPPORTS(StartupCacheListener, nsIObserver)

nsresult StartupCacheListener::Observe(nsISupports* subject, const char* topic,
                                       const char16_t* data) {
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) return NS_OK;

  if (strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    // Do not leave the thread running past xpcom shutdown
    sc->WaitOnPrefetchThread();
    // Note that we don't do anything special for the background write
    // task; we expect the threadpool to finish running any tasks already
    // posted to it prior to shutdown. FastShutdown will call
    // EnsureShutdownWriteComplete() to ensure any pending writes happen
    // in that case.
  } else if (strcmp(topic, "startupcache-invalidate") == 0) {
    MutexAutoLock lock(sc->mLock);
    sc->InvalidateCacheImpl(data && nsCRT::strcmp(data, u"memoryOnly") == 0);
  } else if (sc->mContentStartupFinishedTopic.Equals(topic) &&
             sc->mChildActor) {
    if (sProcessType == ProcessType::PrivilegedAbout) {
      if (!sc->mSendEntriesTimer) {
        sc->mSendEntriesTimer = NS_NewTimer();
        sc->mSendEntriesTimer->InitWithNamedFuncCallback(
            StartupCache::SendEntriesTimeout, sc, 10000,
            nsITimer::TYPE_ONE_SHOT, "StartupCache::SendEntriesTimeout");
      }
    } else {
      MutexAutoLock lock(sc->mLock);
      ((StartupCacheChild*)sc->mChildActor)->SendEntriesAndFinalize(sc->mTable);
    }
  }
  return NS_OK;
}

nsresult StartupCache::GetDebugObjectOutputStream(
    nsIObjectOutputStream* aStream, nsIObjectOutputStream** aOutStream) {
  NS_ENSURE_ARG_POINTER(aStream);
#ifdef DEBUG
  auto* stream = new StartupCacheDebugOutputStream(aStream, &mWriteObjectMap);
  NS_ADDREF(*aOutStream = stream);
#else
  NS_ADDREF(*aOutStream = aStream);
#endif

  return NS_OK;
}

nsresult StartupCache::ResetStartupWriteTimerCheckingReadCount() {
  mLock.AssertCurrentThreadOwns();
  nsresult rv = NS_OK;
  if (!mWriteTimer) {
    mWriteTimer = NS_NewTimer();
  } else {
    rv = mWriteTimer->Cancel();
  }
  NS_ENSURE_SUCCESS(rv, rv);
  // Wait for the specified timeout, then write out the cache.
  mWriteTimer->InitWithNamedFuncCallback(
      StartupCache::WriteTimeout, this, STARTUP_CACHE_WRITE_TIMEOUT * 1000,
      nsITimer::TYPE_ONE_SHOT, "StartupCache::WriteTimeout");
  return NS_OK;
}

nsresult StartupCache::ResetStartupWriteTimer() {
  MutexAutoLock lock(mLock);
  return ResetStartupWriteTimerImpl();
}

nsresult StartupCache::ResetStartupWriteTimerImpl() {
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  mLock.AssertCurrentThreadOwns();
  mDirty = true;
  nsresult rv = NS_OK;
  if (!mWriteTimer) {
    mWriteTimer = NS_NewTimer();
  } else {
    rv = mWriteTimer->Cancel();
  }
  NS_ENSURE_SUCCESS(rv, rv);
  // Wait for the specified timeout, then write out the cache.
  mWriteTimer->InitWithNamedFuncCallback(
      StartupCache::WriteTimeout, this, STARTUP_CACHE_WRITE_TIMEOUT * 1000,
      nsITimer::TYPE_ONE_SHOT, "StartupCache::WriteTimeout");
  return NS_OK;
}

// Used only in tests:
bool StartupCache::StartupWriteComplete() { return !mDirty && mWrittenOnce; }

// StartupCacheDebugOutputStream implementation
#ifdef DEBUG
NS_IMPL_ISUPPORTS(StartupCacheDebugOutputStream, nsIObjectOutputStream,
                  nsIBinaryOutputStream, nsIOutputStream)

bool StartupCacheDebugOutputStream::CheckReferences(nsISupports* aObject) {
  nsresult rv;

  nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aObject);
  if (!classInfo) {
    NS_ERROR("aObject must implement nsIClassInfo");
    return false;
  }

  uint32_t flags;
  rv = classInfo->GetFlags(&flags);
  NS_ENSURE_SUCCESS(rv, false);
  if (flags & nsIClassInfo::SINGLETON) return true;

  nsISupportsHashKey* key = mObjectMap->GetEntry(aObject);
  if (key) {
    NS_ERROR(
        "non-singleton aObject is referenced multiple times in this"
        "serialization, we don't support that.");
    return false;
  }

  mObjectMap->PutEntry(aObject);
  return true;
}

// nsIObjectOutputStream implementation
nsresult StartupCacheDebugOutputStream::WriteObject(nsISupports* aObject,
                                                    bool aIsStrongRef) {
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));

  NS_ASSERTION(rootObject.get() == aObject,
               "bad call to WriteObject -- call WriteCompoundObject!");
  bool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteObject(aObject, aIsStrongRef);
}

nsresult StartupCacheDebugOutputStream::WriteSingleRefObject(
    nsISupports* aObject) {
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));

  NS_ASSERTION(rootObject.get() == aObject,
               "bad call to WriteSingleRefObject -- call WriteCompoundObject!");
  bool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteSingleRefObject(aObject);
}

nsresult StartupCacheDebugOutputStream::WriteCompoundObject(
    nsISupports* aObject, const nsIID& aIID, bool aIsStrongRef) {
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

nsresult StartupCacheDebugOutputStream::WriteID(nsID const& aID) {
  return mBinaryStream->WriteID(aID);
}

char* StartupCacheDebugOutputStream::GetBuffer(uint32_t aLength,
                                               uint32_t aAlignMask) {
  return mBinaryStream->GetBuffer(aLength, aAlignMask);
}

void StartupCacheDebugOutputStream::PutBuffer(char* aBuffer, uint32_t aLength) {
  mBinaryStream->PutBuffer(aBuffer, aLength);
}
#endif  // DEBUG

}  // namespace scache
}  // namespace mozilla
