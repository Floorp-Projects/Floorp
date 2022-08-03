/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "PLDHashTable.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/AutoMemMap.h"
#include "mozilla/IOBuffers.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MemUtils.h"
#include "mozilla/MmapFaultHandler.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/ScopeExit.h"

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
#include "xpcpublic.h"
#ifdef MOZ_BACKGROUNDTASKS
#  include "mozilla/BackgroundTasks.h"
#endif

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

using namespace mozilla::Compression;

namespace mozilla {
namespace scache {

MOZ_DEFINE_MALLOC_SIZE_OF(StartupCacheMallocSizeOf)

NS_IMETHODIMP
StartupCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT(
      "explicit/startup-cache/mapping", KIND_NONHEAP, UNITS_BYTES,
      mCacheData.nonHeapSizeOfExcludingThis(),
      "Memory used to hold the mapping of the startup cache from file. "
      "This memory is likely to be swapped out shortly after start-up.");

  MOZ_COLLECT_REPORT("explicit/startup-cache/data", KIND_HEAP, UNITS_BYTES,
                     HeapSizeOfIncludingThis(StartupCacheMallocSizeOf),
                     "Memory used by the startup cache for things other than "
                     "the file mapping.");

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

StartupCache* StartupCache::GetSingletonNoInit() {
  return StartupCache::gStartupCache;
}

StartupCache* StartupCache::GetSingleton() {
#ifdef MOZ_BACKGROUNDTASKS
  if (BackgroundTasks::IsBackgroundTaskMode()) {
    return nullptr;
  }
#endif

  if (!gStartupCache) {
    if (!XRE_IsParentProcess()) {
      return nullptr;
    }
#ifdef MOZ_DISABLE_STARTUPCACHE
    return nullptr;
#else
    StartupCache::InitSingleton();
#endif
  }

  return StartupCache::gStartupCache;
}

void StartupCache::DeleteSingleton() { StartupCache::gStartupCache = nullptr; }

nsresult StartupCache::InitSingleton() {
  nsresult rv;
  StartupCache::gStartupCache = new StartupCache();

  rv = StartupCache::gStartupCache->Init();
  if (NS_FAILED(rv)) {
    StartupCache::gStartupCache = nullptr;
  }
  return rv;
}

StaticRefPtr<StartupCache> StartupCache::gStartupCache;
bool StartupCache::gShutdownInitiated;
bool StartupCache::gIgnoreDiskCache;
bool StartupCache::gFoundDiskCacheOnInit;

NS_IMPL_ISUPPORTS(StartupCache, nsIMemoryReporter)

StartupCache::StartupCache()
    : mTableLock("StartupCache::mTableLock"),
      mDirty(false),
      mWrittenOnce(false),
      mCurTableReferenced(false),
      mRequestedCount(0),
      mCacheEntriesBaseOffset(0),
      mPrefetchThread(nullptr) {}

StartupCache::~StartupCache() { UnregisterWeakMemoryReporter(this); }

nsresult StartupCache::Init() {
  // workaround for bug 653936
  nsCOMPtr<nsIProtocolHandler> jarInitializer(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "jar"));

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
  } else {
    nsCOMPtr<nsIFile> file;
    rv = NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(file));
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
  rv = mObserverService->AddObserver(mListener, "intl:app-locales-changed",
                                     false);
  NS_ENSURE_SUCCESS(rv, rv);

  auto result = LoadArchive();
  rv = result.isErr() ? result.unwrapErr() : NS_OK;

  gFoundDiskCacheOnInit = rv != NS_ERROR_FILE_NOT_FOUND;

  // Sometimes we don't have a cache yet, that's ok.
  // If it's corrupted, just remove it and start over.
  if (gIgnoreDiskCache || (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)) {
    NS_WARNING("Failed to load startupcache file correctly, removing!");
    InvalidateCache();
  }

  RegisterWeakMemoryReporter(this);
  mDecompressionContext = MakeUnique<LZ4FrameDecompressionContext>(true);

  return NS_OK;
}

void StartupCache::StartPrefetchMemoryThread() {
  // XXX: It would be great for this to not create its own thread, unfortunately
  // there doesn't seem to be an existing thread that makes sense for this, so
  // barring a coordinated global scheduling system this is the best we get.
  mPrefetchThread = PR_CreateThread(
      PR_USER_THREAD, StartupCache::ThreadedPrefetch, this, PR_PRIORITY_NORMAL,
      PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 256 * 1024);
}

/**
 * LoadArchive can only be called from the main thread.
 */
Result<Ok, nsresult> StartupCache::LoadArchive() {
  MOZ_ASSERT(NS_IsMainThread(), "Can only load startup cache on main thread");
  if (gIgnoreDiskCache) return Err(NS_ERROR_FAILURE);

  MOZ_TRY(mCacheData.init(mFile));
  auto size = mCacheData.size();
  if (CanPrefetchMemory()) {
    StartPrefetchMemoryThread();
  }

  uint32_t headerSize;
  if (size < sizeof(MAGIC) + sizeof(headerSize)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto data = mCacheData.get<uint8_t>();
  auto end = data + size;

  MMAP_FAULT_HANDLER_BEGIN_BUFFER(data.get(), size)

  if (memcmp(MAGIC, data.get(), sizeof(MAGIC))) {
    return Err(NS_ERROR_UNEXPECTED);
  }
  data += sizeof(MAGIC);

  headerSize = LittleEndian::readUint32(data.get());
  data += sizeof(headerSize);

  if (headerSize > end - data) {
    MOZ_ASSERT(false, "StartupCache file is corrupt.");
    return Err(NS_ERROR_UNEXPECTED);
  }

  Range<uint8_t> header(data, data + headerSize);
  data += headerSize;

  mCacheEntriesBaseOffset = sizeof(MAGIC) + sizeof(headerSize) + headerSize;
  {
    if (!mTable.reserve(STARTUP_CACHE_RESERVE_CAPACITY)) {
      return Err(NS_ERROR_UNEXPECTED);
    }
    auto cleanup = MakeScopeExit([&]() {
      WaitOnPrefetchThread();
      mTable.clear();
      mCacheData.reset();
    });
    loader::InputBuffer buf(header);

    uint32_t currentOffset = 0;
    while (!buf.finished()) {
      uint32_t offset = 0;
      uint32_t compressedSize = 0;
      uint32_t uncompressedSize = 0;
      nsCString key;
      buf.codeUint32(offset);
      buf.codeUint32(compressedSize);
      buf.codeUint32(uncompressedSize);
      buf.codeString(key);

      if (offset + compressedSize > end - data) {
        MOZ_ASSERT(false, "StartupCache file is corrupt.");
        return Err(NS_ERROR_UNEXPECTED);
      }

      // Make sure offsets match what we'd expect based on script ordering and
      // size, as a basic sanity check.
      if (offset != currentOffset) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      currentOffset += compressedSize;

      // We could use mTable.putNew if we knew the file we're loading weren't
      // corrupt. However, we don't know that, so check if the key already
      // exists. If it does, we know the file must be corrupt.
      decltype(mTable)::AddPtr p = mTable.lookupForAdd(key);
      if (p) {
        return Err(NS_ERROR_UNEXPECTED);
      }

      if (!mTable.add(
              p, key,
              StartupCacheEntry(offset, compressedSize, uncompressedSize))) {
        return Err(NS_ERROR_UNEXPECTED);
      }
    }

    if (buf.error()) {
      return Err(NS_ERROR_UNEXPECTED);
    }

    cleanup.release();
  }

  MMAP_FAULT_HANDLER_CATCH(Err(NS_ERROR_UNEXPECTED))

  return Ok();
}

bool StartupCache::HasEntry(const char* id) {
  AUTO_PROFILER_LABEL("StartupCache::HasEntry", OTHER);

  MOZ_ASSERT(NS_IsMainThread(), "Startup cache only available on main thread");

  return mTable.has(nsDependentCString(id));
}

nsresult StartupCache::GetBuffer(const char* id, const char** outbuf,
                                 uint32_t* length)
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  AUTO_PROFILER_LABEL("StartupCache::GetBuffer", OTHER);

  NS_ASSERTION(NS_IsMainThread(),
               "Startup cache only available on main thread");

  Telemetry::LABELS_STARTUP_CACHE_REQUESTS label =
      Telemetry::LABELS_STARTUP_CACHE_REQUESTS::Miss;
  auto telemetry =
      MakeScopeExit([&label] { Telemetry::AccumulateCategorical(label); });

  decltype(mTable)::Ptr p = mTable.lookup(nsDependentCString(id));
  if (!p) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto& value = p->value();
  if (value.mData) {
    label = Telemetry::LABELS_STARTUP_CACHE_REQUESTS::HitMemory;
  } else {
    if (!mCacheData.initialized()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
#ifdef DEBUG
    // It should be impossible for a write to be pending here. This is because
    // we just checked mCacheData.initialized(), and this is reset before
    // writing to the cache. It's not re-initialized unless we call
    // LoadArchive(), either from Init() (which must have already happened) or
    // InvalidateCache(). InvalidateCache() locks the mutex, so a write can't be
    // happening. Really, we want to MOZ_ASSERT(!mTableLock.IsLocked()) here,
    // but there is no such method. So we hack around by attempting to gain the
    // lock. This should always succeed; if it fails, someone's broken the
    // assumptions.
    if (!mTableLock.TryLock()) {
      MOZ_ASSERT(false, "Could not gain mTableLock - should never happen!");
      return NS_ERROR_NOT_AVAILABLE;
    }
    mTableLock.Unlock();
#endif

    size_t totalRead = 0;
    size_t totalWritten = 0;
    Span<const char> compressed = Span(
        mCacheData.get<char>().get() + mCacheEntriesBaseOffset + value.mOffset,
        value.mCompressedSize);
    value.mData = MakeUnique<char[]>(value.mUncompressedSize);
    Span<char> uncompressed = Span(value.mData.get(), value.mUncompressedSize);
    MMAP_FAULT_HANDLER_BEGIN_BUFFER(uncompressed.Elements(),
                                    uncompressed.Length())
    bool finished = false;
    while (!finished) {
      auto result = mDecompressionContext->Decompress(
          uncompressed.From(totalWritten), compressed.From(totalRead));
      if (NS_WARN_IF(result.isErr())) {
        value.mData = nullptr;
        InvalidateCache();
        return NS_ERROR_FAILURE;
      }
      auto decompressionResult = result.unwrap();
      totalRead += decompressionResult.mSizeRead;
      totalWritten += decompressionResult.mSizeWritten;
      finished = decompressionResult.mFinished;
    }

    MMAP_FAULT_HANDLER_CATCH(NS_ERROR_FAILURE)

    label = Telemetry::LABELS_STARTUP_CACHE_REQUESTS::HitDisk;
  }

  if (!value.mRequested) {
    value.mRequested = true;
    value.mRequestedOrder = ++mRequestedCount;
    MOZ_ASSERT(mRequestedCount <= mTable.count(),
               "Somehow we requested more StartupCache items than exist.");
    ResetStartupWriteTimerCheckingReadCount();
  }

  // Track that something holds a reference into mTable, so we know to hold
  // onto it in case the cache is invalidated.
  mCurTableReferenced = true;
  *outbuf = value.mData.get();
  *length = value.mUncompressedSize;
  return NS_OK;
}

// Makes a copy of the buffer, client retains ownership of inbuf.
nsresult StartupCache::PutBuffer(const char* id, UniquePtr<char[]>&& inbuf,
                                 uint32_t len) MOZ_NO_THREAD_SAFETY_ANALYSIS {
  NS_ASSERTION(NS_IsMainThread(),
               "Startup cache only available on main thread");
  if (StartupCache::gShutdownInitiated) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool exists = mTable.has(nsDependentCString(id));

  if (exists) {
    NS_WARNING("Existing entry in StartupCache.");
    // Double-caching is undesirable but not an error.
    return NS_OK;
  }
  // Try to gain the table write lock. If the background task to write the
  // cache is running, this will fail.
  if (!mTableLock.TryLock()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  auto lockGuard = MakeScopeExit([&] {
    mTableLock.AssertCurrentThreadOwns();
    mTableLock.Unlock();
  });

  // putNew returns false on alloc failure - in the very unlikely event we hit
  // that and aren't going to crash elsewhere, there's no reason we need to
  // crash here.
  if (mTable.putNew(nsCString(id), StartupCacheEntry(std::move(inbuf), len,
                                                     ++mRequestedCount))) {
    return ResetStartupWriteTimer();
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
    n += iter.get().key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

/**
 * WriteToDisk writes the cache out to disk. Callers of WriteToDisk need to call
 * WaitOnWriteComplete to make sure there isn't a write
 * happening on another thread
 */
Result<Ok, nsresult> StartupCache::WriteToDisk() {
  mTableLock.AssertCurrentThreadOwns();

  if (!mDirty || mWrittenOnce) {
    return Ok();
  }

  if (!mFile) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  AutoFDClose fd;
  MOZ_TRY(mFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                  0644, &fd.rwget()));

  nsTArray<std::pair<const nsCString*, StartupCacheEntry*>> entries;
  for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
    if (iter.get().value().mRequested) {
      entries.AppendElement(
          std::make_pair(&iter.get().key(), &iter.get().value()));
    }
  }

  if (entries.IsEmpty()) {
    return Ok();
  }

  entries.Sort(StartupCacheEntry::Comparator());
  loader::OutputBuffer buf;
  for (auto& e : entries) {
    auto key = e.first;
    auto value = e.second;
    auto uncompressedSize = value->mUncompressedSize;
    // Set the mHeaderOffsetInFile so we can go back and edit the offset.
    value->mHeaderOffsetInFile = buf.cursor();
    // Write a 0 offset/compressed size as a placeholder until we get the real
    // offset after compressing.
    buf.codeUint32(0);
    buf.codeUint32(0);
    buf.codeUint32(uncompressedSize);
    buf.codeString(*key);
  }

  uint8_t headerSize[4];
  LittleEndian::writeUint32(headerSize, buf.cursor());

  MOZ_TRY(Write(fd, MAGIC, sizeof(MAGIC)));
  MOZ_TRY(Write(fd, headerSize, sizeof(headerSize)));
  size_t headerStart = sizeof(MAGIC) + sizeof(headerSize);
  size_t dataStart = headerStart + buf.cursor();
  MOZ_TRY(Seek(fd, dataStart));

  size_t offset = 0;

  const size_t chunkSize = 1024 * 16;
  LZ4FrameCompressionContext ctx(6,         /* aCompressionLevel */
                                 chunkSize, /* aReadBufLen */
                                 true,      /* aChecksum */
                                 true);     /* aStableSrc */
  size_t writeBufLen = ctx.GetRequiredWriteBufferLength();
  auto writeBuffer = MakeUnique<char[]>(writeBufLen);
  auto writeSpan = Span(writeBuffer.get(), writeBufLen);

  for (auto& e : entries) {
    auto value = e.second;
    value->mOffset = offset;
    Span<const char> result;
    MOZ_TRY_VAR(result,
                ctx.BeginCompressing(writeSpan).mapErr(MapLZ4ErrorToNsresult));
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
    MOZ_TRY(Seek(fd, dataStart + offset));
  }

  for (auto& e : entries) {
    auto value = e.second;
    uint8_t* headerEntry = buf.Get() + value->mHeaderOffsetInFile;
    LittleEndian::writeUint32(headerEntry, value->mOffset);
    LittleEndian::writeUint32(headerEntry + sizeof(value->mOffset),
                              value->mCompressedSize);
  }
  MOZ_TRY(Seek(fd, headerStart));
  MOZ_TRY(Write(fd, buf.Get(), buf.cursor()));

  mDirty = false;
  mWrittenOnce = true;

  return Ok();
}

void StartupCache::InvalidateCache(bool memoryOnly) {
  WaitOnPrefetchThread();
  // Ensure we're not writing using mTable...
  MutexAutoLock unlock(mTableLock);

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
    // There should be no way for this assert to fail other than a user manually
    // sending startupcache-invalidate messages through the Browser Toolbox. If
    // something knowingly invalidates the cache, the event can be counted with
    // mAllowedInvalidationsCount.
    MOZ_DIAGNOSTIC_ASSERT(
        xpc::IsInAutomation() ||
            // The allowed invalidations can grow faster than the old tables, so
            // guard against incorrect unsigned subtraction.
            mAllowedInvalidationsCount > mOldTables.Length() ||
            // Now perform the real check.
            mOldTables.Length() - mAllowedInvalidationsCount < 10,
        "Startup cache invalidated too many times.");
    mOldTables.AppendElement(std::move(mTable));
    mCurTableReferenced = false;
  } else {
    mTable.clear();
  }
  mRequestedCount = 0;
  if (!memoryOnly) {
    mCacheData.reset();
    nsresult rv = mFile->Remove(false);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
      gIgnoreDiskCache = true;
      return;
    }
  }
  gIgnoreDiskCache = false;
  auto result = LoadArchive();
  if (NS_WARN_IF(result.isErr())) {
    gIgnoreDiskCache = true;
  }
}

void StartupCache::CountAllowedInvalidation() { mAllowedInvalidationsCount++; }

void StartupCache::MaybeInitShutdownWrite() {
  if (mTimer) {
    mTimer->Cancel();
  }
  gShutdownInitiated = true;

  MaybeWriteOffMainThread();
}

void StartupCache::EnsureShutdownWriteComplete() {
  // If we've already written or there's nothing to write,
  // we don't need to do anything. This is the common case.
  if (mWrittenOnce || (mCacheData.initialized() && !ShouldCompactCache())) {
    return;
  }
  // Otherwise, ensure the write happens. The timer should have been cancelled
  // already in MaybeInitShutdownWrite.
  if (!mTableLock.TryLock()) {
    // Uh oh, we're writing away from the main thread. Wait to gain the lock,
    // to ensure the write completes.
    mTableLock.Lock();
  } else {
    // We got the lock. Keep the following in sync with
    // MaybeWriteOffMainThread:
    WaitOnPrefetchThread();
    mDirty = true;
    mCacheData.reset();
    // Most of this should be redundant given MaybeWriteOffMainThread should
    // have run before now.

    auto writeResult = WriteToDisk();
    Unused << NS_WARN_IF(writeResult.isErr());
    // We've had the lock, and `WriteToDisk()` sets mWrittenOnce and mDirty
    // when done, and checks for them when starting, so we don't need to do
    // anything else.
  }
  mTableLock.Unlock();
}

void StartupCache::IgnoreDiskCache() {
  gIgnoreDiskCache = true;
  if (gStartupCache) gStartupCache->InvalidateCache();
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
  size_t size = startupCacheObj->mCacheData.size();
  MMAP_FAULT_HANDLER_BEGIN_BUFFER(buf, size)
  PrefetchMemory(buf, size);
  MMAP_FAULT_HANDLER_CATCH()
  mozilla::IOInterposer::UnregisterCurrentThread();
}

bool StartupCache::ShouldCompactCache() {
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
  startupCacheObj->MaybeWriteOffMainThread();
}

/*
 * See StartupCache::WriteTimeout above - this is just the non-static body.
 */
void StartupCache::MaybeWriteOffMainThread() {
  if (mWrittenOnce) {
    return;
  }

  if (mCacheData.initialized() && !ShouldCompactCache()) {
    return;
  }

  // Keep this code in sync with EnsureShutdownWriteComplete.
  WaitOnPrefetchThread();
  mDirty = true;
  mCacheData.reset();

  RefPtr<StartupCache> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("StartupCache::Write", [self]() mutable {
        MutexAutoLock unlock(self->mTableLock);
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
    StartupCache::gShutdownInitiated = true;
    // Note that we don't do anything special for the background write
    // task; we expect the threadpool to finish running any tasks already
    // posted to it prior to shutdown. FastShutdown will call
    // EnsureShutdownWriteComplete() to ensure any pending writes happen
    // in that case.
  } else if (strcmp(topic, "startupcache-invalidate") == 0) {
    sc->InvalidateCache(data && nsCRT::strcmp(data, u"memoryOnly") == 0);
  } else if (strcmp(topic, "intl:app-locales-changed") == 0) {
    // Live language switching invalidates the startup cache due to the history
    // sidebar retaining localized strings in its internal SQL query. This
    // should be a relatively rare event, but a user could do it an arbitrary
    // number of times.
    sc->CountAllowedInvalidation();
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
  nsresult rv = NS_OK;
  if (!mTimer)
    mTimer = NS_NewTimer();
  else
    rv = mTimer->Cancel();
  NS_ENSURE_SUCCESS(rv, rv);
  // Wait for the specified timeout, then write out the cache.
  mTimer->InitWithNamedFuncCallback(
      StartupCache::WriteTimeout, this, STARTUP_CACHE_WRITE_TIMEOUT * 1000,
      nsITimer::TYPE_ONE_SHOT, "StartupCache::WriteTimeout");
  return NS_OK;
}

nsresult StartupCache::ResetStartupWriteTimer() {
  mDirty = true;
  nsresult rv = NS_OK;
  if (!mTimer)
    mTimer = NS_NewTimer();
  else
    rv = mTimer->Cancel();
  NS_ENSURE_SUCCESS(rv, rv);
  // Wait for the specified timeout, then write out the cache.
  mTimer->InitWithNamedFuncCallback(
      StartupCache::WriteTimeout, this, STARTUP_CACHE_WRITE_TIMEOUT * 1000,
      nsITimer::TYPE_ONE_SHOT, "StartupCache::WriteTimeout");
  return NS_OK;
}

// Used only in tests:
bool StartupCache::StartupWriteComplete() {
  // Need to have written to disk and not added new things since;
  return !mDirty && mWrittenOnce;
}

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

  bool inserted = mObjectMap->EnsureInserted(aObject);
  if (!inserted) {
    NS_ERROR(
        "non-singleton aObject is referenced multiple times in this"
        "serialization, we don't support that.");
  }

  return inserted;
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
