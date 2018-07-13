//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCacheV4.h"
#include "HashStore.h"
#include "mozilla/Unused.h"
#include <string>

// MOZ_LOG=UrlClassifierDbService:5
extern mozilla::LazyLogModule gUrlClassifierDbServiceLog;
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

#define METADATA_SUFFIX NS_LITERAL_CSTRING(".metadata")

namespace mozilla {
namespace safebrowsing {

const int LookupCacheV4::VER = 4;
const uint32_t LookupCacheV4::MAX_METADATA_VALUE_LENGTH = 256;

// Prefixes coming from updates and VLPrefixSet are both stored in the HashTable
// where the (key, value) pair is a prefix size and a lexicographic-sorted string.
// The difference is prefixes from updates use std:string(to avoid additional copies)
// and prefixes from VLPrefixSet use nsCString.
// This class provides a common interface for the partial update algorithm to make it
// easier to operate on two different kind prefix string map..
class VLPrefixSet
{
public:
  explicit VLPrefixSet(const PrefixStringMap& aMap);

  // This function will merge the prefix map in VLPrefixSet to aPrefixMap.
  void Merge(PrefixStringMap& aPrefixMap);

  // Find the smallest string from the map in VLPrefixSet.
  bool GetSmallestPrefix(nsACString& aOutString) const;

  // Return the number of prefixes in the map
  uint32_t Count() const { return mCount; }

private:
  // PrefixString structure contains a lexicographic-sorted string with
  // a |pos| variable to indicate which substring we are pointing to right now.
  // |pos| increases each time GetSmallestPrefix finds the smallest string.
  struct PrefixString {
    PrefixString(const nsACString& aStr, uint32_t aSize)
      : data(aStr)
      , pos(0)
      , size(aSize)
    {
      MOZ_ASSERT(data.Length() % size == 0,
                 "PrefixString length must be a multiple of the prefix size.");
    }

    void getRemainingString(nsACString& out) {
      MOZ_ASSERT(out.IsEmpty());
      if (remaining() > 0) {
        out = Substring(data, pos);
      }
    }
    void getPrefix(nsACString& out) {
      MOZ_ASSERT(out.IsEmpty());
      if (remaining() >= size) {
        out = Substring(data, pos, size);
      } else {
        MOZ_ASSERT(remaining() == 0,
                   "Remaining bytes but not enough for a (size)-byte prefix.");
      }
    }
    void next() {
      pos += size;
      MOZ_ASSERT(pos <= data.Length());
    }
    uint32_t remaining() {
      return data.Length() - pos;
      MOZ_ASSERT(pos <= data.Length());
    }

    nsCString data;
    uint32_t pos;
    uint32_t size;
  };

  nsClassHashtable<nsUint32HashKey, PrefixString> mMap;
  uint32_t mCount;
};

nsresult
LookupCacheV4::Init()
{
  mVLPrefixSet = new VariableLengthPrefixSet();
  nsresult rv = mVLPrefixSet->Init(mTableName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
LookupCacheV4::Has(const Completion& aCompletion,
                   bool* aHas,
                   uint32_t* aMatchLength,
                   bool* aConfirmed)
{
  *aHas = *aConfirmed = false;
  *aMatchLength = 0;

  uint32_t length = 0;
  nsDependentCSubstring fullhash;
  fullhash.Rebind((const char *)aCompletion.buf, COMPLETE_SIZE);

  nsresult rv = mVLPrefixSet->Matches(fullhash, &length);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(length == 0 || (length >= PREFIX_SIZE && length <= COMPLETE_SIZE));

  *aHas = length >= PREFIX_SIZE;
  *aMatchLength = length;

  if (LOG_ENABLED()) {
    uint32_t prefix = aCompletion.ToUint32();
    LOG(("Probe in V4 %s: %X, found %d, complete %d", mTableName.get(),
          prefix, *aHas, length == COMPLETE_SIZE));
  }

  // Check if fullhash match any prefix in the local database
  if (!(*aHas)) {
    return NS_OK;
  }

  // Even though V4 supports variable-length prefix, we always send 4-bytes for
  // completion (Bug 1323953). This means cached prefix length is also 4-bytes.
  return CheckCache(aCompletion, aHas, aConfirmed);
}

bool
LookupCacheV4::IsEmpty() const
{
  bool isEmpty;
  mVLPrefixSet->IsEmpty(&isEmpty);
  return isEmpty;
}

nsresult
LookupCacheV4::Build(PrefixStringMap& aPrefixMap)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_VLPS_CONSTRUCT_TIME> timer;

  nsresult rv = mVLPrefixSet->SetPrefixes(aPrefixMap);
  NS_ENSURE_SUCCESS(rv, rv);
  mPrimed = true;

  return rv;
}

nsresult
LookupCacheV4::GetPrefixes(PrefixStringMap& aPrefixMap)
{
  if (!mPrimed) {
    // This can happen if its a new table, so no error.
    LOG(("GetPrefixes from empty LookupCache"));
    return NS_OK;
  }
  return mVLPrefixSet->GetPrefixes(aPrefixMap);
}

nsresult
LookupCacheV4::GetFixedLengthPrefixes(FallibleTArray<uint32_t>& aPrefixes)
{
  return mVLPrefixSet->GetFixedLengthPrefixes(aPrefixes);
}

nsresult
LookupCacheV4::ClearPrefixes()
{
  // Clear by seting a empty map
  PrefixStringMap map;
  return mVLPrefixSet->SetPrefixes(map);
}

nsresult
LookupCacheV4::StoreToFile(nsCOMPtr<nsIFile>& aFile)
{
  return mVLPrefixSet->StoreToFile(aFile);
}

nsresult
LookupCacheV4::LoadFromFile(nsCOMPtr<nsIFile>& aFile)
{
  nsresult rv = mVLPrefixSet->LoadFromFile(aFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString state, checksum;
  rv = LoadMetadata(state, checksum);
  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_VLPS_METADATA_CORRUPT,
                        rv == NS_ERROR_FILE_CORRUPTED);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = VerifyChecksum(checksum);
  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_VLPS_LOAD_CORRUPT,
                        rv == NS_ERROR_FILE_CORRUPTED);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

size_t
LookupCacheV4::SizeOfPrefixSet() const
{
  return mVLPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
}

static nsresult
AppendPrefixToMap(PrefixStringMap& prefixes, const nsACString& prefix)
{
  uint32_t len = prefix.Length();
  MOZ_ASSERT(len >= PREFIX_SIZE && len <= COMPLETE_SIZE);
  if (!len) {
    return NS_OK;
  }

  nsCString* prefixString = prefixes.LookupOrAdd(len);
  if (!prefixString->Append(prefix, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

static nsresult
InitCrypto(nsCOMPtr<nsICryptoHash>& aCrypto)
{
  nsresult rv;
  aCrypto = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aCrypto->Init(nsICryptoHash::SHA256);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InitCrypto failed");

  return rv;
}

// Read prefix into a buffer and also update the hash which
// keeps track of the checksum
static void
UpdateChecksum(nsICryptoHash* aCrypto, const nsACString& aPrefix)
{
  MOZ_ASSERT(aCrypto);
  aCrypto->Update(reinterpret_cast<uint8_t*>(const_cast<char*>(
                  aPrefix.BeginReading())),
                  aPrefix.Length());
}

// Please see https://bug1287058.bmoattachments.org/attachment.cgi?id=8795366
// for detail about partial update algorithm.
nsresult
LookupCacheV4::ApplyUpdate(RefPtr<TableUpdateV4> aTableUpdate,
                           PrefixStringMap& aInputMap,
                           PrefixStringMap& aOutputMap)
{
  MOZ_ASSERT(aOutputMap.IsEmpty());

  nsCOMPtr<nsICryptoHash> crypto;
  nsresult rv = InitCrypto(crypto);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // oldPSet contains prefixes we already have or we just merged last round.
  // addPSet contains prefixes stored in tableUpdate which should be merged with oldPSet.
  VLPrefixSet oldPSet(aInputMap);
  VLPrefixSet addPSet(aTableUpdate->Prefixes());

  // RemovalIndiceArray is a sorted integer array indicating the index of prefix we should
  // remove from the old prefix set(according to lexigraphic order).
  // |removalIndex| is the current index of RemovalIndiceArray.
  // |numOldPrefixPicked| is used to record how many prefixes we picked from the old map.
  const TableUpdateV4::RemovalIndiceArray& removalArray = aTableUpdate->RemovalIndices();
  uint32_t removalIndex = 0;
  int32_t numOldPrefixPicked = -1;

  nsAutoCString smallestOldPrefix;
  nsAutoCString smallestAddPrefix;

  bool isOldMapEmpty = false, isAddMapEmpty = false;

  // This is used to avoid infinite loop for partial update algorithm.
  // The maximum loops will be the number of old prefixes plus the number of add prefixes.
  int32_t index = oldPSet.Count() + addPSet.Count() + 1;
  for(;index > 0; index--) {
    // Get smallest prefix from the old prefix set if we don't have one
    if (smallestOldPrefix.IsEmpty() && !isOldMapEmpty) {
      isOldMapEmpty = !oldPSet.GetSmallestPrefix(smallestOldPrefix);
    }

    // Get smallest prefix from add prefix set if we don't have one
    if (smallestAddPrefix.IsEmpty() && !isAddMapEmpty) {
      isAddMapEmpty = !addPSet.GetSmallestPrefix(smallestAddPrefix);
    }

    bool pickOld;

    // If both prefix sets are not empty, then compare to find the smaller one.
    if (!isOldMapEmpty && !isAddMapEmpty) {
      if (smallestOldPrefix == smallestAddPrefix) {
        LOG(("Add prefix should not exist in the original prefix set."));
        return NS_ERROR_UC_UPDATE_DUPLICATE_PREFIX;
      }

      // Compare the smallest string in old prefix set and add prefix set,
      // merge the smaller one into new map to ensure merged string still
      // follows lexigraphic order.
      pickOld = smallestOldPrefix < smallestAddPrefix;
    } else if (!isOldMapEmpty && isAddMapEmpty) {
      pickOld = true;
    } else if (isOldMapEmpty && !isAddMapEmpty) {
      pickOld = false;
    // If both maps are empty, then partial update is complete.
    } else {
      break;
    }

    if (pickOld) {
      numOldPrefixPicked++;

      // If the number of picks from old map matches the removalIndex, then this prefix
      // will be removed by not merging it to new map.
      if (removalIndex < removalArray.Length() &&
          numOldPrefixPicked == removalArray[removalIndex]) {
        removalIndex++;
      } else {
        rv = AppendPrefixToMap(aOutputMap, smallestOldPrefix);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        UpdateChecksum(crypto, smallestOldPrefix);
      }
      smallestOldPrefix.SetLength(0);
    } else {
      rv = AppendPrefixToMap(aOutputMap, smallestAddPrefix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      UpdateChecksum(crypto, smallestAddPrefix);
      smallestAddPrefix.SetLength(0);
    }
  }

  // We expect index will be greater to 0 because max number of runs will be
  // the number of original prefix plus add prefix.
  if (index <= 0) {
    LOG(("There are still prefixes remaining after reaching maximum runs."));
    return NS_ERROR_UC_UPDATE_INFINITE_LOOP;
  }

  if (removalIndex < removalArray.Length()) {
    LOG(("There are still prefixes to remove after exhausting the old PrefixSet."));
    return NS_ERROR_UC_UPDATE_WRONG_REMOVAL_INDICES;
  }

  nsAutoCString checksum;
  crypto->Finish(false, checksum);
  if (aTableUpdate->Checksum().IsEmpty()) {
    LOG(("Update checksum missing."));
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UPDATE_ERROR, mProvider,
        NS_ERROR_GET_CODE(NS_ERROR_UC_UPDATE_MISSING_CHECKSUM));

    // Generate our own checksum to tableUpdate to ensure there is always
    // checksum in .metadata
    std::string stdChecksum(checksum.BeginReading(), checksum.Length());
    aTableUpdate->NewChecksum(stdChecksum);
  } else if (aTableUpdate->Checksum() != checksum){
    LOG(("Checksum mismatch after applying partial update"));
    return NS_ERROR_UC_UPDATE_CHECKSUM_MISMATCH;
  }

  return NS_OK;
}

nsresult
LookupCacheV4::AddFullHashResponseToCache(const FullHashResponseMap& aResponseMap)
{
  CopyClassHashTable<FullHashResponseMap>(aResponseMap, mFullHashCache);

  return NS_OK;
}

nsresult
LookupCacheV4::VerifyChecksum(const nsACString& aChecksum)
{
  nsCOMPtr<nsICryptoHash> crypto;
  nsresult rv = InitCrypto(crypto);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PrefixStringMap map;
  mVLPrefixSet->GetPrefixes(map);

  VLPrefixSet loadPSet(map);
  uint32_t index = loadPSet.Count() + 1;
  for(;index > 0; index--) {
    nsAutoCString prefix;
    if (!loadPSet.GetSmallestPrefix(prefix)) {
      break;
    }
    UpdateChecksum(crypto, prefix);
  }

  nsAutoCString checksum;
  crypto->Finish(false, checksum);

  if (checksum != aChecksum) {
    LOG(("Checksum mismatch when loading prefixes from file."));
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
// A set of lightweight functions for reading/writing value from/to file.

namespace {

template<typename T>
struct ValueTraits
{
  static_assert(sizeof(T) <= LookupCacheV4::MAX_METADATA_VALUE_LENGTH,
                "LookupCacheV4::MAX_METADATA_VALUE_LENGTH is too small.");
  static uint32_t Length(const T& aValue) { return sizeof(T); }
  static char* WritePtr(T& aValue, uint32_t aLength) { return (char*)&aValue; }
  static const char* ReadPtr(const T& aValue) { return (char*)&aValue; }
  static bool IsFixedLength() { return true; }
};

template<>
struct ValueTraits<nsACString>
{
  static bool IsFixedLength() { return false; }

  static uint32_t Length(const nsACString& aValue)
  {
    return aValue.Length();
  }

  static char* WritePtr(nsACString& aValue, uint32_t aLength)
  {
    aValue.SetLength(aLength);
    return aValue.BeginWriting();
  }

  static const char* ReadPtr(const nsACString& aValue)
  {
    return aValue.BeginReading();
  }
};

template<typename T> static nsresult
WriteValue(nsIOutputStream *aOutputStream, const T& aValue)
{
  uint32_t writeLength = ValueTraits<T>::Length(aValue);
  MOZ_ASSERT(writeLength <= LookupCacheV4::MAX_METADATA_VALUE_LENGTH,
             "LookupCacheV4::MAX_METADATA_VALUE_LENGTH is too small.");
  if (!ValueTraits<T>::IsFixedLength()) {
    // We need to write out the variable value length.
    nsresult rv = WriteValue(aOutputStream, writeLength);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out the value.
  auto valueReadPtr = ValueTraits<T>::ReadPtr(aValue);
  uint32_t written;
  nsresult rv = aOutputStream->Write(valueReadPtr, writeLength, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_WARN_IF(written != writeLength)) {
    return NS_ERROR_FAILURE;
  }

  return rv;
}

template<typename T> static nsresult
ReadValue(nsIInputStream* aInputStream, T& aValue)
{
  nsresult rv;

  uint32_t readLength;
  if (ValueTraits<T>::IsFixedLength()) {
    readLength = ValueTraits<T>::Length(aValue);
  } else {
    // Read the variable value length from file.
    nsresult rv = ReadValue(aInputStream, readLength);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Sanity-check the readLength in case of disk corruption
  // (see bug 1433636).
  if (readLength > LookupCacheV4::MAX_METADATA_VALUE_LENGTH) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  // Read the value.
  uint32_t read;
  auto valueWritePtr = ValueTraits<T>::WritePtr(aValue, readLength);
  rv = aInputStream->Read(valueWritePtr, readLength, &read);
  if (NS_FAILED(rv) || read != readLength) {
    LOG(("Failed to read the value."));
    return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
  }

  return rv;
}

} // end of unnamed namespace.
////////////////////////////////////////////////////////////////////////

nsresult
LookupCacheV4::WriteMetadata(RefPtr<const TableUpdateV4> aTableUpdate)
{
  NS_ENSURE_ARG_POINTER(aTableUpdate);
  if (nsUrlClassifierDBService::ShutdownHasStarted()) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIFile> metaFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(metaFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metaFile->AppendNative(mTableName + METADATA_SUFFIX);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), metaFile,
                                   PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write the state.
  rv = WriteValue(outputStream, aTableUpdate->ClientState());
  NS_ENSURE_SUCCESS(rv, rv);

  // Write the checksum.
  rv = WriteValue(outputStream, aTableUpdate->Checksum());
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
LookupCacheV4::LoadMetadata(nsACString& aState, nsACString& aChecksum)
{
  nsCOMPtr<nsIFile> metaFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(metaFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metaFile->AppendNative(mTableName + METADATA_SUFFIX);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> localInFile;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(localInFile), metaFile,
                                  PR_RDONLY | nsIFile::OS_READAHEAD);
  if (NS_FAILED(rv)) {
    LOG(("Unable to open metadata file."));
    return rv;
  }

  // Read the list state.
  rv = ReadValue(localInFile, aState);
  if (NS_FAILED(rv)) {
    LOG(("Failed to read state."));
    return rv;
  }

  // Read the checksum.
  rv = ReadValue(localInFile, aChecksum);
  if (NS_FAILED(rv)) {
    LOG(("Failed to read checksum."));
    return rv;
  }

  return rv;
}

VLPrefixSet::VLPrefixSet(const PrefixStringMap& aMap)
  : mCount(0)
{
  for (auto iter = aMap.ConstIter(); !iter.Done(); iter.Next()) {
    uint32_t size = iter.Key();
    MOZ_ASSERT(iter.Data()->Length() % size == 0,
               "PrefixString must be a multiple of the prefix size.");
    mMap.Put(size, new PrefixString(*iter.Data(), size));
    mCount += iter.Data()->Length() / size;
  }
}

void
VLPrefixSet::Merge(PrefixStringMap& aPrefixMap) {
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    nsCString* prefixString = aPrefixMap.LookupOrAdd(iter.Key());
    PrefixString* str = iter.Data();

    nsAutoCString remainingString;
    str->getRemainingString(remainingString);
    if (!remainingString.IsEmpty()) {
      MOZ_ASSERT(remainingString.Length() == str->remaining());
      prefixString->Append(remainingString);
    }
  }
}

bool
VLPrefixSet::GetSmallestPrefix(nsACString& aOutString) const {
  PrefixString* pick = nullptr;
  for (auto iter = mMap.ConstIter(); !iter.Done(); iter.Next()) {
    PrefixString* str = iter.Data();

    if (str->remaining() <= 0) {
      continue;
    }

    if (aOutString.IsEmpty()) {
      str->getPrefix(aOutString);
      MOZ_ASSERT(aOutString.Length() == iter.Key());
      pick = str;
      continue;
    }

    nsAutoCString cur;
    str->getPrefix(cur);
    if (!cur.IsEmpty() && cur < aOutString) {
      aOutString.Assign(cur);
      MOZ_ASSERT(aOutString.Length() == iter.Key());
      pick = str;
    }
  }

  if (pick) {
    pick->next();
  }

  return pick != nullptr;
}

} // namespace safebrowsing
} // namespace mozilla
