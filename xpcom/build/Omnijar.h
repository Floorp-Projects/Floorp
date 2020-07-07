/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Omnijar_h
#define mozilla_Omnijar_h

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsZipArchive.h"

#include "mozilla/Span.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class CacheAwareZipReader;

class Omnijar {
 private:
  /**
   * Store an nsIFile for an omni.jar. We can store two paths here, one
   * for GRE (corresponding to resource://gre/) and one for APP
   * (corresponding to resource:/// and resource://app/), but only
   * store one when both point to the same location (unified).
   */
  static StaticRefPtr<nsIFile> sPath[2];

  /**
   * Cached CacheAwareZipReaders for the corresponding sPath
   */
  static StaticRefPtr<CacheAwareZipReader> sReader[2];

  /**
   * Cached CacheAwareZipReaders for the outer jar, when using nested jars.
   * Otherwise nullptr.
   */
  static StaticRefPtr<CacheAwareZipReader> sOuterReader[2];

  /**
   * Has Omnijar::Init() been called?
   */
  static bool sInitialized;

  /**
   * Is using unified GRE/APP jar?
   */
  static bool sIsUnified;

 public:
  enum Type { GRE = 0, APP = 1 };

 private:
  /**
   * Returns whether we are using nested jars.
   */
  static inline bool IsNested(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    return !!sOuterReader[aType];
  }

  /**
   * Returns an CacheAwareZipReader pointer for the outer jar file when using
   * nested jars. Returns nullptr in the same cases GetPath() would, or if not
   * using nested jars.
   */
  static inline already_AddRefed<CacheAwareZipReader> GetOuterReader(
      Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    RefPtr<CacheAwareZipReader> reader = sOuterReader[aType].get();
    return reader.forget();
  }

 public:
  /**
   * Returns whether SetBase has been called at least once with
   * a valid nsIFile
   */
  static inline bool IsInitialized() { return sInitialized; }

  /**
   * Initializes the Omnijar API with the given directory or file for GRE and
   * APP. Each of the paths given can be:
   * - a file path, pointing to the omnijar file,
   * - a directory path, pointing to a directory containing an "omni.jar" file,
   * - nullptr for autodetection of an "omni.jar" file.
   */
  static void Init(nsIFile* aGrePath = nullptr, nsIFile* aAppPath = nullptr);

  /**
   * Cleans up the Omnijar API
   */
  static void CleanUp();

  /**
   * Returns an nsIFile pointing to the omni.jar file for GRE or APP.
   * Returns nullptr when there is no corresponding omni.jar.
   * Also returns nullptr for APP in the unified case.
   */
  static inline already_AddRefed<nsIFile> GetPath(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    nsCOMPtr<nsIFile> path = sPath[aType].get();
    return path.forget();
  }

  /**
   * Returns whether GRE or APP use an omni.jar. Returns PR_False for
   * APP when using an omni.jar in the unified case.
   */
  static inline bool HasOmnijar(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    return !!sPath[aType];
  }

  /**
   * Returns an CacheAwareZipReader pointer for the omni.jar file for GRE or
   * APP. Returns nullptr in the same cases GetPath() would.
   */
  static inline already_AddRefed<CacheAwareZipReader> GetReader(Type aType) {
    MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");
    RefPtr<CacheAwareZipReader> reader = sReader[aType].get();
    return reader.forget();
  }

  /**
   * Returns an CacheAwareZipReader pointer for the given path IAOI the given
   * path is the omni.jar for either GRE or APP.
   */
  static already_AddRefed<CacheAwareZipReader> GetReader(nsIFile* aPath);

  /**
   * In the case of a nested omnijar, this returns the inner reader for the
   * omnijar if aPath points to the outer archive and aEntry is the omnijar
   * entry name. Returns null otherwise.
   * In concrete terms: On Android the omnijar is nested inside the apk archive.
   * GetReader("path/to.apk") returns the outer reader and GetInnerReader(
   * "path/to.apk", "assets/omni.ja") returns the inner reader.
   */
  static already_AddRefed<CacheAwareZipReader> GetInnerReader(
      nsIFile* aPath, const nsACString& aEntry);

  /**
   * Returns the URI string corresponding to the omni.jar or directory
   * for GRE or APP. i.e. jar:/path/to/omni.jar!/ for omni.jar and
   * /path/to/base/dir/ otherwise. Returns an empty string for APP in
   * the unified case.
   * The returned URI is guaranteed to end with a slash.
   */
  static nsresult GetURIString(Type aType, nsACString& aResult);

 private:
  /**
   * Used internally, respectively by Init() and CleanUp()
   */
  static void InitOne(nsIFile* aPath, Type aType);
  static void CleanUpOne(Type aType);
}; /* class Omnijar */

class CacheAwareZipCursor {
 public:
  CacheAwareZipCursor(nsZipItem* aItem, CacheAwareZipReader* aReader,
                      uint8_t* aBuf = nullptr, uint32_t aBufSize = 0,
                      bool aDoCRC = false);

  uint8_t* Read(uint32_t* aBytesRead) { return ReadOrCopy(aBytesRead, false); }
  uint8_t* Copy(uint32_t* aBytesRead) { return ReadOrCopy(aBytesRead, true); }

 private:
  /* Actual implementation for both Read and Copy above */
  uint8_t* ReadOrCopy(uint32_t* aBytesRead, bool aCopy);

  nsZipItem* mItem;
  CacheAwareZipReader* mReader;

  uint8_t* mBuf;
  uint32_t mBufSize;

  bool mDoCRC;
};

// This class wraps an nsZipHandle, which may be null, if the data is
// cached
class CacheAwareZipHandle {
  friend class CacheAwareZipReader;

 public:
  CacheAwareZipHandle() : mFd(nullptr), mDataIsCached(false) {}
  ~CacheAwareZipHandle() { ReleaseHandle(); }

  nsZipHandle* UnderlyingFD() { return mFd; }
  void ReleaseHandle();

  explicit operator bool() const { return mDataIsCached || mFd; }

 private:
  RefPtr<nsZipHandle> mFd;
  nsCString mDeferredCachingKey;
  Span<const uint8_t> mDataToCache;
  bool mDataIsCached;
};

class CacheAwareZipReader {
  friend class CacheAwareZipCursor;
  friend class CacheAwareZipHandle;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheAwareZipReader)

  enum Caching {
    Default,
    DeferCaching,
  };

  // Constructor for CacheAwareZipReader. aCacheKeyPrefix will be a prefix
  // which the wrapper will prepend to any requested entries prior to
  // requesting the entry from the StartupCache. However, if aCacheKeyPrefix
  // is null, we will simply pass through to the underlying zip archive
  // without caching.
  explicit CacheAwareZipReader(nsZipArchive* aZip, const char* aCacheKeyPrefix);

  // The default constructor should be used for an nsZipArchive which doesn't
  // want to cache any entries. Consumers will need to call `OpenArchive`
  // explicitly after constructing.
  CacheAwareZipReader() : mZip(new nsZipArchive()) {}

  nsresult OpenArchive(nsIFile* aFile) { return mZip->OpenArchive(aFile); }
  nsresult OpenArchive(nsZipHandle* aHandle) {
    return mZip->OpenArchive(aHandle);
  }

  const uint8_t* GetData(const char* aEntryName, uint32_t* aResultSize,
                         Caching aCaching = Default);
  const uint8_t* GetData(nsZipItem* aItem, Caching aCaching = Default);

  nsresult GetPersistentHandle(nsZipItem* aItem, CacheAwareZipHandle* aHandle,
                               Caching aCaching);

  already_AddRefed<nsIFile> GetBaseFile() { return mZip->GetBaseFile(); }

  void GetURIString(nsACString& result) { mZip->GetURIString(result); }

  nsZipArchive* GetZipArchive() { return mZip; }

  nsresult FindInit(const char* aPattern, nsZipFind** aFind);
  bool IsForZip(nsZipArchive* aArchive) { return aArchive == mZip; }
  nsZipItem* GetItem(const char* aEntryName);

  nsresult CloseArchive();

  nsresult Test(const char* aEntryName) { return mZip->Test(aEntryName); }

  nsresult ExtractFile(nsZipItem* zipEntry, nsIFile* outFile,
                       PRFileDesc* outFD) {
    return mZip->ExtractFile(zipEntry, outFile, outFD);
  }

  static void PushSuspendStartupCacheWrites();
  static void PopSuspendStartupCacheWrites();

 protected:
  ~CacheAwareZipReader() = default;

 private:
  const uint8_t* GetCachedBuffer(const char* aEntryName,
                                 uint32_t aEntryNameLength,
                                 uint32_t* aResultSize, nsCString& aCacheKey);
  static void PutBufferIntoCache(const nsCString& aCacheKey,
                                 const uint8_t* aBuffer, uint32_t aSize);

  RefPtr<nsZipArchive> mZip;
  nsCString mCacheKeyPrefix;
};

class MOZ_RAII AutoSuspendStartupCacheWrites {
 public:
  AutoSuspendStartupCacheWrites() {
    CacheAwareZipReader::PushSuspendStartupCacheWrites();
  }

  ~AutoSuspendStartupCacheWrites() {
    CacheAwareZipReader::PopSuspendStartupCacheWrites();
  }
};

} /* namespace mozilla */

#endif /* mozilla_Omnijar_h */
