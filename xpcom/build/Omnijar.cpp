/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Omnijar.h"

#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsZipArchive.h"
#include "nsNetUtil.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/MmapFaultHandler.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {

StaticRefPtr<nsIFile> Omnijar::sPath[2];
StaticRefPtr<CacheAwareZipReader> Omnijar::sReader[2];
StaticRefPtr<CacheAwareZipReader> Omnijar::sOuterReader[2];
bool Omnijar::sInitialized = false;
bool Omnijar::sIsUnified = false;

static MOZ_THREAD_LOCAL(int) tlsSuspendStartupCacheWrites;

bool SuspendingStartupCacheWritesForCurrentThread() {
  if (!tlsSuspendStartupCacheWrites.init()) {
    return true;
  }
  return tlsSuspendStartupCacheWrites.get() > 0;
}

static const char* sProp[2] = {NS_GRE_DIR, NS_XPCOM_CURRENT_PROCESS_DIR};
static const char* sCachePrefixes[2] = {"GreOmnijar:", "AppOmnijar:"};

#define SPROP(Type) ((Type == mozilla::Omnijar::GRE) ? sProp[GRE] : sProp[APP])

void Omnijar::CleanUpOne(Type aType) {
  if (sReader[aType]) {
    sReader[aType]->CloseArchive();
    sReader[aType] = nullptr;
  }
  if (sOuterReader[aType]) {
    sOuterReader[aType]->CloseArchive();
    sOuterReader[aType] = nullptr;
  }
  sPath[aType] = nullptr;
}

void Omnijar::InitOne(nsIFile* aPath, Type aType) {
  nsCOMPtr<nsIFile> file;
  if (aPath) {
    file = aPath;
  } else {
    nsCOMPtr<nsIFile> dir;
    nsDirectoryService::gService->Get(SPROP(aType), NS_GET_IID(nsIFile),
                                      getter_AddRefs(dir));
    constexpr auto kOmnijarName = nsLiteralCString{MOZ_STRINGIFY(OMNIJAR_NAME)};
    if (NS_FAILED(dir->Clone(getter_AddRefs(file))) ||
        NS_FAILED(file->AppendNative(kOmnijarName))) {
      return;
    }
  }
  bool isFile;
  if (NS_FAILED(file->IsFile(&isFile)) || !isFile) {
    // If we're not using an omni.jar for GRE, and we don't have an
    // omni.jar for APP, check if both directories are the same.
    if ((aType == APP) && (!sPath[GRE])) {
      nsCOMPtr<nsIFile> greDir, appDir;
      bool equals;
      nsDirectoryService::gService->Get(sProp[GRE], NS_GET_IID(nsIFile),
                                        getter_AddRefs(greDir));
      nsDirectoryService::gService->Get(sProp[APP], NS_GET_IID(nsIFile),
                                        getter_AddRefs(appDir));
      if (NS_SUCCEEDED(greDir->Equals(appDir, &equals)) && equals) {
        sIsUnified = true;
      }
    }
    return;
  }

  bool equals;
  if ((aType == APP) && (sPath[GRE]) &&
      NS_SUCCEEDED(sPath[GRE]->Equals(file, &equals)) && equals) {
    // If we're using omni.jar on both GRE and APP and their path
    // is the same, we're in the unified case.
    sIsUnified = true;
    return;
  }

  RefPtr<nsZipArchive> zipReader = new nsZipArchive();
  auto* cache = scache::StartupCache::GetSingleton();
  const uint8_t* centralBuf = nullptr;
  uint32_t centralBufLength = 0;
  nsCString startupCacheKey =
      nsPrintfCString("::%s:OmnijarCentral", sCachePrefixes[aType]);
  if (cache) {
    nsresult rv = cache->GetBuffer(startupCacheKey.get(),
                                   reinterpret_cast<const char**>(&centralBuf),
                                   &centralBufLength);
    if (NS_FAILED(rv)) {
      centralBuf = nullptr;
      centralBufLength = 0;
    }
  }

  if (!centralBuf) {
    if (NS_FAILED(zipReader->OpenArchive(file))) {
      return;
    }
    if (cache) {
      size_t bufSize;
      // Annoyingly, nsZipArchive and the startupcache use different types to
      // represent bytes (uint8_t vs char), so we have to do a little dance to
      // convert the UniquePtr over.
      UniquePtr<char[]> centralBuf(reinterpret_cast<char*>(
          zipReader->CopyCentralDirectoryBuffer(&bufSize).release()));
      if (centralBuf) {
        cache->PutBuffer(startupCacheKey.get(), std::move(centralBuf), bufSize);
      }
    }
  } else {
    if (NS_FAILED(zipReader->LazyOpenArchive(
            file, MakeSpan(centralBuf, centralBufLength)))) {
      return;
    }
  }

  RefPtr<nsZipArchive> outerReader;
  RefPtr<nsZipHandle> handle;
  if (NS_SUCCEEDED(nsZipHandle::Init(zipReader, MOZ_STRINGIFY(OMNIJAR_NAME),
                                     getter_AddRefs(handle)))) {
    outerReader = zipReader;
    zipReader = new nsZipArchive();
    if (NS_FAILED(zipReader->OpenArchive(handle))) {
      return;
    }
  }

  CleanUpOne(aType);
  sReader[aType] = new CacheAwareZipReader(zipReader, sCachePrefixes[aType]);
  sOuterReader[aType] =
      outerReader ? new CacheAwareZipReader(outerReader, nullptr) : nullptr;
  sPath[aType] = file;
}

void Omnijar::Init(nsIFile* aGrePath, nsIFile* aAppPath) {
  InitOne(aGrePath, GRE);
  InitOne(aAppPath, APP);
  sInitialized = true;
}

void Omnijar::CleanUp() {
  CleanUpOne(GRE);
  CleanUpOne(APP);
  sInitialized = false;
}

already_AddRefed<CacheAwareZipReader> Omnijar::GetReader(nsIFile* aPath) {
  MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");

  bool equals;
  nsresult rv;

  if (sPath[GRE]) {
    rv = sPath[GRE]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(GRE) ? GetOuterReader(GRE) : GetReader(GRE);
    }
  }
  if (sPath[APP]) {
    rv = sPath[APP]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(APP) ? GetOuterReader(APP) : GetReader(APP);
    }
  }
  return nullptr;
}

already_AddRefed<CacheAwareZipReader> Omnijar::GetInnerReader(
    nsIFile* aPath, const nsACString& aEntry) {
  MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");

  if (!aEntry.EqualsLiteral(MOZ_STRINGIFY(OMNIJAR_NAME))) {
    return nullptr;
  }

  bool equals;
  nsresult rv;

  if (sPath[GRE]) {
    rv = sPath[GRE]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(GRE) ? GetReader(GRE) : nullptr;
    }
  }
  if (sPath[APP]) {
    rv = sPath[APP]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(APP) ? GetReader(APP) : nullptr;
    }
  }
  return nullptr;
}

nsresult Omnijar::GetURIString(Type aType, nsACString& aResult) {
  MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");

  aResult.Truncate();

  // Return an empty string for APP in the unified case.
  if ((aType == APP) && sIsUnified) {
    return NS_OK;
  }

  nsAutoCString omniJarSpec;
  if (sPath[aType]) {
    nsresult rv = NS_GetURLSpecFromActualFile(sPath[aType], omniJarSpec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aResult = "jar:";
    if (IsNested(aType)) {
      aResult += "jar:";
    }
    aResult += omniJarSpec;
    aResult += "!";
    if (IsNested(aType)) {
      aResult += "/" MOZ_STRINGIFY(OMNIJAR_NAME) "!";
    }
  } else {
    nsCOMPtr<nsIFile> dir;
    nsDirectoryService::gService->Get(SPROP(aType), NS_GET_IID(nsIFile),
                                      getter_AddRefs(dir));
    nsresult rv = NS_GetURLSpecFromActualFile(dir, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  aResult += "/";
  return NS_OK;
}

CacheAwareZipReader::CacheAwareZipReader(nsZipArchive* aZip,
                                         const char* aCacheKeyPrefix)
    : mZip(aZip), mCacheKeyPrefix(aCacheKeyPrefix) {}

nsresult CacheAwareZipReader::FindInit(const char* aPattern,
                                       nsZipFind** aFind) {
  return mZip->FindInit(aPattern, aFind);
}

const uint8_t* CacheAwareZipReader::GetData(
    nsZipItem* aItem, CacheAwareZipReader::Caching aCaching) {
  nsAutoCString cacheKey;
  uint32_t size;
  const uint8_t* cached =
      GetCachedBuffer(aItem->Name(), aItem->nameLength, &size, cacheKey);
  if (cached) {
    MOZ_ASSERT(size == aItem->RealSize());
    return cached;
  }

  const uint8_t* zipItemData = mZip->GetData(aItem);
  if (!zipItemData) {
    return nullptr;
  }

  // If the data is compressed, it is somewhat silly to store it in the startup
  // cache, as the startup cache will try to double compress it.
  if (aCaching == Default && aItem->Compression() == STORED &&
      !cacheKey.IsEmpty()) {
    MOZ_ASSERT(aItem->RealSize() == aItem->Size());
    PutBufferIntoCache(cacheKey, zipItemData, aItem->Size());
  }

  return zipItemData;
}

const uint8_t* CacheAwareZipReader::GetData(
    const char* aEntryName, uint32_t* aResultSize,
    CacheAwareZipReader::Caching aCaching) {
  nsAutoCString cacheKey;
  const uint8_t* cached =
      GetCachedBuffer(aEntryName, strlen(aEntryName), aResultSize, cacheKey);
  if (cached) {
    return cached;
  }

  nsZipItem* zipItem = mZip->GetItem(aEntryName);
  if (!zipItem) {
    *aResultSize = 0;
    return nullptr;
  }
  const uint8_t* zipItemData = mZip->GetData(zipItem);
  if (!zipItemData) {
    return nullptr;
  }

  *aResultSize = zipItem->Size();

  // If the data is compressed, it is somewhat silly to store it in the startup
  // cache, as the startup cache will try to double compress it.
  if (aCaching == Default && zipItem->Compression() == STORED &&
      !cacheKey.IsEmpty()) {
    MOZ_ASSERT(zipItem->RealSize() == *aResultSize);
    PutBufferIntoCache(cacheKey, zipItemData, *aResultSize);
  }

  return zipItemData;
}

nsZipItem* CacheAwareZipReader::GetItem(const char* aEntryName) {
  return mZip->GetItem(aEntryName);
}

nsresult CacheAwareZipReader::CloseArchive() { return mZip->CloseArchive(); }

CacheAwareZipCursor::CacheAwareZipCursor(nsZipItem* aItem,
                                         CacheAwareZipReader* aReader,
                                         uint8_t* aBuf, uint32_t aBufSize,
                                         bool aDoCRC)
    : mItem(aItem),
      mReader(aReader),
      mBuf(aBuf),
      mBufSize(aBufSize),
      mDoCRC(aDoCRC) {}

uint8_t* CacheAwareZipCursor::ReadOrCopy(uint32_t* aBytesRead, bool aCopy) {
  nsCString cacheKey;
  const uint8_t* cached = mReader->GetCachedBuffer(
      mItem->Name(), mItem->nameLength, aBytesRead, cacheKey);
  if (cached && *aBytesRead <= mBufSize) {
    if (aCopy) {
      memcpy(mBuf, cached, *aBytesRead);
      return mBuf;
    }

    // The const cast is unfortunate, but it matches existing consumers'
    // uses. We ought to file a bug to make Read return a const uint8_t*
    return const_cast<uint8_t*>(cached);
  }

  nsZipCursor cursor(mItem, mReader->mZip, mBuf, mBufSize, mDoCRC);
  uint8_t* buf = nullptr;
  if (aCopy) {
    cursor.Copy(aBytesRead);
    buf = mBuf;
  } else {
    buf = cursor.Read(aBytesRead);
  }

  if (!buf) {
    return nullptr;
  }

  if (!cacheKey.IsEmpty() && *aBytesRead == mItem->RealSize()) {
    CacheAwareZipReader::PutBufferIntoCache(cacheKey, buf, *aBytesRead);
  }

  return buf;
}

nsresult CacheAwareZipReader::GetPersistentHandle(
    nsZipItem* aItem, CacheAwareZipHandle* aHandle,
    CacheAwareZipReader::Caching aCaching) {
  nsCString cacheKey;
  if (!mCacheKeyPrefix.IsEmpty() && aItem->Compression() == STORED) {
    auto* cache = scache::StartupCache::GetSingleton();
    if (cache) {
      cacheKey.Append(mCacheKeyPrefix);
      cacheKey.Append(aItem->Name(), aItem->nameLength);

      if (cache->HasEntry(cacheKey.get())) {
        aHandle->mDataIsCached = true;
        aHandle->mFd = nullptr;
        return NS_OK;
      }
      if (aCaching == DeferCaching) {
        aHandle->mDeferredCachingKey = std::move(cacheKey);
      }
    }
  }

  nsresult rv = mZip->EnsureArchiveOpenedOnDisk();
  if (NS_FAILED(rv)) {
    return rv;
  }

  aHandle->mDataIsCached = false;
  aHandle->mFd = mZip->GetFD();

  if (!aHandle->mDeferredCachingKey.IsEmpty() &&
      aItem->Compression() == STORED) {
    MOZ_ASSERT(aItem->RealSize() == aItem->Size());
    const uint8_t* data = mZip->GetData(aItem);
    if (data) {
      aHandle->mDataToCache = MakeSpan(data, aItem->Size());
    }
  }

  return NS_OK;
}

const uint8_t* CacheAwareZipReader::GetCachedBuffer(const char* aEntryName,
                                                    uint32_t aEntryNameLength,
                                                    uint32_t* aResultSize,
                                                    nsCString& aCacheKey) {
  *aResultSize = 0;
  if (mCacheKeyPrefix.IsEmpty()) {
    return nullptr;
  }
  auto* cache = scache::StartupCache::GetSingleton();
  if (!cache) {
    return nullptr;
  }

  aCacheKey.Append(mCacheKeyPrefix);
  aCacheKey.Append(aEntryName, aEntryNameLength);

  const char* cached;
  nsresult rv = cache->GetBuffer(aCacheKey.get(), &cached, aResultSize);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return reinterpret_cast<const uint8_t*>(cached);
}

void CacheAwareZipReader::PutBufferIntoCache(const nsCString& aCacheKey,
                                             const uint8_t* aBuffer,
                                             uint32_t aSize) {
  if (SuspendingStartupCacheWritesForCurrentThread() || aSize == 0) {
    return;
  }

  auto* cache = scache::StartupCache::GetSingleton();
  auto dataCopy = MakeUniqueFallible<char[]>(aSize);

  if (dataCopy) {
    MMAP_FAULT_HANDLER_BEGIN_BUFFER(aBuffer, aSize)
    memcpy(dataCopy.get(), aBuffer, aSize);
    MMAP_FAULT_HANDLER_CATCH()
    Unused << cache->PutBuffer(aCacheKey.get(), std::move(dataCopy), aSize);
  }
}

void CacheAwareZipReader::PushSuspendStartupCacheWrites() {
  if (!tlsSuspendStartupCacheWrites.init()) {
    return;
  }
  tlsSuspendStartupCacheWrites.set(tlsSuspendStartupCacheWrites.get() + 1);
}

void CacheAwareZipReader::PopSuspendStartupCacheWrites() {
  if (!tlsSuspendStartupCacheWrites.init()) {
    return;
  }
  int current = tlsSuspendStartupCacheWrites.get();
  MOZ_ASSERT(current > 0);
  tlsSuspendStartupCacheWrites.set(current - 1);
}

void CacheAwareZipHandle::ReleaseHandle() {
  if (!mDataToCache.IsEmpty()) {
    MOZ_ASSERT(mFd);
    MOZ_ASSERT(!mDeferredCachingKey.IsEmpty());
    MOZ_ASSERT(!mDataIsCached);

    auto* cache = scache::StartupCache::GetSingleton();
    MOZ_ASSERT(cache);
    if (cache) {
      CacheAwareZipReader::PutBufferIntoCache(
          mDeferredCachingKey, mDataToCache.Elements(), mDataToCache.Length());
      mDataToCache = Span<const uint8_t>();
    }
  }

  mFd = nullptr;
}

} /* namespace mozilla */
