/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFileContextEvictor.h"
#include "CacheFileIOManager.h"
#include "CacheFileMetadata.h"
#include "CacheIndex.h"
#include "CacheIndexIterator.h"
#include "CacheFileUtils.h"
#include "CacheObserver.h"
#include "nsIFile.h"
#include "LoadContextInfo.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsIDirectoryEnumerator.h"
#include "mozilla/Base64.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla::net {

#define CONTEXT_EVICTION_PREFIX "ce_"
const uint32_t kContextEvictionPrefixLength =
    sizeof(CONTEXT_EVICTION_PREFIX) - 1;

bool CacheFileContextEvictor::sDiskAlreadySearched = false;

CacheFileContextEvictor::CacheFileContextEvictor() {
  LOG(("CacheFileContextEvictor::CacheFileContextEvictor() [this=%p]", this));
}

CacheFileContextEvictor::~CacheFileContextEvictor() {
  LOG(("CacheFileContextEvictor::~CacheFileContextEvictor() [this=%p]", this));
}

nsresult CacheFileContextEvictor::Init(nsIFile* aCacheDirectory) {
  LOG(("CacheFileContextEvictor::Init()"));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheIndex::IsUpToDate(&mIndexIsUpToDate);

  mCacheDirectory = aCacheDirectory;

  rv = aCacheDirectory->Clone(getter_AddRefs(mEntriesDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mEntriesDir->AppendNative(nsLiteralCString(ENTRIES_DIR));
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

void CacheFileContextEvictor::Shutdown() {
  LOG(("CacheFileContextEvictor::Shutdown()"));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CloseIterators();
}

uint32_t CacheFileContextEvictor::ContextsCount() {
  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  return mEntries.Length();
}

nsresult CacheFileContextEvictor::AddContext(
    nsILoadContextInfo* aLoadContextInfo, bool aPinned,
    const nsAString& aOrigin) {
  LOG(
      ("CacheFileContextEvictor::AddContext() [this=%p, loadContextInfo=%p, "
       "pinned=%d]",
       this, aLoadContextInfo, aPinned));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  CacheFileContextEvictorEntry* entry = nullptr;
  if (aLoadContextInfo) {
    for (uint32_t i = 0; i < mEntries.Length(); ++i) {
      if (mEntries[i]->mInfo && mEntries[i]->mInfo->Equals(aLoadContextInfo) &&
          mEntries[i]->mPinned == aPinned &&
          mEntries[i]->mOrigin.Equals(aOrigin)) {
        entry = mEntries[i].get();
        break;
      }
    }
  } else {
    // Not providing load context info means we want to delete everything,
    // so let's not bother with any currently running context cleanups
    // for the same pinning state.
    for (uint32_t i = mEntries.Length(); i > 0;) {
      --i;
      if (mEntries[i]->mInfo && mEntries[i]->mPinned == aPinned) {
        RemoveEvictInfoFromDisk(mEntries[i]->mInfo, mEntries[i]->mPinned,
                                mEntries[i]->mOrigin);
        mEntries.RemoveElementAt(i);
      }
    }
  }

  if (!entry) {
    entry = new CacheFileContextEvictorEntry();
    entry->mInfo = aLoadContextInfo;
    entry->mPinned = aPinned;
    entry->mOrigin = aOrigin;
    mEntries.AppendElement(WrapUnique(entry));
  }

  entry->mTimeStamp = PR_Now() / PR_USEC_PER_MSEC;

  PersistEvictionInfoToDisk(aLoadContextInfo, aPinned, aOrigin);

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
      LOG(
          ("CacheFileContextEvictor::AddContext() - Cannot get an iterator. "
           "[rv=0x%08" PRIx32 "]",
           static_cast<uint32_t>(rv)));
      mEntries.RemoveElement(entry);
      return rv;
    }

    StartEvicting();
  }

  return NS_OK;
}

void CacheFileContextEvictor::CacheIndexStateChanged() {
  LOG(("CacheFileContextEvictor::CacheIndexStateChanged() [this=%p]", this));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  bool isUpToDate = false;
  CacheIndex::IsUpToDate(&isUpToDate);
  if (mEntries.Length() == 0) {
    // Just save the state and exit, since there is nothing to do
    mIndexIsUpToDate = isUpToDate;
    return;
  }

  if (!isUpToDate && !mIndexIsUpToDate) {
    // Index is outdated and status has not changed, nothing to do.
    return;
  }

  if (isUpToDate && mIndexIsUpToDate) {
    // Status has not changed, but make sure the eviction is running.
    if (mEvicting) {
      return;
    }

    // We're not evicting, but we should be evicting?!
    LOG(
        ("CacheFileContextEvictor::CacheIndexStateChanged() - Index is up to "
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
}

void CacheFileContextEvictor::WasEvicted(const nsACString& aKey, nsIFile* aFile,
                                         bool* aEvictedAsPinned,
                                         bool* aEvictedAsNonPinned) {
  LOG(("CacheFileContextEvictor::WasEvicted() [key=%s]",
       PromiseFlatCString(aKey).get()));

  *aEvictedAsPinned = false;
  *aEvictedAsNonPinned = false;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(aKey);
  MOZ_ASSERT(info);
  if (!info) {
    LOG(("CacheFileContextEvictor::WasEvicted() - Cannot parse key!"));
    return;
  }

  for (uint32_t i = 0; i < mEntries.Length(); ++i) {
    const auto& entry = mEntries[i];

    if (entry->mInfo && !info->Equals(entry->mInfo)) {
      continue;
    }

    PRTime lastModifiedTime;
    if (NS_FAILED(aFile->GetLastModifiedTime(&lastModifiedTime))) {
      LOG(
          ("CacheFileContextEvictor::WasEvicted() - Cannot get last modified "
           "time, returning."));
      return;
    }

    if (lastModifiedTime > entry->mTimeStamp) {
      // File has been modified since context eviction.
      continue;
    }

    LOG(
        ("CacheFileContextEvictor::WasEvicted() - evicted [pinning=%d, "
         "mTimeStamp=%" PRId64 ", lastModifiedTime=%" PRId64 "]",
         entry->mPinned, entry->mTimeStamp, lastModifiedTime));

    if (entry->mPinned) {
      *aEvictedAsPinned = true;
    } else {
      *aEvictedAsNonPinned = true;
    }
  }
}

nsresult CacheFileContextEvictor::PersistEvictionInfoToDisk(
    nsILoadContextInfo* aLoadContextInfo, bool aPinned,
    const nsAString& aOrigin) {
  LOG(
      ("CacheFileContextEvictor::PersistEvictionInfoToDisk() [this=%p, "
       "loadContextInfo=%p]",
       this, aLoadContextInfo));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsIFile> file;
  rv = GetContextFile(aLoadContextInfo, aPinned, aOrigin, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString path = file->HumanReadablePath();

  PRFileDesc* fd;
  rv =
      file->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(
        ("CacheFileContextEvictor::PersistEvictionInfoToDisk() - Creating file "
         "failed! [path=%s, rv=0x%08" PRIx32 "]",
         path.get(), static_cast<uint32_t>(rv)));
    return rv;
  }

  PR_Close(fd);

  LOG(
      ("CacheFileContextEvictor::PersistEvictionInfoToDisk() - Successfully "
       "created file. [path=%s]",
       path.get()));

  return NS_OK;
}

nsresult CacheFileContextEvictor::RemoveEvictInfoFromDisk(
    nsILoadContextInfo* aLoadContextInfo, bool aPinned,
    const nsAString& aOrigin) {
  LOG(
      ("CacheFileContextEvictor::RemoveEvictInfoFromDisk() [this=%p, "
       "loadContextInfo=%p]",
       this, aLoadContextInfo));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  nsCOMPtr<nsIFile> file;
  rv = GetContextFile(aLoadContextInfo, aPinned, aOrigin, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString path = file->HumanReadablePath();

  rv = file->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(
        ("CacheFileContextEvictor::RemoveEvictionInfoFromDisk() - Removing file"
         " failed! [path=%s, rv=0x%08" PRIx32 "]",
         path.get(), static_cast<uint32_t>(rv)));
    return rv;
  }

  LOG(
      ("CacheFileContextEvictor::RemoveEvictionInfoFromDisk() - Successfully "
       "removed file. [path=%s]",
       path.get()));

  return NS_OK;
}

nsresult CacheFileContextEvictor::LoadEvictInfoFromDisk() {
  LOG(("CacheFileContextEvictor::LoadEvictInfoFromDisk() [this=%p]", this));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  sDiskAlreadySearched = true;

  nsCOMPtr<nsIDirectoryEnumerator> dirEnum;
  rv = mCacheDirectory->GetDirectoryEntries(getter_AddRefs(dirEnum));
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
      LOG(
          ("CacheFileContextEvictor::LoadEvictInfoFromDisk() - "
           "GetNativeLeafName() failed! Skipping file."));
      continue;
    }

    if (leaf.Length() < kContextEvictionPrefixLength) {
      continue;
    }

    if (!StringBeginsWith(leaf, nsLiteralCString(CONTEXT_EVICTION_PREFIX))) {
      continue;
    }

    nsAutoCString encoded;
    encoded = Substring(leaf, kContextEvictionPrefixLength);
    encoded.ReplaceChar('-', '/');

    nsAutoCString decoded;
    rv = Base64Decode(encoded, decoded);
    if (NS_FAILED(rv)) {
      LOG(
          ("CacheFileContextEvictor::LoadEvictInfoFromDisk() - Base64 decoding "
           "failed. Removing the file. [file=%s]",
           leaf.get()));
      file->Remove(false);
      continue;
    }

    bool pinned = decoded[0] == '\t';
    if (pinned) {
      decoded = Substring(decoded, 1);
    }

    // Let's see if we have an origin.
    nsAutoCString origin;
    if (decoded.Contains('\t')) {
      auto split = decoded.Split('\t');
      MOZ_ASSERT(decoded.CountChar('\t') == 1);

      auto splitIt = split.begin();
      origin = *splitIt;
      ++splitIt;
      decoded = *splitIt;
    }

    nsCOMPtr<nsILoadContextInfo> info;
    if (!"*"_ns.Equals(decoded)) {
      // "*" is indication of 'delete all', info left null will pass
      // to CacheFileContextEvictor::AddContext and clear all the cache data.
      info = CacheFileUtils::ParseKey(decoded);
      if (!info) {
        LOG(
            ("CacheFileContextEvictor::LoadEvictInfoFromDisk() - Cannot parse "
             "context key, removing file. [contextKey=%s, file=%s]",
             decoded.get(), leaf.get()));
        file->Remove(false);
        continue;
      }
    }

    PRTime lastModifiedTime;
    rv = file->GetLastModifiedTime(&lastModifiedTime);
    if (NS_FAILED(rv)) {
      continue;
    }

    CacheFileContextEvictorEntry* entry = new CacheFileContextEvictorEntry();
    entry->mInfo = info;
    entry->mPinned = pinned;
    CopyUTF8toUTF16(origin, entry->mOrigin);
    entry->mTimeStamp = lastModifiedTime;
    mEntries.AppendElement(entry);
  }

  return NS_OK;
}

nsresult CacheFileContextEvictor::GetContextFile(
    nsILoadContextInfo* aLoadContextInfo, bool aPinned,
    const nsAString& aOrigin, nsIFile** _retval) {
  nsresult rv;

  nsAutoCString keyPrefix;
  if (aPinned) {
    // Mark pinned context files with a tab char at the start.
    // Tab is chosen because it can never be used as a context key tag.
    keyPrefix.Append('\t');
  }
  if (aLoadContextInfo) {
    CacheFileUtils::AppendKeyPrefix(aLoadContextInfo, keyPrefix);
  } else {
    keyPrefix.Append('*');
  }
  if (!aOrigin.IsEmpty()) {
    keyPrefix.Append('\t');
    keyPrefix.Append(NS_ConvertUTF16toUTF8(aOrigin));
  }

  nsAutoCString leafName;
  leafName.AssignLiteral(CONTEXT_EVICTION_PREFIX);

  rv = Base64EncodeAppend(keyPrefix, leafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Replace '/' with '-' since '/' cannot be part of the filename.
  leafName.ReplaceChar('/', '-');

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

void CacheFileContextEvictor::CreateIterators() {
  LOG(("CacheFileContextEvictor::CreateIterators() [this=%p]", this));

  CloseIterators();

  nsresult rv;

  for (uint32_t i = 0; i < mEntries.Length();) {
    rv = CacheIndex::GetIterator(mEntries[i]->mInfo, false,
                                 getter_AddRefs(mEntries[i]->mIterator));
    if (NS_FAILED(rv)) {
      LOG(
          ("CacheFileContextEvictor::CreateIterators() - Cannot get an iterator"
           ". [rv=0x%08" PRIx32 "]",
           static_cast<uint32_t>(rv)));
      mEntries.RemoveElementAt(i);
      continue;
    }

    ++i;
  }
}

void CacheFileContextEvictor::CloseIterators() {
  LOG(("CacheFileContextEvictor::CloseIterators() [this=%p]", this));

  for (uint32_t i = 0; i < mEntries.Length(); ++i) {
    if (mEntries[i]->mIterator) {
      mEntries[i]->mIterator->Close();
      mEntries[i]->mIterator = nullptr;
    }
  }
}

void CacheFileContextEvictor::StartEvicting() {
  LOG(("CacheFileContextEvictor::StartEvicting() [this=%p]", this));

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  if (mEvicting) {
    LOG(("CacheFileContextEvictor::StartEvicting() - already evicting."));
    return;
  }

  if (mEntries.Length() == 0) {
    LOG(("CacheFileContextEvictor::StartEvicting() - no context to evict."));
    return;
  }

  nsCOMPtr<nsIRunnable> ev =
      NewRunnableMethod("net::CacheFileContextEvictor::EvictEntries", this,
                        &CacheFileContextEvictor::EvictEntries);

  RefPtr<CacheIOThread> ioThread = CacheFileIOManager::IOThread();

  nsresult rv = ioThread->Dispatch(ev, CacheIOThread::EVICT);
  if (NS_FAILED(rv)) {
    LOG(
        ("CacheFileContextEvictor::StartEvicting() - Cannot dispatch event to "
         "IO thread. [rv=0x%08" PRIx32 "]",
         static_cast<uint32_t>(rv)));
  }

  mEvicting = true;
}

void CacheFileContextEvictor::EvictEntries() {
  LOG(("CacheFileContextEvictor::EvictEntries()"));

  nsresult rv;

  MOZ_ASSERT(CacheFileIOManager::IsOnIOThread());

  mEvicting = false;

  if (!mIndexIsUpToDate) {
    LOG(
        ("CacheFileContextEvictor::EvictEntries() - Stopping evicting due to "
         "outdated index."));
    return;
  }

  while (true) {
    if (CacheObserver::ShuttingDown()) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Stopping evicting due to "
           "shutdown."));
      mEvicting =
          true;  // We don't want to start eviction again during shutdown
                 // process. Setting this flag to true ensures it.
      return;
    }

    if (CacheIOThread::YieldAndRerun()) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Breaking loop for higher "
           "level events."));
      mEvicting = true;
      return;
    }

    if (mEntries.Length() == 0) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Stopping evicting, there "
           "is no context to evict."));

      // Allow index to notify AsyncGetDiskConsumption callbacks.  The size is
      // actual again.
      CacheIndex::OnAsyncEviction(false);
      return;
    }

    SHA1Sum::Hash hash;
    rv = mEntries[0]->mIterator->GetNextHash(&hash);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - No more entries left in "
           "iterator. [iterator=%p, info=%p]",
           mEntries[0]->mIterator.get(), mEntries[0]->mInfo.get()));
      RemoveEvictInfoFromDisk(mEntries[0]->mInfo, mEntries[0]->mPinned,
                              mEntries[0]->mOrigin);
      mEntries.RemoveElementAt(0);
      continue;
    }
    if (NS_FAILED(rv)) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Iterator failed to "
           "provide next hash (shutdown?), keeping eviction info on disk."
           " [iterator=%p, info=%p]",
           mEntries[0]->mIterator.get(), mEntries[0]->mInfo.get()));
      mEntries.RemoveElementAt(0);
      continue;
    }

    LOG(
        ("CacheFileContextEvictor::EvictEntries() - Processing hash. "
         "[hash=%08x%08x%08x%08x%08x, iterator=%p, info=%p]",
         LOGSHA1(&hash), mEntries[0]->mIterator.get(),
         mEntries[0]->mInfo.get()));

    RefPtr<CacheFileHandle> handle;
    CacheFileIOManager::gInstance->mHandles.GetHandle(&hash,
                                                      getter_AddRefs(handle));
    if (handle) {
      // We doom any active handle in CacheFileIOManager::EvictByContext(), so
      // this must be a new one. Skip it.
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Skipping entry since we "
           "found an active handle. [handle=%p]",
           handle.get()));
      continue;
    }

    CacheIndex::EntryStatus status;
    bool pinned = false;
    auto callback = [&pinned](const CacheIndexEntry* aEntry) {
      pinned = aEntry->IsPinned();
    };
    rv = CacheIndex::HasEntry(hash, &status, callback);
    // This must never fail, since eviction (this code) happens only when the
    // index is up-to-date and thus the informatin is known.
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (pinned != mEntries[0]->mPinned) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Skipping entry since "
           "pinning "
           "doesn't match [evicting pinned=%d, entry pinned=%d]",
           mEntries[0]->mPinned, pinned));
      continue;
    }

    if (!mEntries[0]->mOrigin.IsEmpty()) {
      nsCOMPtr<nsIFile> file;
      CacheFileIOManager::gInstance->GetFile(&hash, getter_AddRefs(file));

      // Read metadata from the file synchronously
      RefPtr<CacheFileMetadata> metadata = new CacheFileMetadata();
      rv = metadata->SyncReadMetadata(file);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      // Now get the context + enhance id + URL from the key.
      nsAutoCString uriSpec;
      RefPtr<nsILoadContextInfo> info =
          CacheFileUtils::ParseKey(metadata->GetKey(), nullptr, &uriSpec);
      MOZ_ASSERT(info);
      if (!info) {
        continue;
      }

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), uriSpec);
      if (NS_FAILED(rv)) {
        LOG(
            ("CacheFileContextEvictor::EvictEntries() - Skipping entry since "
             "NS_NewURI failed to parse the uriSpec"));
        continue;
      }

      nsAutoString urlOrigin;
      rv = nsContentUtils::GetUTFOrigin(uri, urlOrigin);
      if (NS_FAILED(rv)) {
        LOG(
            ("CacheFileContextEvictor::EvictEntries() - Skipping entry since "
             "We failed to extract an origin"));
        continue;
      }

      if (!urlOrigin.Equals(mEntries[0]->mOrigin)) {
        LOG(
            ("CacheFileContextEvictor::EvictEntries() - Skipping entry since "
             "origin "
             "doesn't match"));
        continue;
      }
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
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Cannot get last modified "
           "time, skipping entry."));
      continue;
    }

    if (lastModifiedTime > mEntries[0]->mTimeStamp) {
      LOG(
          ("CacheFileContextEvictor::EvictEntries() - Skipping newer entry. "
           "[mTimeStamp=%" PRId64 ", lastModifiedTime=%" PRId64 "]",
           mEntries[0]->mTimeStamp, lastModifiedTime));
      continue;
    }

    LOG(("CacheFileContextEvictor::EvictEntries - Removing entry."));
    file->Remove(false);
    CacheIndex::RemoveEntry(&hash);
  }

  MOZ_ASSERT_UNREACHABLE("We should never get here");
}

}  // namespace mozilla::net
