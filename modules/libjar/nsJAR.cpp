/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <string.h>
#include "nsJARInputStream.h"
#include "nsJAR.h"
#include "nsIFile.h"
#include "nsIConsoleService.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Unused.h"

#ifdef XP_UNIX
  #include <sys/stat.h>
#elif defined (XP_WIN)
  #include <io.h>
#endif

using namespace mozilla;

//----------------------------------------------
// nsJAR constructor/destructor
//----------------------------------------------

// The following initialization makes a guess of 10 entries per jarfile.
nsJAR::nsJAR(): mZip(new nsZipArchive()),
                mReleaseTime(PR_INTERVAL_NO_TIMEOUT),
                mCache(nullptr),
                mLock("nsJAR::mLock"),
                mMtime(0),
                mOpened(false),
                mIsOmnijar(false)
{
}

nsJAR::~nsJAR()
{
  Close();
}

NS_IMPL_QUERY_INTERFACE(nsJAR, nsIZipReader)
NS_IMPL_ADDREF(nsJAR)

// Custom Release method works with nsZipReaderCache...
// Release might be called from multi-thread, we have to
// take this function carefully to avoid delete-after-use.
MozExternalRefCountType nsJAR::Release(void)
{
  nsrefcnt count;
  MOZ_ASSERT(0 != mRefCnt, "dup release");

  RefPtr<nsZipReaderCache> cache;
  if (mRefCnt == 2) { // don't use a lock too frequently
    // Use a mutex here to guarantee mCache is not racing and the target instance
    // is still valid to increase ref-count.
    MutexAutoLock lock(mLock);
    cache = mCache;
    mCache = nullptr;
  }
  if (cache) {
    DebugOnly<nsresult> rv = cache->ReleaseZip(this);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to release zip file");
  }

  count = --mRefCnt; // don't access any member variable after this line
  NS_LOG_RELEASE(this, count, "nsJAR");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(nsJAR); */
    delete this;
    return 0;
  }

  return count;
}

//----------------------------------------------
// nsIZipReader implementation
//----------------------------------------------

NS_IMETHODIMP
nsJAR::Open(nsIFile* zipFile)
{
  NS_ENSURE_ARG_POINTER(zipFile);
  if (mOpened) return NS_ERROR_FAILURE; // Already open!

  mZipFile = zipFile;
  mOuterZipEntry.Truncate();
  mOpened = true;

  // The omnijar is special, it is opened early on and closed late
  // this avoids reopening it
  RefPtr<nsZipArchive> zip = mozilla::Omnijar::GetReader(zipFile);
  if (zip) {
    mZip = zip;
    mIsOmnijar = true;
    return NS_OK;
  }
  return mZip->OpenArchive(zipFile);
}

NS_IMETHODIMP
nsJAR::OpenInner(nsIZipReader *aZipReader, const nsACString &aZipEntry)
{
  NS_ENSURE_ARG_POINTER(aZipReader);
  if (mOpened) return NS_ERROR_FAILURE; // Already open!

  bool exist;
  nsresult rv = aZipReader->HasEntry(aZipEntry, &exist);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(exist, NS_ERROR_FILE_NOT_FOUND);

  rv = aZipReader->GetFile(getter_AddRefs(mZipFile));
  NS_ENSURE_SUCCESS(rv, rv);

  mOpened = true;

  mOuterZipEntry.Assign(aZipEntry);

  RefPtr<nsZipHandle> handle;
  rv = nsZipHandle::Init(static_cast<nsJAR*>(aZipReader)->mZip.get(), PromiseFlatCString(aZipEntry).get(),
                         getter_AddRefs(handle));
  if (NS_FAILED(rv))
    return rv;

  return mZip->OpenArchive(handle);
}

NS_IMETHODIMP
nsJAR::OpenMemory(void* aData, uint32_t aLength)
{
  NS_ENSURE_ARG_POINTER(aData);
  if (mOpened) return NS_ERROR_FAILURE; // Already open!

  mOpened = true;

  RefPtr<nsZipHandle> handle;
  nsresult rv = nsZipHandle::Init(static_cast<uint8_t*>(aData), aLength,
                                  getter_AddRefs(handle));
  if (NS_FAILED(rv))
    return rv;

  return mZip->OpenArchive(handle);
}

NS_IMETHODIMP
nsJAR::GetFile(nsIFile* *result)
{
  *result = mZipFile;
  NS_IF_ADDREF(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::Close()
{
  if (!mOpened) {
    return NS_ERROR_FAILURE; // Never opened or already closed.
  }

  mOpened = false;

  if (mIsOmnijar) {
    // Reset state, but don't close the omnijar because we did not open it.
    mIsOmnijar = false;
    mZip = new nsZipArchive();
    return NS_OK;
  }

  return mZip->CloseArchive();
}

NS_IMETHODIMP
nsJAR::Test(const nsACString &aEntryName)
{
  return mZip->Test(aEntryName.IsEmpty()? nullptr : PromiseFlatCString(aEntryName).get());
}

NS_IMETHODIMP
nsJAR::Extract(const nsACString &aEntryName, nsIFile* outFile)
{
  // nsZipArchive and zlib are not thread safe
  // we need to use a lock to prevent bug #51267
  MutexAutoLock lock(mLock);

  nsZipItem *item = mZip->GetItem(PromiseFlatCString(aEntryName).get());
  NS_ENSURE_TRUE(item, NS_ERROR_FILE_TARGET_DOES_NOT_EXIST);

  // Remove existing file or directory so we set permissions correctly.
  // If it's a directory that already exists and contains files, throw
  // an exception and return.

  nsresult rv = outFile->Remove(false);
  if (rv == NS_ERROR_FILE_DIR_NOT_EMPTY ||
      rv == NS_ERROR_FAILURE)
    return rv;

  if (item->IsDirectory())
  {
    rv = outFile->Create(nsIFile::DIRECTORY_TYPE, item->Mode());
    //XXX Do this in nsZipArchive?  It would be nice to keep extraction
    //XXX code completely there, but that would require a way to get a
    //XXX PRDir from outFile.
  }
  else
  {
    PRFileDesc* fd;
    rv = outFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, item->Mode(), &fd);
    if (NS_FAILED(rv)) return rv;

    // ExtractFile also closes the fd handle and resolves the symlink if needed
    rv = mZip->ExtractFile(item, outFile, fd);
  }
  if (NS_FAILED(rv)) return rv;

  // nsIFile needs milliseconds, while prtime is in microseconds.
  // non-fatal if this fails, ignore errors
  outFile->SetLastModifiedTime(item->LastModTime() / PR_USEC_PER_MSEC);

  return NS_OK;
}

NS_IMETHODIMP
nsJAR::GetEntry(const nsACString &aEntryName, nsIZipEntry* *result)
{
  nsZipItem* zipItem = mZip->GetItem(PromiseFlatCString(aEntryName).get());
  NS_ENSURE_TRUE(zipItem, NS_ERROR_FILE_TARGET_DOES_NOT_EXIST);

  nsJARItem* jarItem = new nsJARItem(zipItem);

  NS_ADDREF(*result = jarItem);
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::HasEntry(const nsACString &aEntryName, bool *result)
{
  *result = mZip->GetItem(PromiseFlatCString(aEntryName).get()) != nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::FindEntries(const nsACString &aPattern, nsIUTF8StringEnumerator **result)
{
  NS_ENSURE_ARG_POINTER(result);

  nsZipFind *find;
  nsresult rv = mZip->FindInit(aPattern.IsEmpty()? nullptr : PromiseFlatCString(aPattern).get(), &find);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIUTF8StringEnumerator *zipEnum = new nsJAREnumerator(find);

  NS_ADDREF(*result = zipEnum);
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::GetInputStream(const nsACString &aFilename, nsIInputStream** result)
{
  return GetInputStreamWithSpec(EmptyCString(), aFilename, result);
}

NS_IMETHODIMP
nsJAR::GetInputStreamWithSpec(const nsACString& aJarDirSpec,
                          const nsACString &aEntryName, nsIInputStream** result)
{
  NS_ENSURE_ARG_POINTER(result);

  // Watch out for the jar:foo.zip!/ (aDir is empty) top-level special case!
  nsZipItem *item = nullptr;
  const nsCString& entry = PromiseFlatCString(aEntryName);
  if (*entry.get()) {
    // First check if item exists in jar
    item = mZip->GetItem(entry.get());
    if (!item) return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
  }
  nsJARInputStream* jis = new nsJARInputStream();
  // addref now so we can call InitFile/InitDirectory()
  NS_ADDREF(*result = jis);

  nsresult rv = NS_OK;
  if (!item || item->IsDirectory()) {
    rv = jis->InitDirectory(this, aJarDirSpec, entry.get());
  } else {
    rv = jis->InitFile(this, item);
  }
  if (NS_FAILED(rv)) {
    NS_RELEASE(*result);
  }
  return rv;
}

nsresult
nsJAR::GetJarPath(nsACString& aResult)
{
  NS_ENSURE_ARG_POINTER(mZipFile);

  return mZipFile->GetPersistentDescriptor(aResult);
}

nsresult
nsJAR::GetNSPRFileDesc(PRFileDesc** aNSPRFileDesc)
{
  if (!aNSPRFileDesc) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  *aNSPRFileDesc = nullptr;

  if (!mZip) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsZipHandle> handle = mZip->GetFD();
  if (!handle) {
    return NS_ERROR_FAILURE;
  }

  return handle->GetNSPRFileDesc(aNSPRFileDesc);
}

//----------------------------------------------
// nsJAR private implementation
//----------------------------------------------
nsresult
nsJAR::LoadEntry(const nsACString& aFilename, nsCString& aBuf)
{
  //-- Get a stream for reading the file
  nsresult rv;
  nsCOMPtr<nsIInputStream> manifestStream;
  rv = GetInputStream(aFilename, getter_AddRefs(manifestStream));
  if (NS_FAILED(rv)) return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;

  //-- Read the manifest file into memory
  char* buf;
  uint64_t len64;
  rv = manifestStream->Available(&len64);
  if (NS_FAILED(rv)) return rv;
  if (len64 >= UINT32_MAX) { // bug 164695
    nsZipArchive::sFileCorruptedReason = "nsJAR: invalid manifest size";
    return NS_ERROR_FILE_CORRUPTED;
  }
  uint32_t len = (uint32_t)len64;
  buf = (char*)malloc(len+1);
  if (!buf) return NS_ERROR_OUT_OF_MEMORY;
  uint32_t bytesRead;
  rv = manifestStream->Read(buf, len, &bytesRead);
  if (bytesRead != len) {
    nsZipArchive::sFileCorruptedReason = "nsJAR: manifest too small";
    rv = NS_ERROR_FILE_CORRUPTED;
  }
  if (NS_FAILED(rv)) {
    free(buf);
    return rv;
  }
  buf[len] = '\0'; //Null-terminate the buffer
  aBuf.Adopt(buf, len);
  return NS_OK;
}


int32_t
nsJAR::ReadLine(const char** src)
{
  if (!*src) {
    return 0;
  }

  //--Moves pointer to beginning of next line and returns line length
  //  not including CR/LF.
  int32_t length;
  char* eol = PL_strpbrk(*src, "\r\n");

  if (eol == nullptr) // Probably reached end of file before newline
  {
    length = strlen(*src);
    if (length == 0) // immediate end-of-file
      *src = nullptr;
    else             // some data left on this line
      *src += length;
  }
  else
  {
    length = eol - *src;
    if (eol[0] == '\r' && eol[1] == '\n')      // CR LF, so skip 2
      *src = eol+2;
    else                                       // Either CR or LF, so skip 1
      *src = eol+1;
  }
  return length;
}

NS_IMPL_ISUPPORTS(nsJAREnumerator, nsIUTF8StringEnumerator)

//----------------------------------------------
// nsJAREnumerator::HasMore
//----------------------------------------------
NS_IMETHODIMP
nsJAREnumerator::HasMore(bool* aResult)
{
    // try to get the next element
    if (!mName) {
        NS_ASSERTION(mFind, "nsJAREnumerator: Missing zipFind.");
        nsresult rv = mFind->FindNext( &mName, &mNameLen );
        if (rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
            *aResult = false;                    // No more matches available
            return NS_OK;
        }
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);    // no error translation
    }

    *aResult = true;
    return NS_OK;
}

//----------------------------------------------
// nsJAREnumerator::GetNext
//----------------------------------------------
NS_IMETHODIMP
nsJAREnumerator::GetNext(nsACString& aResult)
{
    // check if the current item is "stale"
    if (!mName) {
        bool     bMore;
        nsresult rv = HasMore(&bMore);
        if (NS_FAILED(rv) || !bMore)
            return NS_ERROR_FAILURE; // no error translation
    }
    aResult.Assign(mName, mNameLen);
    mName = 0; // we just gave this one away
    return NS_OK;
}


NS_IMPL_ISUPPORTS(nsJARItem, nsIZipEntry)

nsJARItem::nsJARItem(nsZipItem* aZipItem)
    : mSize(aZipItem->Size()),
      mRealsize(aZipItem->RealSize()),
      mCrc32(aZipItem->CRC32()),
      mLastModTime(aZipItem->LastModTime()),
      mCompression(aZipItem->Compression()),
      mPermissions(aZipItem->Mode()),
      mIsDirectory(aZipItem->IsDirectory()),
      mIsSynthetic(aZipItem->isSynthetic)
{
}

//------------------------------------------
// nsJARItem::GetCompression
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetCompression(uint16_t *aCompression)
{
    NS_ENSURE_ARG_POINTER(aCompression);

    *aCompression = mCompression;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetSize
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetSize(uint32_t *aSize)
{
    NS_ENSURE_ARG_POINTER(aSize);

    *aSize = mSize;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetRealSize
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetRealSize(uint32_t *aRealsize)
{
    NS_ENSURE_ARG_POINTER(aRealsize);

    *aRealsize = mRealsize;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetCrc32
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetCRC32(uint32_t *aCrc32)
{
    NS_ENSURE_ARG_POINTER(aCrc32);

    *aCrc32 = mCrc32;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetIsDirectory
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetIsDirectory(bool *aIsDirectory)
{
    NS_ENSURE_ARG_POINTER(aIsDirectory);

    *aIsDirectory = mIsDirectory;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetIsSynthetic
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetIsSynthetic(bool *aIsSynthetic)
{
    NS_ENSURE_ARG_POINTER(aIsSynthetic);

    *aIsSynthetic = mIsSynthetic;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetLastModifiedTime
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetLastModifiedTime(PRTime* aLastModTime)
{
    NS_ENSURE_ARG_POINTER(aLastModTime);

    *aLastModTime = mLastModTime;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetPermissions
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetPermissions(uint32_t* aPermissions)
{
    NS_ENSURE_ARG_POINTER(aPermissions);

    *aPermissions = mPermissions;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIZipReaderCache

NS_IMPL_ISUPPORTS(nsZipReaderCache, nsIZipReaderCache, nsIObserver, nsISupportsWeakReference)

nsZipReaderCache::nsZipReaderCache()
  : mLock("nsZipReaderCache.mLock")
  , mCacheSize(0)
  , mZips()
#ifdef ZIP_CACHE_HIT_RATE
    ,
    mZipCacheLookups(0),
    mZipCacheHits(0),
    mZipCacheFlushes(0),
    mZipSyncMisses(0)
#endif
{
}

NS_IMETHODIMP
nsZipReaderCache::Init(uint32_t cacheSize)
{
  mCacheSize = cacheSize;

// Register as a memory pressure observer
  nsCOMPtr<nsIObserverService> os =
           do_GetService("@mozilla.org/observer-service;1");
  if (os)
  {
    os->AddObserver(this, "memory-pressure", true);
    os->AddObserver(this, "chrome-flush-caches", true);
    os->AddObserver(this, "flush-cache-entry", true);
  }
// ignore failure of the observer registration.

  return NS_OK;
}

nsZipReaderCache::~nsZipReaderCache()
{
  for (auto iter = mZips.Iter(); !iter.Done(); iter.Next()) {
    iter.UserData()->SetZipReaderCache(nullptr);
  }

#ifdef ZIP_CACHE_HIT_RATE
  printf("nsZipReaderCache size=%d hits=%d lookups=%d rate=%f%% flushes=%d missed %d\n",
         mCacheSize, mZipCacheHits, mZipCacheLookups,
         (float)mZipCacheHits / mZipCacheLookups,
         mZipCacheFlushes, mZipSyncMisses);
#endif
}

NS_IMETHODIMP
nsZipReaderCache::IsCached(nsIFile* zipFile, bool* aResult)
{
  NS_ENSURE_ARG_POINTER(zipFile);
  nsresult rv;
  MutexAutoLock lock(mLock);

  nsAutoCString uri;
  rv = zipFile->GetPersistentDescriptor(uri);
  if (NS_FAILED(rv))
    return rv;

  uri.InsertLiteral("file:", 0);

  *aResult = mZips.Contains(uri);
  return NS_OK;
}

nsresult
nsZipReaderCache::GetZip(nsIFile* zipFile, nsIZipReader* *result,
                         bool failOnMiss)
{
  NS_ENSURE_ARG_POINTER(zipFile);
  nsresult rv;
  MutexAutoLock lock(mLock);

#ifdef ZIP_CACHE_HIT_RATE
  mZipCacheLookups++;
#endif

  nsAutoCString uri;
  rv = zipFile->GetPersistentDescriptor(uri);
  if (NS_FAILED(rv)) return rv;

  uri.InsertLiteral("file:", 0);

  RefPtr<nsJAR> zip;
  mZips.Get(uri, getter_AddRefs(zip));
  if (zip) {
#ifdef ZIP_CACHE_HIT_RATE
    mZipCacheHits++;
#endif
    zip->ClearReleaseTime();
  } else {
    if (failOnMiss) {
      return NS_ERROR_CACHE_KEY_NOT_FOUND;
    }

    zip = new nsJAR();
    zip->SetZipReaderCache(this);
    rv = zip->Open(zipFile);
    if (NS_FAILED(rv)) {
      return rv;
    }

    MOZ_ASSERT(!mZips.Contains(uri));
    mZips.Put(uri, zip);
  }
  zip.forget(result);
  return rv;
}

NS_IMETHODIMP
nsZipReaderCache::GetZipIfCached(nsIFile* zipFile, nsIZipReader* *result)
{
  return GetZip(zipFile, result, true);
}

NS_IMETHODIMP
nsZipReaderCache::GetZip(nsIFile* zipFile, nsIZipReader* *result)
{
  return GetZip(zipFile, result, false);
}

NS_IMETHODIMP
nsZipReaderCache::GetInnerZip(nsIFile* zipFile, const nsACString &entry,
                              nsIZipReader* *result)
{
  NS_ENSURE_ARG_POINTER(zipFile);

  nsCOMPtr<nsIZipReader> outerZipReader;
  nsresult rv = GetZip(zipFile, getter_AddRefs(outerZipReader));
  NS_ENSURE_SUCCESS(rv, rv);

  MutexAutoLock lock(mLock);

#ifdef ZIP_CACHE_HIT_RATE
  mZipCacheLookups++;
#endif

  nsAutoCString uri;
  rv = zipFile->GetPersistentDescriptor(uri);
  if (NS_FAILED(rv)) return rv;

  uri.InsertLiteral("jar:", 0);
  uri.AppendLiteral("!/");
  uri.Append(entry);

  RefPtr<nsJAR> zip;
  mZips.Get(uri, getter_AddRefs(zip));
  if (zip) {
#ifdef ZIP_CACHE_HIT_RATE
    mZipCacheHits++;
#endif
    zip->ClearReleaseTime();
  } else {
    zip = new nsJAR();
    zip->SetZipReaderCache(this);

    rv = zip->OpenInner(outerZipReader, entry);
    if (NS_FAILED(rv)) {
      return rv;
    }

    MOZ_ASSERT(!mZips.Contains(uri));
    mZips.Put(uri, zip);
  }
  zip.forget(result);
  return rv;
}

NS_IMETHODIMP
nsZipReaderCache::GetFd(nsIFile* zipFile, PRFileDesc** aRetVal)
{
#if defined(XP_WIN)
  MOZ_CRASH("Not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  if (!zipFile) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  nsAutoCString uri;
  rv = zipFile->GetPersistentDescriptor(uri);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uri.InsertLiteral("file:", 0);

  MutexAutoLock lock(mLock);
  RefPtr<nsJAR> zip;
  mZips.Get(uri, getter_AddRefs(zip));
  if (!zip) {
    return NS_ERROR_FAILURE;
  }

  zip->ClearReleaseTime();
  rv = zip->GetNSPRFileDesc(aRetVal);
  // Do this to avoid possible deadlock on mLock with ReleaseZip().
  MutexAutoUnlock unlock(mLock);
  RefPtr<nsJAR> zipTemp = zip.forget();
  return rv;
#endif /* XP_WIN */
}

nsresult
nsZipReaderCache::ReleaseZip(nsJAR* zip)
{
  nsresult rv;
  MutexAutoLock lock(mLock);

  // It is possible that two thread compete for this zip. The dangerous
  // case is where one thread Releases the zip and discovers that the ref
  // count has gone to one. Before it can call this ReleaseZip method
  // another thread calls our GetZip method. The ref count goes to two. That
  // second thread then Releases the zip and the ref count goes to one. It
  // then tries to enter this ReleaseZip method and blocks while the first
  // thread is still here. The first thread continues and remove the zip from
  // the cache and calls its Release method sending the ref count to 0 and
  // deleting the zip. However, the second thread is still blocked at the
  // start of ReleaseZip, but the 'zip' param now hold a reference to a
  // deleted zip!
  //
  // So, we are going to try safeguarding here by searching our hashtable while
  // locked here for the zip. We return fast if it is not found.

  bool found = false;
  for (auto iter = mZips.Iter(); !iter.Done(); iter.Next()) {
    if (zip == iter.UserData()) {
      found = true;
      break;
    }
  }

  if (!found) {
#ifdef ZIP_CACHE_HIT_RATE
    mZipSyncMisses++;
#endif
    return NS_OK;
  }

  zip->SetReleaseTime();

  if (mZips.Count() <= mCacheSize)
    return NS_OK;

  // Find the oldest zip.
  nsJAR* oldest = nullptr;
  for (auto iter = mZips.Iter(); !iter.Done(); iter.Next()) {
    nsJAR* current = iter.UserData();
    PRIntervalTime currentReleaseTime = current->GetReleaseTime();
    if (currentReleaseTime != PR_INTERVAL_NO_TIMEOUT) {
      if (oldest == nullptr ||
          currentReleaseTime < oldest->GetReleaseTime()) {
        oldest = current;
      }
    }
  }

  // Because of the craziness above it is possible that there is no zip that
  // needs removing.
  if (!oldest)
    return NS_OK;

#ifdef ZIP_CACHE_HIT_RATE
    mZipCacheFlushes++;
#endif

  // remove from hashtable
  nsAutoCString uri;
  rv = oldest->GetJarPath(uri);
  if (NS_FAILED(rv))
    return rv;

  if (oldest->mOuterZipEntry.IsEmpty()) {
    uri.InsertLiteral("file:", 0);
  } else {
    uri.InsertLiteral("jar:", 0);
    uri.AppendLiteral("!/");
    uri.Append(oldest->mOuterZipEntry);
  }

  // Retrieving and removing the JAR must be done without an extra AddRef
  // and Release, or we'll trigger nsJAR::Release's magic refcount 1 case
  // an extra time and trigger a deadlock.
  RefPtr<nsJAR> removed;
  mZips.Remove(uri, getter_AddRefs(removed));
  NS_ASSERTION(removed, "botched");
  NS_ASSERTION(oldest == removed, "removed wrong entry");

  if (removed)
    removed->SetZipReaderCache(nullptr);

  return NS_OK;
}

NS_IMETHODIMP
nsZipReaderCache::Observe(nsISupports *aSubject,
                          const char *aTopic,
                          const char16_t *aSomeData)
{
  if (strcmp(aTopic, "memory-pressure") == 0) {
    MutexAutoLock lock(mLock);
    for (auto iter = mZips.Iter(); !iter.Done(); iter.Next()) {
      RefPtr<nsJAR>& current = iter.Data();
      if (current->GetReleaseTime() != PR_INTERVAL_NO_TIMEOUT) {
        current->SetZipReaderCache(nullptr);
        iter.Remove();
      }
    }
  }
  else if (strcmp(aTopic, "chrome-flush-caches") == 0) {
    MutexAutoLock lock(mLock);
    for (auto iter = mZips.Iter(); !iter.Done(); iter.Next()) {
      iter.UserData()->SetZipReaderCache(nullptr);
    }
    mZips.Clear();
  }
  else if (strcmp(aTopic, "flush-cache-entry") == 0) {
    nsCOMPtr<nsIFile> file;
    if (aSubject) {
      file = do_QueryInterface(aSubject);
    } else if (aSomeData) {
      nsDependentString fileName(aSomeData);
      Unused << NS_NewLocalFile(fileName, false, getter_AddRefs(file));
    }

    if (!file)
      return NS_OK;

    nsAutoCString uri;
    if (NS_FAILED(file->GetPersistentDescriptor(uri)))
      return NS_OK;

    uri.InsertLiteral("file:", 0);

    MutexAutoLock lock(mLock);

    RefPtr<nsJAR> zip;
    mZips.Get(uri, getter_AddRefs(zip));
    if (!zip)
      return NS_OK;

#ifdef ZIP_CACHE_HIT_RATE
    mZipCacheFlushes++;
#endif

    zip->SetZipReaderCache(nullptr);

    mZips.Remove(uri);
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
