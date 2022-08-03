/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJAR_h_
#define nsJAR_h_

#include "nscore.h"
#include "prio.h"
#include "plstr.h"
#include "mozilla/Logging.h"
#include "prinrval.h"

#include "mozilla/Atomics.h"
#include "mozilla/RecursiveMutex.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsStringEnumerator.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashtable.h"
#include "nsIZipReader.h"
#include "nsZipArchive.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"

class nsZipReaderCache;

/*-------------------------------------------------------------------------
 * Class nsJAR declaration.
 * nsJAR serves as an XPCOM wrapper for nsZipArchive with the addition of
 * JAR manifest file parsing.
 *------------------------------------------------------------------------*/
class nsJAR final : public nsIZipReader {
  // Allows nsJARInputStream to call the verification functions
  friend class nsJARInputStream;
  // Allows nsZipReaderCache to access mOuterZipEntry
  friend class nsZipReaderCache;

 private:
  virtual ~nsJAR();

 public:
  nsJAR();

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_ZIPREADER_CID)

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSIZIPREADER

  nsresult GetFullJarPath(nsACString& aResult);

  // These access to mReleaseTime, which is locked by nsZipReaderCache's
  // mLock, not nsJAR's mLock
  PRIntervalTime GetReleaseTime() { return mReleaseTime; }

  bool IsReleased() { return mReleaseTime != PR_INTERVAL_NO_TIMEOUT; }

  void SetReleaseTime() { mReleaseTime = PR_IntervalNow(); }

  void ClearReleaseTime() { mReleaseTime = PR_INTERVAL_NO_TIMEOUT; }

  void SetZipReaderCache(nsZipReaderCache* aCache) {
    mozilla::RecursiveMutexAutoLock lock(mLock);
    mCache = aCache;
  }

  nsresult GetNSPRFileDesc(PRFileDesc** aNSPRFileDesc);

 protected:
  nsresult LoadEntry(const nsACString& aFilename, nsCString& aBuf);
  int32_t ReadLine(const char** src);

  // used by nsZipReaderCache for flushing entries; access is locked by
  // nsZipReaderCache's mLock
  PRIntervalTime mReleaseTime;

  //-- Private data members, protected by mLock
  mozilla::RecursiveMutex mLock;
  // The entry in the zip this zip is reading from
  nsCString mOuterZipEntry MOZ_GUARDED_BY(mLock);
  // The zip/jar file on disk
  nsCOMPtr<nsIFile> mZipFile MOZ_GUARDED_BY(mLock);
  // The underlying zip archive
  RefPtr<nsZipArchive> mZip MOZ_GUARDED_BY(mLock);
  // if cached, this points to the cache it's contained in
  nsZipReaderCache* mCache MOZ_GUARDED_BY(mLock);
};

/**
 * nsJARItem
 *
 * An individual JAR entry. A set of nsJARItems matching a
 * supplied pattern are returned in a nsJAREnumerator.
 */
class nsJARItem : public nsIZipEntry {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIZIPENTRY

  explicit nsJARItem(nsZipItem* aZipItem);

 private:
  virtual ~nsJARItem() {}

  const uint32_t mSize;     /* size in original file */
  const uint32_t mRealsize; /* inflated size */
  const uint32_t mCrc32;
  const PRTime mLastModTime;
  const uint16_t mCompression;
  const uint32_t mPermissions;
  const bool mIsDirectory;
  const bool mIsSynthetic;
};

/**
 * nsJAREnumerator
 *
 * Enumerates a list of files in a zip archive
 * (based on a pattern match in its member nsZipFind).
 */
class nsJAREnumerator final : public nsStringEnumeratorBase {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUTF8STRINGENUMERATOR

  using nsStringEnumeratorBase::GetNext;

  explicit nsJAREnumerator(nsZipFind* aFind)
      : mFind(aFind), mName(nullptr), mNameLen(0) {
    NS_ASSERTION(mFind, "nsJAREnumerator: Missing zipFind.");
  }

 private:
  nsZipFind* mFind;
  const char* mName;  // pointer to an name owned by mArchive -- DON'T delete
  uint16_t mNameLen;

  ~nsJAREnumerator() { delete mFind; }
};

////////////////////////////////////////////////////////////////////////////////

#if defined(DEBUG_warren) || defined(DEBUG_jband)
#  define ZIP_CACHE_HIT_RATE
#endif

class nsZipReaderCache : public nsIZipReaderCache,
                         public nsIObserver,
                         public nsSupportsWeakReference {
  friend class nsJAR;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIZIPREADERCACHE
  NS_DECL_NSIOBSERVER

  nsZipReaderCache();

  nsresult ReleaseZip(nsJAR* reader);

  typedef nsRefPtrHashtable<nsCStringHashKey, nsJAR> ZipsHashtable;

 protected:
  void AssertLockOwned() { mLock.AssertCurrentThreadOwns(); }

  virtual ~nsZipReaderCache();

  mozilla::Mutex mLock;
  uint32_t mCacheSize MOZ_GUARDED_BY(mLock);
  ZipsHashtable mZips MOZ_GUARDED_BY(mLock);

#ifdef ZIP_CACHE_HIT_RATE
  uint32_t mZipCacheLookups MOZ_GUARDED_BY(mLock);
  uint32_t mZipCacheHits MOZ_GUARDED_BY(mLock);
  uint32_t mZipCacheFlushes MOZ_GUARDED_BY(mLock);
  uint32_t mZipSyncMisses MOZ_GUARDED_BY(mLock);
#endif

 private:
  nsresult GetZip(nsIFile* zipFile, nsIZipReader** result, bool failOnMiss);
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsJAR_h_ */
