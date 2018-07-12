/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VariableLengthPrefixSet.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include <algorithm>

// MOZ_LOG=UrlClassifierPrefixSet:5
static mozilla::LazyLogModule gUrlClassifierPrefixSetLog("UrlClassifierPrefixSet");
#define LOG(args) MOZ_LOG(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug)

namespace mozilla {
namespace safebrowsing {

#define PREFIX_SIZE_FIXED 4

NS_IMPL_ISUPPORTS(VariableLengthPrefixSet, nsIMemoryReporter)

// Definition required due to std::max<>()
const uint32_t VariableLengthPrefixSet::MAX_BUFFER_SIZE;

// This class will process prefix size between 4~32. But for 4 bytes prefixes,
// they will be passed to nsUrlClassifierPrefixSet because of better optimization.
VariableLengthPrefixSet::VariableLengthPrefixSet()
  : mLock("VariableLengthPrefixSet.mLock")
  , mFixedPrefixSet(new nsUrlClassifierPrefixSet)
{
}

nsresult
VariableLengthPrefixSet::Init(const nsACString& aName)
{
  mMemoryReportPath =
    nsPrintfCString(
      "explicit/storage/prefix-set/%s",
      (!aName.IsEmpty() ? PromiseFlatCString(aName).get() : "?!")
    );

  RegisterWeakMemoryReporter(this);

  return mFixedPrefixSet->Init(aName);
}

VariableLengthPrefixSet::~VariableLengthPrefixSet()
{
  UnregisterWeakMemoryReporter(this);
}

nsresult
VariableLengthPrefixSet::SetPrefixes(const PrefixStringMap& aPrefixMap)
{
  MutexAutoLock lock(mLock);

  // Prefix size should not less than 4-bytes or greater than 32-bytes
  for (auto iter = aPrefixMap.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Key() < PREFIX_SIZE_FIXED ||
        iter.Key() > COMPLETE_SIZE) {
      return NS_ERROR_FAILURE;
    }
  }

  // Clear old prefixSet before setting new one.
  mFixedPrefixSet->SetPrefixes(nullptr, 0);
  mVLPrefixSet.Clear();

  // 4-bytes prefixes are handled by nsUrlClassifierPrefixSet.
  nsCString* prefixes = aPrefixMap.Get(PREFIX_SIZE_FIXED);
  if (prefixes) {
    NS_ENSURE_TRUE(prefixes->Length() % PREFIX_SIZE_FIXED == 0, NS_ERROR_FAILURE);

    uint32_t numPrefixes = prefixes->Length() / PREFIX_SIZE_FIXED;

#if MOZ_BIG_ENDIAN
    const uint32_t* arrayPtr = reinterpret_cast<const uint32_t*>(prefixes->BeginReading());
#else
    FallibleTArray<uint32_t> array;
    // Prefixes are lexicographically-sorted, so the interger array
    // passed to nsUrlClassifierPrefixSet should also follow the same order.
    // To make sure of that, we convert char array to integer with Big-Endian
    // instead of casting to integer directly.
    if (!array.SetCapacity(numPrefixes, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const char* begin = prefixes->BeginReading();
    const char* end = prefixes->EndReading();

    while (begin != end) {
      array.AppendElement(BigEndian::readUint32(begin), fallible);
      begin += sizeof(uint32_t);
    }

    const uint32_t* arrayPtr = array.Elements();
#endif

    nsresult rv = mFixedPrefixSet->SetPrefixes(arrayPtr, numPrefixes);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // 5~32 bytes prefixes are stored in mVLPrefixSet.
  for (auto iter = aPrefixMap.ConstIter(); !iter.Done(); iter.Next()) {
    // Skip 4bytes prefixes because it is already stored in mFixedPrefixSet.
    if (iter.Key() == PREFIX_SIZE_FIXED) {
      continue;
    }

    mVLPrefixSet.Put(iter.Key(), new nsCString(*iter.Data()));
  }

  return NS_OK;
}

nsresult
VariableLengthPrefixSet::GetPrefixes(PrefixStringMap& aPrefixMap)
{
  MutexAutoLock lock(mLock);

  // 4-bytes prefixes are handled by nsUrlClassifierPrefixSet.
  FallibleTArray<uint32_t> array;
  nsresult rv = mFixedPrefixSet->GetPrefixesNative(array);
  NS_ENSURE_SUCCESS(rv, rv);

  size_t count = array.Length();
  if (count) {
    nsCString* prefixes = new nsCString();
    if (!prefixes->SetLength(PREFIX_SIZE_FIXED * count, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Writing integer array to character array
    uint32_t* begin = reinterpret_cast<uint32_t*>(prefixes->BeginWriting());
    for (uint32_t i = 0; i < count; i++) {
      begin[i] = NativeEndian::swapToBigEndian(array[i]);
    }

    aPrefixMap.Put(PREFIX_SIZE_FIXED, prefixes);
  }

  // Copy variable-length prefix set
  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    aPrefixMap.Put(iter.Key(), new nsCString(*iter.Data()));
  }

  return NS_OK;
}

nsresult
VariableLengthPrefixSet::GetFixedLengthPrefixes(FallibleTArray<uint32_t>& aPrefixes)
{
  return mFixedPrefixSet->GetPrefixesNative(aPrefixes);
}

// It should never be the case that more than one hash prefixes match a given
// full hash. However, if that happens, this method returns any one of them.
// It does not guarantee which one of those will be returned.
nsresult
VariableLengthPrefixSet::Matches(const nsACString& aFullHash,
                                 uint32_t* aLength) const
{
  MutexAutoLock lock(mLock);

  // Only allow full-length hash to check if match any of the prefix
  MOZ_ASSERT(aFullHash.Length() == COMPLETE_SIZE);
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = 0;

  // Check if it matches 4-bytes prefixSet first
  const uint32_t* hash = reinterpret_cast<const uint32_t*>(aFullHash.BeginReading());
  uint32_t value = BigEndian::readUint32(hash);

  bool found = false;
  nsresult rv = mFixedPrefixSet->Contains(value, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  if (found) {
    *aLength = PREFIX_SIZE_FIXED;
    return NS_OK;
  }

  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    if (BinarySearch(aFullHash, *iter.Data(), iter.Key())) {
      *aLength = iter.Key();
      MOZ_ASSERT(*aLength > 4);
      return NS_OK;
    }
  }

  return NS_OK;
}

nsresult
VariableLengthPrefixSet::IsEmpty(bool* aEmpty) const
{
  MutexAutoLock lock(mLock);

  NS_ENSURE_ARG_POINTER(aEmpty);

  mFixedPrefixSet->IsEmpty(aEmpty);
  *aEmpty = *aEmpty && mVLPrefixSet.IsEmpty();

  return NS_OK;
}

nsresult
VariableLengthPrefixSet::LoadFromFile(nsCOMPtr<nsIFile>& aFile)
{
  MutexAutoLock lock(mLock);

  NS_ENSURE_ARG_POINTER(aFile);

  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_VLPS_FILELOAD_TIME> timer;

  nsCOMPtr<nsIInputStream> localInFile;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(localInFile), aFile,
                                           PR_RDONLY | nsIFile::OS_READAHEAD);
  NS_ENSURE_SUCCESS(rv, rv);

  // Calculate how big the file is, make sure our read buffer isn't bigger
  // than the file itself which is just wasting memory.
  int64_t fileSize;
  rv = aFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (fileSize < 0 || fileSize > UINT32_MAX) {
    return NS_ERROR_FAILURE;
  }

  uint32_t bufferSize = std::min<uint32_t>(static_cast<uint32_t>(fileSize),
                                           MAX_BUFFER_SIZE);

  // Convert to buffered stream
  nsCOMPtr<nsIInputStream> in;
  rv = NS_NewBufferedInputStream(getter_AddRefs(in), localInFile.forget(),
                                 bufferSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mFixedPrefixSet->LoadPrefixes(in);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = LoadPrefixes(in);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;;
}

nsresult
VariableLengthPrefixSet::StoreToFile(nsCOMPtr<nsIFile>& aFile) const
{
  NS_ENSURE_ARG_POINTER(aFile);

  MutexAutoLock lock(mLock);

  nsCOMPtr<nsIOutputStream> localOutFile;
  nsresult rv = NS_NewLocalFileOutputStream(getter_AddRefs(localOutFile), aFile,
                                            PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t fileSize = 0;
  // Preallocate the file storage
  {
    nsCOMPtr<nsIFileOutputStream> fos(do_QueryInterface(localOutFile));
    Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_VLPS_FALLOCATE_TIME> timer;

    fileSize += mFixedPrefixSet->CalculatePreallocateSize();
    fileSize += CalculatePreallocateSize();

    Unused << fos->Preallocate(fileSize);
  }

  // Convert to buffered stream
  nsCOMPtr<nsIOutputStream> out;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(out), localOutFile.forget(),
                                  std::min(fileSize, MAX_BUFFER_SIZE));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mFixedPrefixSet->WritePrefixes(out);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WritePrefixes(out);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
VariableLengthPrefixSet::LoadPrefixes(nsCOMPtr<nsIInputStream>& in)
{
  uint32_t magic;
  uint32_t read;

  nsresult rv = in->Read(reinterpret_cast<char*>(&magic), sizeof(uint32_t), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  if (magic != PREFIXSET_VERSION_MAGIC) {
    LOG(("Version magic mismatch, not loading"));
    return NS_ERROR_FILE_CORRUPTED;
  }

  mVLPrefixSet.Clear();

  uint32_t count;
  rv = in->Read(reinterpret_cast<char*>(&count), sizeof(uint32_t), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  for(;count > 0; count--) {
    uint8_t prefixSize;
    rv = in->Read(reinterpret_cast<char*>(&prefixSize), sizeof(uint8_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint8_t), NS_ERROR_FAILURE);

    if (prefixSize < PREFIX_SIZE || prefixSize > COMPLETE_SIZE) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    uint32_t stringLength;
    rv = in->Read(reinterpret_cast<char*>(&stringLength), sizeof(uint32_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

    nsCString* vlPrefixes = new nsCString();
    if (!vlPrefixes->SetLength(stringLength, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = in->Read(reinterpret_cast<char*>(vlPrefixes->BeginWriting()), stringLength, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == stringLength, NS_ERROR_FAILURE);

    mVLPrefixSet.Put(prefixSize, vlPrefixes);
  }

  return NS_OK;
}

uint32_t
VariableLengthPrefixSet::CalculatePreallocateSize() const
{
  uint32_t fileSize = 0;

  // Store how many prefix string.
  fileSize += sizeof(uint32_t);

  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    // Store prefix size, prefix string length, and prefix string.
    fileSize += sizeof(uint8_t);
    fileSize += sizeof(uint32_t);
    fileSize += iter.Data()->Length();
  }
  return fileSize;
}

nsresult
VariableLengthPrefixSet::WritePrefixes(nsCOMPtr<nsIOutputStream>& out) const
{
  uint32_t written;
  uint32_t writelen = sizeof(uint32_t);
  uint32_t magic = PREFIXSET_VERSION_MAGIC;
  nsresult rv = out->Write(reinterpret_cast<char*>(&magic), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  uint32_t count = mVLPrefixSet.Count();
  rv = out->Write(reinterpret_cast<char*>(&count), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  // Store PrefixSize, Length of Prefix String and then Prefix String
  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    const nsCString& vlPrefixes = *iter.Data();

    uint8_t prefixSize = iter.Key();
    writelen = sizeof(uint8_t);
    rv = out->Write(reinterpret_cast<char*>(&prefixSize), writelen, &written);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

    uint32_t stringLength = vlPrefixes.Length();
    writelen = sizeof(uint32_t);
    rv = out->Write(reinterpret_cast<char*>(&stringLength), writelen, &written);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

    rv = out->Write(const_cast<char*>(vlPrefixes.BeginReading()),
                    stringLength, &written);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(stringLength == written, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

bool
VariableLengthPrefixSet::BinarySearch(const nsACString& aFullHash,
                                      const nsACString& aPrefixes,
                                      uint32_t aPrefixSize) const
{
  const char* fullhash = aFullHash.BeginReading();
  const char* prefixes = aPrefixes.BeginReading();
  int32_t begin = 0, end = aPrefixes.Length() / aPrefixSize;

  while (end > begin) {
    int32_t mid = (begin + end) >> 1;
    int cmp = memcmp(fullhash, prefixes + mid*aPrefixSize, aPrefixSize);
    if (cmp < 0) {
      end = mid;
    } else if (cmp > 0) {
      begin = mid + 1;
    } else {
      return true;
    }
  }
  return false;
}

MOZ_DEFINE_MALLOC_SIZE_OF(UrlClassifierMallocSizeOf)

NS_IMETHODIMP
VariableLengthPrefixSet::CollectReports(nsIHandleReportCallback* aHandleReport,
                                        nsISupports* aData, bool aAnonymize)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t amount = SizeOfIncludingThis(UrlClassifierMallocSizeOf);

  return aHandleReport->Callback(
    EmptyCString(), mMemoryReportPath, KIND_HEAP, UNITS_BYTES, amount,
    NS_LITERAL_CSTRING("Memory used by the variable-length prefix set for a URL classifier."),
    aData);
}

size_t
VariableLengthPrefixSet::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  MutexAutoLock lock(mLock);

  size_t n = 0;
  n += aMallocSizeOf(this);
  n += mFixedPrefixSet->SizeOfIncludingThis(moz_malloc_size_of) - aMallocSizeOf(mFixedPrefixSet);

  n += mVLPrefixSet.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Data()->SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

} // namespace safebrowsing
} // namespace mozilla
