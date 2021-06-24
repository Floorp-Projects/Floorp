/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataStorage.h"

#include "mozilla/Assertions.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFileStreams.h"
#include "nsIMemoryReporter.h"
#include "nsIObserverService.h"
#include "nsISerialEventTarget.h"
#include "nsITimer.h"
#include "nsIThread.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "private/pprio.h"

#if defined(XP_WIN)
#  include "nsILocalFileWin.h"
#endif

// NB: Read DataStorage.h first.

// The default time between data changing and a write, in milliseconds.
static const uint32_t sDataStorageDefaultTimerDelay = 5u * 60u * 1000u;
// The maximum score an entry can have (prevents overflow)
static const uint32_t sMaxScore = UINT32_MAX;
// The maximum number of entries per type of data (limits resource use)
static const uint32_t sMaxDataEntries = 1024;
static const int64_t sOneDayInMicroseconds =
    int64_t(24 * 60 * 60) * PR_USEC_PER_SEC;

namespace mozilla {

class DataStorageMemoryReporter final : public nsIMemoryReporter {
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)
  ~DataStorageMemoryReporter() = default;

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) final {
    nsTArray<nsString> fileNames;
#define DATA_STORAGE(_) \
  fileNames.AppendElement(NS_LITERAL_STRING_FROM_CSTRING(#_ ".txt"));
#include "mozilla/DataStorageList.h"
#undef DATA_STORAGE
    for (const auto& file : fileNames) {
      RefPtr<DataStorage> ds = DataStorage::GetFromRawFileName(file);
      size_t amount = ds->SizeOfIncludingThis(MallocSizeOf);
      nsPrintfCString path("explicit/data-storage/%s",
                           NS_ConvertUTF16toUTF8(file).get());
      Unused << aHandleReport->Callback(
          ""_ns, path, KIND_HEAP, UNITS_BYTES, amount,
          "Memory used by PSM data storage cache."_ns, aData);
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(DataStorageMemoryReporter, nsIMemoryReporter)

NS_IMPL_ISUPPORTS(DataStorage, nsIObserver)

mozilla::StaticAutoPtr<DataStorage::DataStorages> DataStorage::sDataStorages;

DataStorage::DataStorage(const nsString& aFilename)
    : mMutex("DataStorage::mMutex"),
      mPendingWrite(false),
      mShuttingDown(false),
      mInitCalled(false),
      mReadyMonitor("DataStorage::mReadyMonitor"),
      mReady(false),
      mFilename(aFilename) {}

// static
already_AddRefed<DataStorage> DataStorage::Get(DataStorageClass aFilename) {
  switch (aFilename) {
#define DATA_STORAGE(_)     \
  case DataStorageClass::_: \
    return GetFromRawFileName(NS_LITERAL_STRING_FROM_CSTRING(#_ ".txt"));
#include "mozilla/DataStorageList.h"
#undef DATA_STORAGE
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid DataStorage type passed?");
      return nullptr;
  }
}

// static
already_AddRefed<DataStorage> DataStorage::GetFromRawFileName(
    const nsString& aFilename) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sDataStorages) {
    sDataStorages = new DataStorages();
    ClearOnShutdown(&sDataStorages);
  }
  return do_AddRef(sDataStorages->LookupOrInsertWith(
      aFilename, [&] { return RefPtr{new DataStorage(aFilename)}; }));
}

size_t DataStorage::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t sizeOfExcludingThis =
      mPersistentDataTable.ShallowSizeOfExcludingThis(aMallocSizeOf) +
      mTemporaryDataTable.ShallowSizeOfExcludingThis(aMallocSizeOf) +
      mPrivateDataTable.ShallowSizeOfExcludingThis(aMallocSizeOf) +
      mFilename.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return aMallocSizeOf(this) + sizeOfExcludingThis;
}

nsresult DataStorage::Init() {
  // Don't access the observer service or preferences off the main thread.
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("DataStorage::Init called off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (!XRE_IsParentProcess()) {
    MOZ_ASSERT_UNREACHABLE("DataStorage used in non-parent process");
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (AppShutdown::IsShuttingDown()) {
    // Reject new DataStorage instances if the browser is shutting down. There
    // is no guarantee that DataStorage writes will be able to be persisted if
    // we init during shutdown, so we return an error here to hopefully make
    // this more explicit and consistent.
    return NS_ERROR_NOT_AVAILABLE;
  }

  MutexAutoLock lock(mMutex);

  // Ignore attempts to initialize several times.
  if (mInitCalled) {
    return NS_OK;
  }

  mInitCalled = true;

  static bool memoryReporterRegistered = false;
  if (!memoryReporterRegistered) {
    nsresult rv = RegisterStrongMemoryReporter(new DataStorageMemoryReporter());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    memoryReporterRegistered = true;
  }

  nsCOMPtr<nsISerialEventTarget> target;
  nsresult rv = NS_CreateBackgroundTaskQueue(
      "DataStorage::mBackgroundTaskQueue", getter_AddRefs(target));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mBackgroundTaskQueue = new TaskQueue(target.forget());

  // For test purposes, we can set the write timer to be very fast.
  uint32_t timerDelayMS = Preferences::GetInt("test.datastorage.write_timer_ms",
                                              sDataStorageDefaultTimerDelay);
  rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(mTimer), DataStorage::TimerCallback, this, timerDelayMS,
      nsITimer::TYPE_REPEATING_SLACK_LOW_PRIORITY, "DataStorageTimer",
      mBackgroundTaskQueue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = AsyncReadData(lock);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return NS_ERROR_FAILURE;
  }
  // Clear private data as appropriate.
  os->AddObserver(this, "last-pb-context-exited", false);
  // Observe shutdown; save data and prevent any further writes.
  // We need to write to the profile directory, so we should listen for
  // profile-before-change so that we can safely write to the profile.
  os->AddObserver(this, "profile-before-change", false);
  // This is a backstop for xpcshell and other cases where
  // profile-before-change might not get sent.
  os->AddObserver(this, "xpcom-shutdown-threads", false);

  return NS_OK;
}

class DataStorage::Reader : public Runnable {
 public:
  explicit Reader(DataStorage* aDataStorage)
      : Runnable("DataStorage::Reader"), mDataStorage(aDataStorage) {}
  ~Reader();

 private:
  NS_DECL_NSIRUNNABLE

  static nsresult ParseLine(nsDependentCSubstring& aLine, nsCString& aKeyOut,
                            Entry& aEntryOut);

  RefPtr<DataStorage> mDataStorage;
};

DataStorage::Reader::~Reader() {
  // Notify that calls to Get can proceed.
  {
    MonitorAutoLock readyLock(mDataStorage->mReadyMonitor);
    mDataStorage->mReady = true;
    mDataStorage->mReadyMonitor.NotifyAll();
  }

  // This is for tests.
  nsCOMPtr<nsIRunnable> job = NewRunnableMethod<const char*>(
      "DataStorage::NotifyObservers", mDataStorage,
      &DataStorage::NotifyObservers, "data-storage-ready");
  nsresult rv = NS_DispatchToMainThread(job, NS_DISPATCH_NORMAL);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP
DataStorage::Reader::Run() {
  nsresult rv;
  // Concurrent operations on nsIFile objects are not guaranteed to be safe,
  // so we clone the file while holding the lock and then release the lock.
  // At that point, we can safely operate on the clone.
  nsCOMPtr<nsIFile> file;
  {
    MutexAutoLock lock(mDataStorage->mMutex);
    // If we don't have a profile, bail.
    if (!mDataStorage->mBackingFile) {
      return NS_OK;
    }
    rv = mDataStorage->mBackingFile->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), file);
  // If we failed for some reason other than the file doesn't exist, bail.
  if (NS_WARN_IF(NS_FAILED(rv) &&
                 rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&  // on Unix
                 rv != NS_ERROR_FILE_NOT_FOUND)) {             // on Windows
    return rv;
  }

  // If there is a file with data in it, read it. If there isn't,
  // we'll essentially fall through to notifying that we're good to go.
  nsCString data;
  if (fileInputStream) {
    // Limit to 2MB of data, but only store sMaxDataEntries entries.
    rv = NS_ConsumeStream(fileInputStream, 1u << 21, data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Atomically parse the data and insert the entries read.
  // Don't clear existing entries - they may have been inserted between when
  // this read was kicked-off and when it was run.
  {
    MutexAutoLock lock(mDataStorage->mMutex);
    // The backing file consists of a list of
    //   <key>\t<score>\t<last accessed time>\t<value>\n
    // The final \n is not optional; if it is not present the line is assumed
    // to be corrupt.
    int32_t currentIndex = 0;
    int32_t newlineIndex = 0;
    do {
      newlineIndex = data.FindChar('\n', currentIndex);
      // If there are no more newlines or the data table has too many
      // entries, we are done.
      if (newlineIndex < 0 ||
          mDataStorage->mPersistentDataTable.Count() >= sMaxDataEntries) {
        break;
      }

      nsDependentCSubstring line(data, currentIndex,
                                 newlineIndex - currentIndex);
      currentIndex = newlineIndex + 1;
      nsCString key;
      Entry entry;
      nsresult parseRV = ParseLine(line, key, entry);
      if (NS_SUCCEEDED(parseRV)) {
        // It could be the case that a newer entry was added before
        // we got around to reading the file. Don't overwrite new entries.
        mDataStorage->mPersistentDataTable.LookupOrInsert(key,
                                                          std::move(entry));
      }
    } while (true);

    Telemetry::Accumulate(Telemetry::DATA_STORAGE_ENTRIES,
                          mDataStorage->mPersistentDataTable.Count());
  }

  return NS_OK;
}

// The key must be a non-empty string containing no instances of '\t' or '\n',
// and must have a length no more than 256.
// The value must not contain '\n' and must have a length no more than 1024.
// The length limits are to prevent unbounded memory and disk usage.
/* static */
nsresult DataStorage::ValidateKeyAndValue(const nsCString& aKey,
                                          const nsCString& aValue) {
  if (aKey.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aKey.Length() > 256) {
    return NS_ERROR_INVALID_ARG;
  }
  int32_t delimiterIndex = aKey.FindChar('\t', 0);
  if (delimiterIndex >= 0) {
    return NS_ERROR_INVALID_ARG;
  }
  delimiterIndex = aKey.FindChar('\n', 0);
  if (delimiterIndex >= 0) {
    return NS_ERROR_INVALID_ARG;
  }
  delimiterIndex = aValue.FindChar('\n', 0);
  if (delimiterIndex >= 0) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aValue.Length() > 1024) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// Each line is: <key>\t<score>\t<last accessed time>\t<value>
// Where <score> is a uint32_t as a string, <last accessed time> is a
// int32_t as a string, and the rest are strings.
// <value> can contain anything but a newline.
// Returns a successful status if the line can be decoded into a key and entry.
// Otherwise, an error status is returned and the values assigned to the
// output parameters are in an undefined state.
/* static */
nsresult DataStorage::Reader::ParseLine(nsDependentCSubstring& aLine,
                                        nsCString& aKeyOut, Entry& aEntryOut) {
  // First find the indices to each part of the line.
  int32_t scoreIndex;
  scoreIndex = aLine.FindChar('\t', 0) + 1;
  if (scoreIndex <= 0) {
    return NS_ERROR_UNEXPECTED;
  }
  int32_t accessedIndex = aLine.FindChar('\t', scoreIndex) + 1;
  if (accessedIndex <= 0) {
    return NS_ERROR_UNEXPECTED;
  }
  int32_t valueIndex = aLine.FindChar('\t', accessedIndex) + 1;
  if (valueIndex <= 0) {
    return NS_ERROR_UNEXPECTED;
  }

  // Now make substrings based on where each part is.
  nsDependentCSubstring keyPart(aLine, 0, scoreIndex - 1);
  nsDependentCSubstring scorePart(aLine, scoreIndex,
                                  accessedIndex - scoreIndex - 1);
  nsDependentCSubstring accessedPart(aLine, accessedIndex,
                                     valueIndex - accessedIndex - 1);
  nsDependentCSubstring valuePart(aLine, valueIndex);

  nsresult rv;
  rv = DataStorage::ValidateKeyAndValue(nsCString(keyPart),
                                        nsCString(valuePart));
  if (NS_FAILED(rv)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Now attempt to decode the score part as a uint32_t.
  // XXX nsDependentCSubstring doesn't support ToInteger
  int32_t integer = nsCString(scorePart).ToInteger(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (integer < 0) {
    return NS_ERROR_UNEXPECTED;
  }
  aEntryOut.mScore = (uint32_t)integer;

  integer = nsCString(accessedPart).ToInteger(&rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (integer < 0) {
    return NS_ERROR_UNEXPECTED;
  }
  aEntryOut.mLastAccessed = integer;

  // Now set the key and value.
  aKeyOut.Assign(keyPart);
  aEntryOut.mValue.Assign(valuePart);

  return NS_OK;
}

nsresult DataStorage::AsyncReadData(const MutexAutoLock& /*aProofOfLock*/) {
  // Allocate a Reader so that even if it isn't dispatched,
  // the data-storage-ready notification will be fired and Get
  // will be able to proceed (this happens in its destructor).
  nsCOMPtr<nsIRunnable> job(new Reader(this));
  nsresult rv;
  // If we don't have a profile directory, this will fail.
  // That's okay - it just means there is no persistent state.
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(mBackingFile));
  if (NS_FAILED(rv)) {
    mBackingFile = nullptr;
    return NS_OK;
  }

  rv = mBackingFile->Append(mFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mBackgroundTaskQueue->Dispatch(job.forget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool DataStorage::IsReady() {
  MonitorAutoLock readyLock(mReadyMonitor);
  return mReady;
}

void DataStorage::WaitForReady() {
  MOZ_DIAGNOSTIC_ASSERT(mInitCalled, "Waiting before Init() has been called?");

  MonitorAutoLock readyLock(mReadyMonitor);
  while (!mReady) {
    readyLock.Wait();
  }
  MOZ_ASSERT(mReady);
}

nsCString DataStorage::Get(const nsCString& aKey, DataStorageType aType) {
  WaitForReady();
  MutexAutoLock lock(mMutex);

  Entry entry;
  bool foundValue = GetInternal(aKey, &entry, aType, lock);
  if (!foundValue) {
    return ""_ns;
  }

  // If we're here, we found a value. Maybe update its score.
  if (entry.UpdateScore()) {
    PutInternal(aKey, entry, aType, lock);
  }

  return entry.mValue;
}

bool DataStorage::GetInternal(const nsCString& aKey, Entry* aEntry,
                              DataStorageType aType,
                              const MutexAutoLock& aProofOfLock) {
  DataStorageTable& table = GetTableForType(aType, aProofOfLock);
  bool foundValue = table.Get(aKey, aEntry);
  return foundValue;
}

DataStorage::DataStorageTable& DataStorage::GetTableForType(
    DataStorageType aType, const MutexAutoLock& /*aProofOfLock*/) {
  switch (aType) {
    case DataStorage_Persistent:
      return mPersistentDataTable;
    case DataStorage_Temporary:
      return mTemporaryDataTable;
    case DataStorage_Private:
      return mPrivateDataTable;
  }

  MOZ_CRASH("given bad DataStorage storage type");
}

void DataStorage::ReadAllFromTable(DataStorageType aType,
                                   nsTArray<DataStorageItem>* aItems,
                                   const MutexAutoLock& aProofOfLock) {
  for (auto iter = GetTableForType(aType, aProofOfLock).Iter(); !iter.Done();
       iter.Next()) {
    DataStorageItem* item = aItems->AppendElement();
    item->key = iter.Key();
    item->value = iter.Data().mValue;
    item->type = aType;
  }
}

void DataStorage::GetAll(nsTArray<DataStorageItem>* aItems) {
  WaitForReady();
  MutexAutoLock lock(mMutex);

  aItems->SetCapacity(mPersistentDataTable.Count() +
                      mTemporaryDataTable.Count() + mPrivateDataTable.Count());
  ReadAllFromTable(DataStorage_Persistent, aItems, lock);
  ReadAllFromTable(DataStorage_Temporary, aItems, lock);
  ReadAllFromTable(DataStorage_Private, aItems, lock);
}

// Limit the number of entries per table. This is to prevent unbounded
// resource use. The eviction strategy is as follows:
// - An entry's score is incremented once for every day it is accessed.
// - Evict an entry with score no more than any other entry in the table
//   (this is the same as saying evict the entry with the lowest score,
//    except for when there are multiple entries with the lowest score,
//    in which case one of them is evicted - which one is not specified).
void DataStorage::MaybeEvictOneEntry(DataStorageType aType,
                                     const MutexAutoLock& aProofOfLock) {
  DataStorageTable& table = GetTableForType(aType, aProofOfLock);
  if (table.Count() >= sMaxDataEntries) {
    KeyAndEntry toEvict;
    // If all entries have score sMaxScore, this won't actually remove
    // anything. This will never happen, however, because having that high
    // a score either means someone tampered with the backing file or every
    // entry has been accessed once a day for ~4 billion days.
    // The worst that will happen is there will be 1025 entries in the
    // persistent data table, with the 1025th entry being replaced every time
    // data with a new key is inserted into the table. This is bad but
    // ultimately not that concerning, considering that if an attacker can
    // modify data in the profile, they can cause much worse harm.
    toEvict.mEntry.mScore = sMaxScore;

    for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
      Entry entry = iter.UserData();
      if (entry.mScore < toEvict.mEntry.mScore) {
        toEvict.mKey = iter.Key();
        toEvict.mEntry = entry;
      }
    }

    table.Remove(toEvict.mKey);
  }
}

nsresult DataStorage::Put(const nsCString& aKey, const nsCString& aValue,
                          DataStorageType aType) {
  WaitForReady();
  MutexAutoLock lock(mMutex);

  nsresult rv;
  rv = ValidateKeyAndValue(aKey, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Entry entry;
  bool exists = GetInternal(aKey, &entry, aType, lock);
  if (exists) {
    entry.UpdateScore();
  } else {
    MaybeEvictOneEntry(aType, lock);
  }
  entry.mValue = aValue;
  return PutInternal(aKey, entry, aType, lock);
}

nsresult DataStorage::PutInternal(const nsCString& aKey, Entry& aEntry,
                                  DataStorageType aType,
                                  const MutexAutoLock& aProofOfLock) {
  DataStorageTable& table = GetTableForType(aType, aProofOfLock);
  table.InsertOrUpdate(aKey, aEntry);

  if (aType == DataStorage_Persistent) {
    mPendingWrite = true;
  }

  return NS_OK;
}

void DataStorage::Remove(const nsCString& aKey, DataStorageType aType) {
  WaitForReady();
  MutexAutoLock lock(mMutex);

  DataStorageTable& table = GetTableForType(aType, lock);
  table.Remove(aKey);

  if (aType == DataStorage_Persistent) {
    mPendingWrite = true;
  }
}

class DataStorage::Writer final : public Runnable {
 public:
  Writer(nsCString& aData, DataStorage* aDataStorage)
      : Runnable("DataStorage::Writer"),
        mData(aData),
        mDataStorage(aDataStorage) {}

 protected:
  NS_DECL_NSIRUNNABLE

  nsCString mData;
  RefPtr<DataStorage> mDataStorage;
};

NS_IMETHODIMP
DataStorage::Writer::Run() {
  nsresult rv;
  // Concurrent operations on nsIFile objects are not guaranteed to be safe,
  // so we clone the file while holding the lock and then release the lock.
  // At that point, we can safely operate on the clone.
  nsCOMPtr<nsIFile> file;
  {
    MutexAutoLock lock(mDataStorage->mMutex);
    // If we don't have a profile, bail.
    if (!mDataStorage->mBackingFile) {
      return NS_OK;
    }
    rv = mDataStorage->mBackingFile->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file,
                                   PR_CREATE_FILE | PR_TRUNCATE | PR_WRONLY);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // When the output stream is null, it means we don't have a profile.
  if (!outputStream) {
    return NS_OK;
  }

  const char* ptr = mData.get();
  uint32_t remaining = mData.Length();
  uint32_t written = 0;
  while (remaining > 0) {
    rv = outputStream->Write(ptr, remaining, &written);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    remaining -= written;
    ptr += written;
  }

  // Observed by tests.
  nsCOMPtr<nsIRunnable> job = NewRunnableMethod<const char*>(
      "DataStorage::NotifyObservers", mDataStorage,
      &DataStorage::NotifyObservers, "data-storage-written");
  rv = NS_DispatchToMainThread(job, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DataStorage::AsyncWriteData(const MutexAutoLock& /*aProofOfLock*/) {
  if (!mPendingWrite || mShuttingDown || !mBackingFile) {
    return NS_OK;
  }

  nsCString output;
  for (auto iter = mPersistentDataTable.Iter(); !iter.Done(); iter.Next()) {
    Entry entry = iter.UserData();
    output.Append(iter.Key());
    output.Append('\t');
    output.AppendInt(entry.mScore);
    output.Append('\t');
    output.AppendInt(entry.mLastAccessed);
    output.Append('\t');
    output.Append(entry.mValue);
    output.Append('\n');
  }

  nsCOMPtr<nsIRunnable> job(new Writer(output, this));
  nsresult rv = mBackgroundTaskQueue->Dispatch(job.forget());
  mPendingWrite = false;
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DataStorage::Clear() {
  WaitForReady();
  MutexAutoLock lock(mMutex);
  mPersistentDataTable.Clear();
  mTemporaryDataTable.Clear();
  mPrivateDataTable.Clear();
  mPendingWrite = true;

  // Asynchronously clear the file. This is similar to the permission manager
  // in that it doesn't wait to synchronously remove the data from its backing
  // storage either.
  return AsyncWriteData(lock);
}

/* static */
void DataStorage::TimerCallback(nsITimer* aTimer, void* aClosure) {
  RefPtr<DataStorage> aDataStorage = (DataStorage*)aClosure;
  MutexAutoLock lock(aDataStorage->mMutex);
  Unused << aDataStorage->AsyncWriteData(lock);
}

void DataStorage::NotifyObservers(const char* aTopic) {
  // Don't access the observer service off the main thread.
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE(
        "DataStorage::NotifyObservers called off main thread");
    return;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, aTopic, mFilename.get());
  }
}

void DataStorage::ShutdownTimer() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mTimer) {
    nsresult rv = mTimer->Cancel();
    Unused << NS_WARN_IF(NS_FAILED(rv));
    mTimer = nullptr;
  }
}

//------------------------------------------------------------
// DataStorage::nsIObserver
//------------------------------------------------------------

NS_IMETHODIMP
DataStorage::Observe(nsISupports* /*aSubject*/, const char* aTopic,
                     const char16_t* /*aData*/) {
  if (!NS_IsMainThread()) {
    MOZ_ASSERT_UNREACHABLE("DataStorage::Observe called off main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    MutexAutoLock lock(mMutex);
    mPrivateDataTable.Clear();
    return NS_OK;
  }

  if (strcmp(aTopic, "profile-before-change") == 0 ||
      strcmp(aTopic, "xpcom-shutdown-threads") == 0) {
    RefPtr<TaskQueue> taskQueueToAwait;
    {
      MutexAutoLock lock(mMutex);
      if (!mShuttingDown) {
        nsresult rv = AsyncWriteData(lock);
        Unused << NS_WARN_IF(NS_FAILED(rv));
        mShuttingDown = true;
        mBackgroundTaskQueue->BeginShutdown();
        taskQueueToAwait = mBackgroundTaskQueue;
      }
    }
    // Tasks on the background queue may take the lock, so it can't be held
    // while waiting for them to finish.
    if (taskQueueToAwait) {
      taskQueueToAwait->AwaitShutdownAndIdle();
    }
    ShutdownTimer();
  }

  return NS_OK;
}

DataStorage::Entry::Entry()
    : mScore(0), mLastAccessed((int32_t)(PR_Now() / sOneDayInMicroseconds)) {}

// Updates this entry's score. Returns true if the score has actually changed.
// If it's been less than a day since this entry has been accessed, the score
// does not change. Otherwise, the score increases by 1.
// The default score is 0. The maximum score is the maximum value that can
// be represented by an unsigned 32 bit integer.
// This is to handle evictions from our tables, which in turn is to prevent
// unbounded resource use.
bool DataStorage::Entry::UpdateScore() {
  int32_t nowInDays = (int32_t)(PR_Now() / sOneDayInMicroseconds);
  int32_t daysSinceAccessed = (nowInDays - mLastAccessed);

  // Update the last accessed time.
  mLastAccessed = nowInDays;

  // If it's been less than a day since we've been accessed,
  // the score isn't updated.
  if (daysSinceAccessed < 1) {
    return false;
  }

  // Otherwise, increment the score (but don't overflow).
  if (mScore < sMaxScore) {
    mScore++;
  }
  return true;
}

}  // namespace mozilla
