//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCacheV4.h"
#include "HashStore.h"
#include "mozilla/Unused.h"
#include "nsCheckSummedOutputStream.h"
#include "crc32c.h"
#include <string>

// MOZ_LOG=UrlClassifierDbService:5
extern mozilla::LazyLogModule gUrlClassifierDbServiceLog;
#define LOG(args) \
  MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

#define METADATA_SUFFIX NS_LITERAL_CSTRING(".metadata")
namespace {

static const uint64_t STREAM_BUFFER_SIZE = 4096;

//////////////////////////////////////////////////////////////////////////
// A set of lightweight functions for reading/writing value from/to file.
template <typename T>
struct ValueTraits {
  static_assert(sizeof(T) <= LookupCacheV4::MAX_METADATA_VALUE_LENGTH,
                "LookupCacheV4::MAX_METADATA_VALUE_LENGTH is too small.");
  static uint32_t Length(const T& aValue) { return sizeof(T); }
  static char* WritePtr(T& aValue, uint32_t aLength) { return (char*)&aValue; }
  static const char* ReadPtr(const T& aValue) { return (char*)&aValue; }
  static bool IsFixedLength() { return true; }
};

template <>
struct ValueTraits<nsACString> {
  static bool IsFixedLength() { return false; }

  static uint32_t Length(const nsACString& aValue) { return aValue.Length(); }

  static char* WritePtr(nsACString& aValue, uint32_t aLength) {
    aValue.SetLength(aLength);
    return aValue.BeginWriting();
  }

  static const char* ReadPtr(const nsACString& aValue) {
    return aValue.BeginReading();
  }
};

template <typename T>
static nsresult WriteValue(nsIOutputStream* aOutputStream, const T& aValue) {
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

template <typename T>
static nsresult ReadValue(nsIInputStream* aInputStream, T& aValue) {
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

}  // end of unnamed namespace.
////////////////////////////////////////////////////////////////////////

namespace mozilla {
namespace safebrowsing {

const int LookupCacheV4::VER = 4;
const uint32_t LookupCacheV4::MAX_METADATA_VALUE_LENGTH = 256;

const uint32_t VLPSET_MAGIC = 0x36044a35;
const uint32_t VLPSET_VERSION = 1;

// Prefixes coming from updates and VLPrefixSet are both stored in the HashTable
// where the (key, value) pair is a prefix size and a lexicographic-sorted
// string. The difference is prefixes from updates use std:string(to avoid
// additional copies) and prefixes from VLPrefixSet use nsCString. This class
// provides a common interface for the partial update algorithm to make it
// easier to operate on two different kind prefix string map..
class VLPrefixSet {
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
        : data(aStr), pos(0), size(aSize) {
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

nsresult LookupCacheV4::Init() {
  mVLPrefixSet = new VariableLengthPrefixSet();
  nsresult rv = mVLPrefixSet->Init(mTableName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult LookupCacheV4::Has(const Completion& aCompletion, bool* aHas,
                            uint32_t* aMatchLength, bool* aConfirmed) {
  *aHas = *aConfirmed = false;
  *aMatchLength = 0;

  uint32_t length = 0;
  nsDependentCSubstring fullhash;
  fullhash.Rebind((const char*)aCompletion.buf, COMPLETE_SIZE);

  nsresult rv = mVLPrefixSet->Matches(fullhash, &length);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(length == 0 || (length >= PREFIX_SIZE && length <= COMPLETE_SIZE));

  *aHas = length >= PREFIX_SIZE;
  *aMatchLength = length;

  // Check if fullhash match any prefix in the local database
  if (!(*aHas)) {
    return NS_OK;
  }

  // Even though V4 supports variable-length prefix, we always send 4-bytes for
  // completion (Bug 1323953). This means cached prefix length is also 4-bytes.
  return CheckCache(aCompletion, aHas, aConfirmed);
}

bool LookupCacheV4::IsEmpty() const {
  bool isEmpty;
  mVLPrefixSet->IsEmpty(&isEmpty);
  return isEmpty;
}

nsresult LookupCacheV4::Build(PrefixStringMap& aPrefixMap) {
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_VLPS_CONSTRUCT_TIME> timer;

  nsresult rv = mVLPrefixSet->SetPrefixes(aPrefixMap);
  NS_ENSURE_SUCCESS(rv, rv);
  mPrimed = true;

  return rv;
}

nsresult LookupCacheV4::GetPrefixes(PrefixStringMap& aPrefixMap) {
  if (!mPrimed) {
    // This can happen if its a new table, so no error.
    LOG(("GetPrefixes from empty LookupCache"));
    return NS_OK;
  }
  return mVLPrefixSet->GetPrefixes(aPrefixMap);
}

nsresult LookupCacheV4::GetFixedLengthPrefixes(
    FallibleTArray<uint32_t>& aPrefixes) {
  return mVLPrefixSet->GetFixedLengthPrefixes(aPrefixes);
}

nsresult LookupCacheV4::ClearPrefixes() {
  // Clear by seting a empty map
  PrefixStringMap map;
  return mVLPrefixSet->SetPrefixes(map);
}

nsresult LookupCacheV4::StoreToFile(nsCOMPtr<nsIFile>& aFile) {
  NS_ENSURE_ARG_POINTER(aFile);

  uint32_t fileSize = sizeof(Header) +
                      mVLPrefixSet->CalculatePreallocateSize() +
                      nsCrc32CheckSumedOutputStream::CHECKSUM_SIZE;

  nsCOMPtr<nsIOutputStream> localOutFile;
  nsresult rv =
      NS_NewSafeLocalFileOutputStream(getter_AddRefs(localOutFile), aFile,
                                      PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Preallocate the file storage
  {
    nsCOMPtr<nsIFileOutputStream> fos(do_QueryInterface(localOutFile));
    Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_VLPS_FALLOCATE_TIME> timer;

    Unused << fos->Preallocate(fileSize);
  }

  nsCOMPtr<nsIOutputStream> out;
  rv = NS_NewCrc32OutputStream(getter_AddRefs(out), localOutFile.forget(),
                               std::min(fileSize, MAX_BUFFER_SIZE));

  // Write header
  Header header = {.magic = VLPSET_MAGIC, .version = VLPSET_VERSION};
  rv = WriteValue(out, header);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Write prefixes
  rv = mVLPrefixSet->WritePrefixes(out);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Write checksum
  nsCOMPtr<nsISafeOutputStream> safeOut = do_QueryInterface(out, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = safeOut->Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LOG(("[%s] Storing PrefixSet successful", mTableName.get()));

  // This is to remove old ".pset" files if exist
  Unused << CleanOldPrefixSet();
  return NS_OK;
}

nsresult LookupCacheV4::CleanOldPrefixSet() {
  nsCOMPtr<nsIFile> file;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->AppendNative(mTableName + NS_LITERAL_CSTRING(".pset"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = file->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    LOG(("[%s] Old PrefixSet is succuessfully removed!", mTableName.get()));
  }

  return NS_OK;
}

nsresult LookupCacheV4::LoadLegacyFile() {
  nsCOMPtr<nsIFile> file;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->AppendNative(mTableName + NS_LITERAL_CSTRING(".pset"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> localInFile;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(localInFile), file,
                                  PR_RDONLY | nsIFile::OS_READAHEAD);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Calculate how big the file is, make sure our read buffer isn't bigger
  // than the file itself which is just wasting memory.
  int64_t fileSize;
  rv = file->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (fileSize < 0 || fileSize > UINT32_MAX) {
    return NS_ERROR_FAILURE;
  }

  uint32_t bufferSize =
      std::min<uint32_t>(static_cast<uint32_t>(fileSize), MAX_BUFFER_SIZE);

  // Convert to buffered stream
  nsCOMPtr<nsIInputStream> in;
  rv = NS_NewBufferedInputStream(getter_AddRefs(in), localInFile.forget(),
                                 bufferSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Load data
  rv = mVLPrefixSet->LoadPrefixes(in);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPrimed = true;

  LOG(("[%s] Loading Legacy PrefixSet successful", mTableName.get()));
  return NS_OK;
}

nsresult LookupCacheV4::LoadFromFile(nsCOMPtr<nsIFile>& aFile) {
  NS_ENSURE_ARG_POINTER(aFile);

  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_VLPS_FILELOAD_TIME> timer;

  nsCOMPtr<nsIInputStream> localInFile;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(localInFile), aFile,
                                           PR_RDONLY | nsIFile::OS_READAHEAD);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Calculate how big the file is, make sure our read buffer isn't bigger
  // than the file itself which is just wasting memory.
  int64_t fileSize;
  rv = aFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (fileSize < 0 || fileSize > UINT32_MAX) {
    return NS_ERROR_FAILURE;
  }

  uint32_t bufferSize =
      std::min<uint32_t>(static_cast<uint32_t>(fileSize), MAX_BUFFER_SIZE);

  // Convert to buffered stream
  nsCOMPtr<nsIInputStream> in;
  rv = NS_NewBufferedInputStream(getter_AddRefs(in), localInFile.forget(),
                                 bufferSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Load header
  Header header;
  rv = ReadValue(in, header);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SanityCheck(header);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Load data
  rv = mVLPrefixSet->LoadPrefixes(in);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Load crc32 checksum and verify
  rv = VerifyCRC32(in);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPrimed = true;

  LOG(("[%s] Loading PrefixSet successful", mTableName.get()));
  return NS_OK;
}

nsresult LookupCacheV4::SanityCheck(const Header& aHeader) {
  if (aHeader.magic != VLPSET_MAGIC) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  if (aHeader.version != VLPSET_VERSION) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

size_t LookupCacheV4::SizeOfPrefixSet() const {
  return mVLPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
}

nsCString LookupCacheV4::GetPrefixSetSuffix() const {
  return NS_LITERAL_CSTRING(".vlpset");
}

static nsresult AppendPrefixToMap(PrefixStringMap& prefixes,
                                  const nsACString& prefix) {
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

static nsresult InitCrypto(nsCOMPtr<nsICryptoHash>& aCrypto) {
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
// keeps track of the sha256 hash
static void UpdateSHA256(nsICryptoHash* aCrypto, const nsACString& aPrefix) {
  MOZ_ASSERT(aCrypto);
  aCrypto->Update(
      reinterpret_cast<uint8_t*>(const_cast<char*>(aPrefix.BeginReading())),
      aPrefix.Length());
}

// Please see https://bug1287058.bmoattachments.org/attachment.cgi?id=8795366
// for detail about partial update algorithm.
nsresult LookupCacheV4::ApplyUpdate(RefPtr<TableUpdateV4> aTableUpdate,
                                    PrefixStringMap& aInputMap,
                                    PrefixStringMap& aOutputMap) {
  MOZ_ASSERT(aOutputMap.IsEmpty());

  nsCOMPtr<nsICryptoHash> crypto;
  nsresult rv = InitCrypto(crypto);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // oldPSet contains prefixes we already have or we just merged last round.
  // addPSet contains prefixes stored in tableUpdate which should be merged with
  // oldPSet.
  VLPrefixSet oldPSet(aInputMap);
  VLPrefixSet addPSet(aTableUpdate->Prefixes());

  // RemovalIndiceArray is a sorted integer array indicating the index of prefix
  // we should remove from the old prefix set(according to lexigraphic order).
  // |removalIndex| is the current index of RemovalIndiceArray.
  // |numOldPrefixPicked| is used to record how many prefixes we picked from the
  // old map.
  const TableUpdateV4::RemovalIndiceArray& removalArray =
      aTableUpdate->RemovalIndices();
  uint32_t removalIndex = 0;
  int32_t numOldPrefixPicked = -1;

  nsAutoCString smallestOldPrefix;
  nsAutoCString smallestAddPrefix;

  bool isOldMapEmpty = false, isAddMapEmpty = false;

  // This is used to avoid infinite loop for partial update algorithm.
  // The maximum loops will be the number of old prefixes plus the number of add
  // prefixes.
  int32_t index = oldPSet.Count() + addPSet.Count() + 1;
  for (; index > 0; index--) {
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

      // If the number of picks from old map matches the removalIndex, then this
      // prefix will be removed by not merging it to new map.
      if (removalIndex < removalArray.Length() &&
          numOldPrefixPicked == removalArray[removalIndex]) {
        removalIndex++;
      } else {
        rv = AppendPrefixToMap(aOutputMap, smallestOldPrefix);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        UpdateSHA256(crypto, smallestOldPrefix);
      }
      smallestOldPrefix.SetLength(0);
    } else {
      rv = AppendPrefixToMap(aOutputMap, smallestAddPrefix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      UpdateSHA256(crypto, smallestAddPrefix);
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
    LOG(
        ("There are still prefixes to remove after exhausting the old "
         "PrefixSet."));
    return NS_ERROR_UC_UPDATE_WRONG_REMOVAL_INDICES;
  }

  // Prefixes and removal indice from update is no longer required
  // after merging the data with local prefixes.
  aTableUpdate->Clear();

  nsAutoCString sha256;
  crypto->Finish(false, sha256);
  if (aTableUpdate->SHA256().IsEmpty()) {
    LOG(("Update sha256 hash missing."));
    Telemetry::Accumulate(
        Telemetry::URLCLASSIFIER_UPDATE_ERROR, mProvider,
        NS_ERROR_GET_CODE(NS_ERROR_UC_UPDATE_MISSING_CHECKSUM));

    // Generate our own sha256 to tableUpdate to ensure there is always
    // checksum in .metadata
    std::string stdSha256(sha256.BeginReading(), sha256.Length());
    aTableUpdate->SetSHA256(stdSha256);
  } else if (aTableUpdate->SHA256() != sha256) {
    LOG(("SHA256 hash mismatch after applying partial update"));
    return NS_ERROR_UC_UPDATE_CHECKSUM_MISMATCH;
  }

  return NS_OK;
}

nsresult LookupCacheV4::AddFullHashResponseToCache(
    const FullHashResponseMap& aResponseMap) {
  CopyClassHashTable<FullHashResponseMap>(aResponseMap, mFullHashCache);

  return NS_OK;
}

// This function assumes CRC32 checksum is in the end of the input stream
nsresult LookupCacheV4::VerifyCRC32(nsCOMPtr<nsIInputStream>& aIn) {
  nsCOMPtr<nsISeekableStream> seekIn = do_QueryInterface(aIn);
  nsresult rv = seekIn->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t len;
  rv = aIn->Available(&len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t calculateCrc32 = ~0;

  // We don't want to include the checksum itself
  len = len - nsCrc32CheckSumedOutputStream::CHECKSUM_SIZE;

  char buffer[STREAM_BUFFER_SIZE];
  while (len) {
    uint32_t read;
    uint64_t readLimit = std::min<uint64_t>(STREAM_BUFFER_SIZE, len);

    rv = aIn->Read(buffer, readLimit, &read);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    calculateCrc32 = ComputeCrc32c(
        calculateCrc32, reinterpret_cast<const uint8_t*>(buffer), read);

    len -= read;
  }

  // Now read the CRC32
  uint32_t crc32;
  ReadValue(aIn, crc32);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (crc32 != calculateCrc32) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

nsresult LookupCacheV4::WriteMetadata(
    RefPtr<const TableUpdateV4> aTableUpdate) {
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

  // Write the SHA256 hash.
  rv = WriteValue(outputStream, aTableUpdate->SHA256());
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult LookupCacheV4::LoadMetadata(nsACString& aState, nsACString& aSHA256) {
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

  // Read the SHA256 hash.
  rv = ReadValue(localInFile, aSHA256);
  if (NS_FAILED(rv)) {
    LOG(("Failed to read SHA256 hash."));
    return rv;
  }

  return rv;
}

VLPrefixSet::VLPrefixSet(const PrefixStringMap& aMap) : mCount(0) {
  for (auto iter = aMap.ConstIter(); !iter.Done(); iter.Next()) {
    uint32_t size = iter.Key();
    MOZ_ASSERT(iter.Data()->Length() % size == 0,
               "PrefixString must be a multiple of the prefix size.");
    mMap.Put(size, new PrefixString(*iter.Data(), size));
    mCount += iter.Data()->Length() / size;
  }
}

void VLPrefixSet::Merge(PrefixStringMap& aPrefixMap) {
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

bool VLPrefixSet::GetSmallestPrefix(nsACString& aOutString) const {
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

}  // namespace safebrowsing
}  // namespace mozilla
