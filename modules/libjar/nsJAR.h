/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsJAR_h__
#define nsJAR_h__

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"
#include "prtypes.h"
#include "prinrval.h"

#include "mozilla/Mutex.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsStringEnumerator.h"
#include "nsHashtable.h"
#include "nsIZipReader.h"
#include "nsZipArchive.h"
#include "nsIPrincipal.h"
#include "nsISignatureVerifier.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"

class nsIInputStream;
class nsJARManifestItem;
class nsZipReaderCache;

/* For mManifestStatus */
typedef enum
{
  JAR_MANIFEST_NOT_PARSED = 0,
  JAR_VALID_MANIFEST      = 1,
  JAR_INVALID_SIG         = 2,
  JAR_INVALID_UNKNOWN_CA  = 3,
  JAR_INVALID_MANIFEST    = 4,
  JAR_INVALID_ENTRY       = 5,
  JAR_NO_MANIFEST         = 6,
  JAR_NOT_SIGNED          = 7
} JARManifestStatusType;

/*-------------------------------------------------------------------------
 * Class nsJAR declaration. 
 * nsJAR serves as an XPCOM wrapper for nsZipArchive with the addition of 
 * JAR manifest file parsing. 
 *------------------------------------------------------------------------*/
class nsJAR : public nsIZipReader
{
  // Allows nsJARInputStream to call the verification functions
  friend class nsJARInputStream;
  // Allows nsZipReaderCache to access mOuterZipEntry
  friend class nsZipReaderCache;

  public:

    nsJAR();
    virtual ~nsJAR();
    
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_ZIPREADER_CID )
  
    NS_DECL_ISUPPORTS

    NS_DECL_NSIZIPREADER

    nsresult GetJarPath(nsACString& aResult);

    PRIntervalTime GetReleaseTime() {
        return mReleaseTime;
    }
    
    bool IsReleased() {
        return mReleaseTime != PR_INTERVAL_NO_TIMEOUT;
    }

    void SetReleaseTime() {
      mReleaseTime = PR_IntervalNow();
    }
    
    void ClearReleaseTime() {
      mReleaseTime = PR_INTERVAL_NO_TIMEOUT;
    }
    
    void SetZipReaderCache(nsZipReaderCache* cache) {
      mCache = cache;
    }

  protected:
    //-- Private data members
    nsCOMPtr<nsIFile>        mZipFile;        // The zip/jar file on disk
    nsCString                mOuterZipEntry;  // The entry in the zip this zip is reading from
    nsRefPtr<nsZipArchive>   mZip;            // The underlying zip archive
    nsObjectHashtable        mManifestData;   // Stores metadata for each entry
    bool                     mParsedManifest; // True if manifest has been parsed
    nsCOMPtr<nsIPrincipal>   mPrincipal;      // The entity which signed this file
    PRInt16                  mGlobalStatus;   // Global signature verification status
    PRIntervalTime           mReleaseTime;    // used by nsZipReaderCache for flushing entries
    nsZipReaderCache*        mCache;          // if cached, this points to the cache it's contained in
    mozilla::Mutex           mLock;	
    PRInt64                  mMtime;
    PRInt32                  mTotalItemsInManifest;
    bool                     mOpened;

    nsresult ParseManifest();
    void     ReportError(const nsACString &aFilename, PRInt16 errorCode);
    nsresult LoadEntry(const nsACString &aFilename, char** aBuf, 
                       PRUint32* aBufLen = nsnull);
    PRInt32  ReadLine(const char** src); 
    nsresult ParseOneFile(const char* filebuf, PRInt16 aFileType);
    nsresult VerifyEntry(nsJARManifestItem* aEntry, const char* aEntryData, 
                         PRUint32 aLen);

    nsresult CalculateDigest(const char* aInBuf, PRUint32 aInBufLen,
                             nsCString& digest);
};

/**
 * nsJARItem
 *
 * An individual JAR entry. A set of nsJARItems macthing a
 * supplied pattern are returned in a nsJAREnumerator.
 */
class nsJARItem : public nsIZipEntry
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIZIPENTRY
    
    nsJARItem(nsZipItem* aZipItem);
    virtual ~nsJARItem() {}

private:
    PRUint32     mSize;             /* size in original file */
    PRUint32     mRealsize;         /* inflated size */
    PRUint32     mCrc32;
    PRTime       mLastModTime;
    PRUint16     mCompression;
    bool mIsDirectory; 
    bool mIsSynthetic;
};

/**
 * nsJAREnumerator
 *
 * Enumerates a list of files in a zip archive 
 * (based on a pattern match in its member nsZipFind).
 */
class nsJAREnumerator MOZ_FINAL : public nsIUTF8StringEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIUTF8STRINGENUMERATOR

    nsJAREnumerator(nsZipFind *aFind) : mFind(aFind), mName(nsnull) { 
      NS_ASSERTION(mFind, "nsJAREnumerator: Missing zipFind.");
    }

private:
    nsZipFind    *mFind;
    const char*   mName;    // pointer to an name owned by mArchive -- DON'T delete
    PRUint16      mNameLen;

    ~nsJAREnumerator() { delete mFind; }
};

////////////////////////////////////////////////////////////////////////////////

#if defined(DEBUG_warren) || defined(DEBUG_jband)
#define ZIP_CACHE_HIT_RATE
#endif

class nsZipReaderCache : public nsIZipReaderCache, public nsIObserver,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIZIPREADERCACHE
  NS_DECL_NSIOBSERVER

  nsZipReaderCache();
  virtual ~nsZipReaderCache();

  nsresult ReleaseZip(nsJAR* reader);

protected:
  mozilla::Mutex        mLock;
  PRInt32               mCacheSize;
  nsSupportsHashtable   mZips;

#ifdef ZIP_CACHE_HIT_RATE
  PRUint32              mZipCacheLookups;
  PRUint32              mZipCacheHits;
  PRUint32              mZipCacheFlushes;
  PRUint32              mZipSyncMisses;
#endif

};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsJAR_h__ */
