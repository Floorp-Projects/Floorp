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
#include "mozilla/Mutex.h"
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

  nsresult GetJarPath(nsACString& aResult);

  PRIntervalTime GetReleaseTime() { return mReleaseTime; }

  bool IsReleased() { return mReleaseTime != PR_INTERVAL_NO_TIMEOUT; }

  void SetReleaseTime() { mReleaseTime = PR_IntervalNow(); }

  void ClearReleaseTime() { mReleaseTime = PR_INTERVAL_NO_TIMEOUT; }

  void SetZipReaderCache(nsZipReaderCache* aCache) {
    mozilla::MutexAutoLock lock(mLock);
    mCache = aCache;
  }

  nsresult GetNSPRFileDesc(PRFileDesc** aNSPRFileDesc);

 protected:
  //-- Private data members
  nsCOMPtr<nsIFile> mZipFile;   // The zip/jar file on disk
  nsCString mOuterZipEntry;     // The entry in the zip this zip is reading from
  RefPtr<nsZipArchive> mZip;    // The underlying zip archive
  PRIntervalTime mReleaseTime;  // used by nsZipReaderCache for flushing entries
  nsZipReaderCache*
      mCache;  // if cached, this points to the cache it's contained in
  mozilla::Mutex mLock;  // protect mCache and mZip
  int64_t mMtime;
  bool mOpened;

  // true if mZip was adopted from elsewhere and should not be closed by us.
  bool mSkipArchiveClosing;

  nsresult LoadEntry(const nsACString& aFilename, nsCString& aBuf);
  int32_t ReadLine(const char** src);
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

  uint32_t mSize;     /* size in original file */
  uint32_t mRealsize; /* inflated size */
  uint32_t mCrc32;
  PRTime mLastModTime;
  uint16_t mCompression;
  uint32_t mPermissions;
  bool mIsDirectory;
  bool mIsSynthetic;
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
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIZIPREADERCACHE
  NS_DECL_NSIOBSERVER

  nsZipReaderCache();

  nsresult ReleaseZip(nsJAR* reader);

  typedef nsRefPtrHashtable<nsCStringHashKey, nsJAR> ZipsHashtable;

 protected:
  virtual ~nsZipReaderCache();

  mozilla::Mutex mLock;
  uint32_t mCacheSize;
  ZipsHashtable mZips;

#ifdef ZIP_CACHE_HIT_RATE
  uint32_t mZipCacheLookups;
  uint32_t mZipCacheHits;
  uint32_t mZipCacheFlushes;
  uint32_t mZipSyncMisses;
#endif

 private:
  nsresult GetZip(nsIFile* zipFile, nsIZipReader** result, bool failOnMiss);
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsJAR_h_ */
