/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DataStorage_h
#define mozilla_DataStorage_h

#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsString.h"

namespace mozilla {

/**
 * DataStorage is a threadsafe, generic, narrow string-based hash map that
 * persists data on disk and additionally handles temporary and private data.
 * However, if used in a context where there is no profile directory, data
 * will not be persisted.
 *
 * Its lifecycle is as follows:
 * - Allocate with a filename (this is or will eventually be a file in the
 *   profile directory, if the profile exists).
 * - Call Init() from the main thread. This spins off an asynchronous read
 *   of the backing file.
 * - Eventually observers of the topic "data-storage-ready" will be notified
 *   with the backing filename as the data in the notification when this
 *   has completed.
 * - Should the profile directory not be available, (e.g. in xpcshell),
 *   DataStorage will not initially read any persistent data. The
 *   "data-storage-ready" event will still be emitted. This follows semantics
 *   similar to the permission manager and allows tests that test
 *   unrelated components to proceed without a profile.
 * - When any persistent data changes, a timer is initialized that will
 *   eventually asynchronously write all persistent data to the backing file.
 *   When this happens, observers will be notified with the topic
 *   "data-storage-written" and the backing filename as the data.
 *   It is possible to receive a "data-storage-written" event while there exist
 *   pending persistent data changes. However, those changes will cause the
 *   timer to be reinitialized and another "data-storage-written" event will
 *   be sent.
 * - When DataStorage observes the topic "profile-before-change" in
 *   anticipation of shutdown, all persistent data is synchronously written to
 *   the backing file. The worker thread responsible for these writes is then
 *   disabled to prevent further writes to that file (the delayed-write timer
 *   is cancelled when this happens).
 * - For testing purposes, the preference "test.datastorage.write_timer_ms" can
 *   be set to cause the asynchronous writing of data to happen more quickly.
 * - To prevent unbounded memory and disk use, the number of entries in each
 *   table is limited to 1024. Evictions are handled in by a modified LRU scheme
 *   (see implementation comments).
 * - NB: Instances of DataStorage have long lifetimes because they are strong
 *   observers of events and won't go away until the observer service does.
 *
 * For each key/value:
 * - The key must be a non-empty string containing no instances of '\t' or '\n'
 *   (this is a limitation of how the data is stored and will be addressed in
 *   the future).
 * - The key must have a length no more than 256.
 * - The value must not contain '\n' and must have a length no more than 1024.
 *   (the length limits are to prevent unbounded disk and memory usage)
 */

/**
 * Data that is DataStorage_Persistent is saved on disk. DataStorage_Temporary
 * and DataStorage_Private are not saved. DataStorage_Private is meant to
 * only be set and accessed from private contexts. It will be cleared upon
 * observing the event "last-pb-context-exited".
 */
enum DataStorageType {
  DataStorage_Persistent,
  DataStorage_Temporary,
  DataStorage_Private
};

class DataStorage : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // If there is a profile directory, there is or will eventually be a file
  // by the name specified by aFilename there.
  explicit DataStorage(const nsString& aFilename);

  // Initializes the DataStorage. Must be called before using.
  // aDataWillPersist returns whether or not data can be persistently saved.
  nsresult Init(/*out*/bool& aDataWillPersist);
  // Given a key and a type of data, returns a value. Returns an empty string if
  // the key is not present for that type of data. If Get is called before the
  // "data-storage-ready" event is observed, it will block. NB: It is not
  // currently possible to differentiate between missing data and data that is
  // the empty string.
  nsCString Get(const nsCString& aKey, DataStorageType aType);
  // Give a key, value, and type of data, adds an entry as appropriate.
  // Updates existing entries.
  nsresult Put(const nsCString& aKey, const nsCString& aValue,
               DataStorageType aType);
  // Given a key and type of data, removes an entry if present.
  void Remove(const nsCString& aKey, DataStorageType aType);
  // Removes all entries of all types of data.
  nsresult Clear();

private:
  virtual ~DataStorage();

  class Writer;
  class Reader;

  class Entry
  {
  public:
    Entry();
    bool UpdateScore();

    uint32_t mScore;
    int32_t mLastAccessed; // the last accessed time in days since the epoch
    nsCString mValue;
  };

  // Utility class for scanning tables for an entry to evict.
  class KeyAndEntry
  {
  public:
    nsCString mKey;
    Entry mEntry;
  };

  typedef nsDataHashtable<nsCStringHashKey, Entry> DataStorageTable;

  void WaitForReady();
  nsresult AsyncWriteData(const MutexAutoLock& aProofOfLock);
  nsresult AsyncReadData(bool& aHaveProfileDir,
                         const MutexAutoLock& aProofOfLock);
  nsresult AsyncSetTimer(const MutexAutoLock& aProofOfLock);
  nsresult DispatchShutdownTimer(const MutexAutoLock& aProofOfLock);

  static nsresult ValidateKeyAndValue(const nsCString& aKey,
                                      const nsCString& aValue);
  static void TimerCallback(nsITimer* aTimer, void* aClosure);
  static PLDHashOperator WriteDataCallback(const nsACString& aKey, Entry aEntry,
                                           void* aArg);
  static PLDHashOperator EvictCallback(const nsACString& aKey, Entry aEntry,
                                       void* aArg);
  void SetTimer();
  void ShutdownTimer();
  void NotifyObservers(const char* aTopic);

  bool GetInternal(const nsCString& aKey, Entry* aEntry, DataStorageType aType,
                   const MutexAutoLock& aProofOfLock);
  nsresult PutInternal(const nsCString& aKey, Entry& aEntry,
                       DataStorageType aType,
                       const MutexAutoLock& aProofOfLock);
  void MaybeEvictOneEntry(DataStorageType aType,
                          const MutexAutoLock& aProofOfLock);
  DataStorageTable& GetTableForType(DataStorageType aType,
                                    const MutexAutoLock& aProofOfLock);

  Mutex mMutex; // This mutex protects access to the following members:
  DataStorageTable  mPersistentDataTable;
  DataStorageTable  mTemporaryDataTable;
  DataStorageTable  mPrivateDataTable;
  nsCOMPtr<nsIThread> mWorkerThread;
  nsCOMPtr<nsIFile> mBackingFile;
  nsCOMPtr<nsITimer> mTimer; // All uses after init must be on the worker thread
  uint32_t mTimerDelay; // in milliseconds
  bool mPendingWrite; // true if a write is needed but hasn't been dispatched
  bool mShuttingDown;
  // (End list of members protected by mMutex)

  Monitor mReadyMonitor; // Do not acquire this at the same time as mMutex.
  bool mReady; // Indicates that saved data has been read and Get can proceed.

  const nsString mFilename;
};

} // namespace mozilla

#endif // mozilla_DataStorage_h
