/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileContextEvictor.h"
#include "CacheFileIOManager.h"
#include "CacheIndex.h"
#include "CacheIndexIterator.h"
#include "CacheFileUtils.h"
#include "nsIFile.h"
#include "LoadContextInfo.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "mozilla/Base64.h"


namespace mozilla {
namespace net {

const char kContextEvictionPrefix[] = "ce_";
const uint32_t kContextEvictionPrefixLength =
  sizeof(kContextEvictionPrefix) - 1;

bool CacheFileContextEvictor::sDiskAlreadySearched = false;

CacheFileContextEvictor::CacheFileContextEvictor()
  : mEvicting(false)
  , mIndexIsUpToDate(false)
{
  LOG(("CacheFileContextEvictor::CacheFileContextEvictor() [this=%p]", this));
}

CacheFileContextEvictor::~CacheFileContextEvictor()
{
  LOG(("CacheFileContextEvictor::~CacheFileContextEvictor() [this=%p]", this));
}

nsresult
CacheFileContextEvictor::Init(nsIFile *aCacheDirectory)
{
  LOG(("CacheFileContextEvictor::Init()"));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndex::IsUpToDate(&mIndexIsUpToDate);

  mCacheDirectory = aCacheDirectory;

  rv = aCacheDirectory->Clone(getter_AddRefs(mEntriesDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mEntriesDir->AppendNative(NS_LITERAL_CSTRING(kEntriesDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!sDiskAlreadySearched) {
    LoadEvictInfoFromDisk();
    if ((mEntries.Length() != 0) && mIndexIsUpToDate) {
      CreateIterators();
      StartEvicting();
    }
  }

  return NS_OK;
}

uint32_t
CacheFileContextEvictor::ContextsCount()
{
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  return mEntries.Length();
}

nsresult
CacheFileContextEvictor::AddContext(nsILoadContextInfo *aLoadContextInfo)
{
  LOG(("CacheFileContextEvictor::AddContext() [this=%p, loadContextInfo=%p]",
       this, aLoadContextInfo));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheFileContextEvictorEntry *entry = nullptr;
  for (uint32_t i = 0; i < mEntries.Length(); ++i) {
    if (mEntries[i]->mInfo->Equals(aLoadContextInfo)) {
      entry = mEntries[i];
      break;
    }
  }

  if (!entry) {
    entry = new CacheFileContextEvictorEntry();
    entry->mInfo = aLoadContextInfo;
    mEntries.AppendElement(entry);
  }

  entry->mTimeStamp = PR_Now() / PR_USEC_PER_MSEC;

  PersistEvictionInfoToDisk(aLoadContextInfo);

  if (mIndexIsUpToDate) {
    // Already existing context could be added again, in this case the iterator
    // would be recreated. Close the old iterator explicitely.
    if (entry->mIterator) {
      entry->mIterator->Close();
      entry->mIterator = nullptr;
    }

    rv = CacheIndex::GetIterator(aLoadContextInfo, false,
                                 getter_AddRefs(entry->mIterator));
    if (NS_FAILED(rv)) {
      // This could probably happen during shutdown. Remove the entry from
      // the array, but leave the info on the disk. No entry can be opened
      // during shutdown and we'll load the eviction info on next start.
      LOG(("CacheFileContextEvictor::AddContext() - Cannot get an iterator. "
           "[rv=0x%08x]", rv));
      mEntries.RemoveElement(entry);
      return rv;
    }

    StartEvicting();
  }

  return NS_OK;
}

nsresult
CacheFileContextEvictor::CacheIndexStateChanged()
{
  LOG(("CacheFileContextEvictor::CacheIndexStateChanged() [this=%p]", this));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  bool isUpToDate = false;
  CacheIndex::IsUpToDate(&isUpToDate);
  if (mEntries.Length() == 0) {
    // Just save the state and exit, since there is nothing to do
    mIndexIsUpToDate = isUpToDate;
    return NS_OK;
  }

  if (!isUpToDate && !mIndexIsUpToDate) {
    // Index is outdated and status has not changed, nothing to do.
    return NS_OK;
  }

  if (isUpToDate && mIndexIsUpToDate) {
    // Status has not changed, but make sure the eviction is running.
    if (mEvicting) {
      return NS_OK;
    }

    // We're not evicting, but we should be evicting?!
    LOG(("CacheFileContextEvictor::CacheIndexStateChanged() - Index is up to "
         "date, we have some context to evict but eviction is not running! "
         "Starting now."));
  }

  mIndexIsUpToDate = isUpToDate;

  if (mIndexIsUpToDate) {
    CreateIterators();
    StartEvicting();
  } else {
    CloseIterators();
  }

  return NS_OK;
}

nsresult
CacheFileContextEvictor::WasEvicted(const nsACString &aKey, nsIFile *aFile,
                                    bool *_retval)
{
  LOG(("CacheFileContextEvictor::WasEvicted() [key=%s]",
       PromiseFlatCString(aKey).get()));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(aKey);
  MOZ_ASSERT(info);
  if (!info) {
    LOG(("CacheFileContextEvictor::WasEvicted() - Cannot parse key!"));
    *_retval = false;
    return NS_OK;
  }

  CacheFileContextEvictorEntry *entry = nullptr;
  for (uint32_t i = 0; i < mEntries.Length(); ++i) {
    if (info->Equals(mEntries[i]->mInfo)) {
      entry = mEntries[i];
      break;
    }
  }

  if (!entry) {
    LOG(("CacheFileContextEvictor::WasEvicted() - Didn't find equal context, "
         "returning false."));
    *_retval = false;
    return NS_OK;
  }

  PRTime lastModifiedTime;
  rv = aFile->GetLastModifiedTime(&lastModifiedTime);
  if (NS_FAILED(rv)) {
    LOG(("CacheFileContextEvictor::WasEvicted() - Cannot get last modified time"
         ", returning false."));
    *_retval = false;
    return NS_OK;
  }

  *_retval = !(lastModifiedTime > entry->mTimeStamp);
  LOG(("CacheFileContextEvictor::WasEvicted() - returning %s. [mTimeStamp=%lld,"
       " lastModifiedTime=%lld]", *_retval ? "true" : "false",
       mEntries[0]->mTimeStamp, lastModifiedTime));

  return NS_OK;
}

nsresult
CacheFileContextEvictor::PersistEvictionInfoToDisk(
  nsILoadContextInfo *aLoadContextInfo)
{
  LOG(("CacheFileContextEvictor::PersistEvictionInfoToDisk() [this=%p, "
       "loadContextInfo=%p]", this, aLoadContextInfo));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsIFile> file;
  rv = GetContextFile(aLoadContextInfo, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef PR_LOGGING
  nsAutoCString path;
  file->GetNativePath(path);
#endif

  PRFileDesc *fd;
  rv = file->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600,
                              &fd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("CacheFileContextEvictor::PersistEvictionInfoToDisk() - Creating file "
         "failed! [path=%s, rv=0x%08x]", path.get(), rv));
    return rv;
  }

  PR_Close(fd);

  LOG(("CacheFileContextEvictor::PersistEvictionInfoToDisk() - Successfully "
       "created file. [path=%s]", path.get()));

  return NS_OK;
}

nsresult
CacheFileContextEvictor::RemoveEvictInfoFromDisk(
  nsILoadContextInfo *aLoadContextInfo)
{
  LOG(("CacheFileContextEvictor::RemoveEvictInfoFromDisk() [this=%p, "
       "loadContextInfo=%p]", this, aLoadContextInfo));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsIFile> file;
  rv = GetContextFile(aLoadContextInfo, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef PR_LOGGING
  nsAutoCString path;
  file->GetNativePath(path);
#endif

  rv = file->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("CacheFileContextEvictor::RemoveEvictionInfoFromDisk() - Removing file"
         " failed! [path=%s, rv=0x%08x]", path.get(), rv));
    return rv;
  }

  LOG(("CacheFileContextEvictor::RemoveEvictionInfoFromDisk() - Successfully "
       "removed file. [path=%s]", path.get()));

  return NS_OK;
}

nsresult
CacheFileContextEvictor::LoadEvictInfoFromDisk()
{
  LOG(("CacheFileContextEvictor::LoadEvictInfoFromDisk() [this=%p]", this));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  sDiskAlreadySearched = true;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = mCacheDirectory->GetDirectoryEntries(getter_AddRefs(enumerator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIDirectoryEnumerator> dirEnum = do_QueryInterface(enumerator, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  while (true) {
    nsCOMPtr<nsIFile> file;
    rv = dirEnum->GetNextFile(getter_AddRefs(file));
    if (!file) {
      break;
    }

    bool isDir = false;
    file->IsDirectory(&isDir);
    if (isDir) {
      continue;
    }

    nsAutoCString leaf;
    rv = file->GetNativeLeafName(leaf);
    if (NS_FAILED(rv)) {
      LOG(("CacheFileContextEvictor::LoadEvictInfoFromDisk() - "
           "GetNativeLeafName() failed! Skipping file."));
      continue;
    }

    if (leaf.Length() < kContextEvictionPrefixLength) {
      continue;
    }

    if (!StringBeginsWith(leaf, NS_LITERAL_CSTRING(kContextEvictionPrefix))) {
      continue;
    }

    nsAutoCString encoded;
    encoded = Substring(leaf, kContextEvictionPrefixLength);
    encoded.ReplaceChar('-', '/');

    nsAutoCString decoded;
    rv = Base64Decode(encoded, decoded);
    if (NS_FAILED(rv)) {
      LOG(("CacheFileContextEvictor::LoadEvictInfoFromDisk() - Base64 decoding "
           "failed. Removing the file. [file=%s]", leaf.get()));
      file->Remove(false);
      continue;
    }

    nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(decoded);

    if (!info) {
      LOG(("CacheFileContextEvictor::LoadEvictInfoFromDisk() - Cannot parse "
           "context key, removing file. [contextKey=%s, file=%s]",
           decoded.get(), leaf.get()));
      file->Remove(false);
      continue;
    }

    PRTime lastModifiedTime;
    rv = file->GetLastModifiedTime(&lastModifiedTime);
    if (NS_FAILED(rv)) {
      continue;
    }

    CacheFileContextEvictorEntry *entry = new CacheFileContextEvictorEntry();
    entry->mInfo = info;
    entry->mTimeStamp = lastModifiedTime;
    mEntries.AppendElement(entry);
  }

  return NS_OK;
}

nsresult
CacheFileContextEvictor::GetContextFile(nsILoadContextInfo *aLoadContextInfo,
                                        nsIFile **_retval)
{
  nsresult rv;

  nsAutoCString leafName;
  leafName.Assign(NS_LITERAL_CSTRING(kContextEvictionPrefix));

  nsAutoCString keyPrefix;
  CacheFileUtils::AppendKeyPrefix(aLoadContextInfo, keyPrefix);

  // TODO: This hack is needed because current CacheFileUtils::ParseKey() can
  // parse only the whole key and not just the key prefix generated by
  // CacheFileUtils::CreateKeyPrefix(). This should be removed once bug #968593
  // is fixed.
  keyPrefix.Append(":foo");

  nsAutoCString data64;
  rv = Base64Encode(keyPrefix, data64);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Replace '/' with '-' since '/' cannot be part of the filename.
  data64.ReplaceChar('/', '-');

  leafName.Append(data64);

  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->AppendNative(leafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  file.swap(*_retval);
  return NS_OK;
}

void
CacheFileContextEvictor::CreateIterators()
{
  LOG(("CacheFileContextEvictor::CreateIterators() [this=%p]", this));

  CloseIterators();

  nsresult rv;

  for (uint32_t i = 0; i < mEntries.Length(); ) {
    rv = CacheIndex::GetIterator(mEntries[i]->mInfo, false,
                                 getter_AddRefs(mEntries[i]->mIterator));
    if (NS_FAILED(rv)) {
      LOG(("CacheFileContextEvictor::CreateIterators() - Cannot get an iterator"
           ". [rv=0x%08x]", rv));
      mEntries.RemoveElementAt(i);
      continue;
    }

    ++i;
  }
}

void
CacheFileContextEvictor::CloseIterators()
{
  LOG(("CacheFileContextEvictor::CloseIterators() [this=%p]", this));

  for (uint32_t i = 0; i < mEntries.Length(); ++i) {
    if (mEntries[i]->mIterator) {
      mEntries[i]->mIterator->Close();
      mEntries[i]->mIterator = nullptr;
    }
  }
}

void
CacheFileContextEvictor::StartEvicting()
{
  LOG(("CacheFileContextEvictor::StartEvicting() [this=%p]", this));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  if (mEvicting) {
    LOG(("CacheFileContextEvictor::StartEvicting() - already evicintg."));
    return;
  }

  if (mEntries.Length() == 0) {
    LOG(("CacheFileContextEvictor::StartEvicting() - no context to evict."));
    return;
  }

  nsCOMPtr<nsIRunnable> ev;
  ev = NS_NewRunnableMethod(this, &CacheFileContextEvictor::EvictEntries);

  nsRefPtr<CacheIOThread> ioThread = CacheFileIOManager::IOThread();

  nsresult rv = ioThread->Dispatch(ev, CacheIOThread::EVICT);
  if (NS_FAILED(rv)) {
    LOG(("CacheFileContextEvictor::StartEvicting() - Cannot dispatch event to "
         "IO thread. [rv=0x%08x]", rv));
  }

  mEvicting = true;
}

nsresult
CacheFileContextEvictor::EvictEntries()
{
  LOG(("CacheFileContextEvictor::EvictEntries()"));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  mEvicting = false;

  if (!mIndexIsUpToDate) {
    LOG(("CacheFileContextEvictor::EvictEntries() - Stopping evicting due to "
         "outdated index."));
    return NS_OK;
  }

  while (true) {
    if (CacheIOThread::YieldAndRerun()) {
      LOG(("CacheFileContextEvictor::EvictEntries() - Breaking loop for higher "
           "level events."));
      mEvicting = true;
      return NS_OK;
    }

    if (mEntries.Length() == 0) {
      LOG(("CacheFileContextEvictor::EvictEntries() - Stopping evicting, there "
           "is no context to evict."));
      return NS_OK;
    }

    SHA1Sum::Hash hash;
    rv = mEntries[0]->mIterator->GetNextHash(&hash);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      LOG(("CacheFileContextEvictor::EvictEntries() - No more entries left in "
           "iterator. [iterator=%p, info=%p]", mEntries[0]->mIterator.get(),
           mEntries[0]->mInfo.get()));
      RemoveEvictInfoFromDisk(mEntries[0]->mInfo);
      mEntries.RemoveElementAt(0);
      continue;
    } else if (NS_FAILED(rv)) {
      LOG(("CacheFileContextEvictor::EvictEntries() - Iterator failed to "
           "provide next hash (shutdown?), keeping eviction info on disk."
           " [iterator=%p, info=%p]", mEntries[0]->mIterator.get(),
           mEntries[0]->mInfo.get()));
      mEntries.RemoveElementAt(0);
      continue;
    }

    LOG(("CacheFileContextEvictor::EvictEntries() - Processing hash. "
         "[hash=%08x%08x%08x%08x%08x, iterator=%p, info=%p]", LOGSHA1(&hash),
         mEntries[0]->mIterator.get(), mEntries[0]->mInfo.get()));

    nsRefPtr<CacheFileHandle> handle;
    CacheFileIOManager::gInstance->mHandles.GetHandle(&hash, false,
                                                      getter_AddRefs(handle));
    if (handle) {
      // We doom any active handle in CacheFileIOManager::EvictByContext(), so
      // this must be a new one. Skip it.
      LOG(("CacheFileContextEvictor::EvictEntries() - Skipping entry since we "
           "found an active handle. [handle=%p]", handle.get()));
      continue;
    }

    nsAutoCString leafName;
    CacheFileIOManager::HashToStr(&hash, leafName);

    PRTime lastModifiedTime;
    nsCOMPtr<nsIFile> file;
    rv = mEntriesDir->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = file->AppendNative(leafName);
    }
    if (NS_SUCCEEDED(rv)) {
      rv = file->GetLastModifiedTime(&lastModifiedTime);
    }
    if (NS_FAILED(rv)) {
      LOG(("CacheFileContextEvictor::EvictEntries() - Cannot get last modified "
           "time, skipping entry."));
      continue;
    }

    if (lastModifiedTime > mEntries[0]->mTimeStamp) {
      LOG(("CacheFileContextEvictor::EvictEntries() - Skipping newer entry. "
           "[mTimeStamp=%lld, lastModifiedTime=%lld]", mEntries[0]->mTimeStamp,
           lastModifiedTime));
      continue;
    }

    LOG(("CacheFileContextEvictor::EvictEntries - Removing entry."));
    file->Remove(false);
    CacheIndex::RemoveEntry(&hash);
  }

  NS_NOTREACHED("We should never get here");
  return NS_OK;
}

} // net
} // mozilla
