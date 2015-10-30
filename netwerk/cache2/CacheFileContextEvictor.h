/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileContextEvictor__h__
#define CacheFileContextEvictor__h__

#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

class nsIFile;
class nsILoadContextInfo;

namespace mozilla {
namespace net {

class CacheIndexIterator;

struct CacheFileContextEvictorEntry
{
  nsCOMPtr<nsILoadContextInfo> mInfo;
  bool                         mPinned;
  PRTime                       mTimeStamp; // in milliseconds
  RefPtr<CacheIndexIterator> mIterator;
};

class CacheFileContextEvictor
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheFileContextEvictor)

  CacheFileContextEvictor();

private:
  virtual ~CacheFileContextEvictor();

public:
  nsresult Init(nsIFile *aCacheDirectory);

  // Returns number of contexts that are being evicted.
  uint32_t ContextsCount();
  // Start evicting given context.
  nsresult AddContext(nsILoadContextInfo *aLoadContextInfo, bool aPinned);
  // CacheFileIOManager calls this method when CacheIndex's state changes. We
  // check whether the index is up to date and start or stop evicting according
  // to index's state.
  nsresult CacheIndexStateChanged();
  // CacheFileIOManager calls this method to check whether an entry file should
  // be considered as evicted. It returns true when there is a matching context
  // info to the given key and the last modified time of the entry file is
  // earlier than the time stamp of the time when the context was added to the
  // evictor.
  nsresult WasEvicted(const nsACString &aKey, nsIFile *aFile,
                      bool *aEvictedAsPinned, bool *aEvictedAsNonPinned);

private:
  // Writes information about eviction of the given context to the disk. This is
  // done for every context added to the evictor to be able to recover eviction
  // after a shutdown or crash. When the context file is found after startup, we
  // restore mTimeStamp from the last modified time of the file.
  nsresult PersistEvictionInfoToDisk(nsILoadContextInfo *aLoadContextInfo, bool aPinned);
  // Once we are done with eviction for the given context, the eviction info is
  // removed from the disk.
  nsresult RemoveEvictInfoFromDisk(nsILoadContextInfo *aLoadContextInfo, bool aPinned);
  // Tries to load all contexts from the disk. This method is called just once
  // after startup.
  nsresult LoadEvictInfoFromDisk();
  nsresult GetContextFile(nsILoadContextInfo *aLoadContextInfo, bool aPinned,
                          nsIFile **_retval);

  void     CreateIterators();
  void     CloseIterators();
  void     StartEvicting();
  nsresult EvictEntries();

  // Whether eviction is in progress
  bool mEvicting;
  // Whether index is up to date. We wait with eviction until the index finishes
  // update process when it is outdated. NOTE: We also stop eviction in progress
  // when the index is found outdated, the eviction is restarted again once the
  // update process finishes.
  bool mIndexIsUpToDate;
  // Whether we already tried to restore unfinished jobs from previous run after
  // startup.
  static bool sDiskAlreadySearched;
  // Array of contexts being evicted.
  nsTArray<nsAutoPtr<CacheFileContextEvictorEntry> > mEntries;
  nsCOMPtr<nsIFile> mCacheDirectory;
  nsCOMPtr<nsIFile> mEntriesDir;
};

} // namespace net
} // namespace mozilla

#endif
