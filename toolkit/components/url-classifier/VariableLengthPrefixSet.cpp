/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VariableLengthPrefixSet.h"
#include "nsIInputStream.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include <algorithm>

// MOZ_LOG=UrlClassifierPrefixSet:5
static mozilla::LazyLogModule gUrlClassifierPrefixSetLog(
    "UrlClassifierPrefixSet");
#define LOG(args) \
  MOZ_LOG(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug)

namespace mozilla::safebrowsing {

#define PREFIX_SIZE_FIXED 4

#ifdef DEBUG
namespace {

template <class T>
void EnsureSorted(T* aArray) {
  MOZ_ASSERT(std::is_sorted(aArray->Elements(),
                            aArray->Elements() + aArray->Length()));
}

}  // namespace
#endif

NS_IMPL_ISUPPORTS(VariableLengthPrefixSet, nsIMemoryReporter)

// This class will process prefix size between 4~32. But for 4 bytes prefixes,
// they will be passed to nsUrlClassifierPrefixSet because of better
// optimization.
VariableLengthPrefixSet::VariableLengthPrefixSet()
    : mLock("VariableLengthPrefixSet.mLock"),
      mFixedPrefixSet(new nsUrlClassifierPrefixSet) {}

nsresult VariableLengthPrefixSet::Init(const nsACString& aName) {
  mName = aName;
  mMemoryReportPath = nsPrintfCString(
      "explicit/storage/prefix-set/%s",
      (!aName.IsEmpty() ? PromiseFlatCString(aName).get() : "?!"));

  RegisterWeakMemoryReporter(this);

  return mFixedPrefixSet->Init(aName);
}

VariableLengthPrefixSet::~VariableLengthPrefixSet() {
  UnregisterWeakMemoryReporter(this);
}

nsresult VariableLengthPrefixSet::SetPrefixes(AddPrefixArray& aAddPrefixes,
                                              AddCompleteArray& aAddCompletes) {
  MutexAutoLock lock(mLock);

  // We may modify the prefix string in this function, clear this data
  // before returning to ensure no one use the data after this API.
  auto scopeExit = MakeScopeExit([&]() {
    aAddPrefixes.Clear();
    aAddCompletes.Clear();
  });

  // Clear old prefixSet before setting new one.
  mFixedPrefixSet->SetPrefixes(nullptr, 0);
  mVLPrefixSet.Clear();

  // Build fixed-length prefix set
  nsTArray<uint32_t> array;
  if (!array.SetCapacity(aAddPrefixes.Length(), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (size_t i = 0; i < aAddPrefixes.Length(); i++) {
    array.AppendElement(aAddPrefixes[i].PrefixHash().ToUint32());
  }

#ifdef DEBUG
  // PrefixSet requires sorted order
  EnsureSorted(&array);
#endif

  nsresult rv = mFixedPrefixSet->SetPrefixes(array.Elements(), array.Length());
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  uint32_t size;
  size = mFixedPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
  LOG(("SB tree done, size = %d bytes\n", size));
#endif

  CompletionArray completions;
  for (size_t i = 0; i < aAddCompletes.Length(); i++) {
    completions.AppendElement(aAddCompletes[i].CompleteHash());
  }
  completions.Sort();

  UniquePtr<nsCString> completionStr(new nsCString);
  completionStr->SetCapacity(completions.Length() * COMPLETE_SIZE);
  for (size_t i = 0; i < completions.Length(); i++) {
    const char* buf = reinterpret_cast<const char*>(completions[i].buf);
    completionStr->Append(buf, COMPLETE_SIZE);
  }
  mVLPrefixSet.Put(COMPLETE_SIZE, completionStr.release());

  return NS_OK;
}

nsresult VariableLengthPrefixSet::SetPrefixes(PrefixStringMap& aPrefixMap) {
  MutexAutoLock lock(mLock);

  // We may modify the prefix string in this function, clear this data
  // before returning to ensure no one use the data after this API.
  auto scopeExit = MakeScopeExit([&]() { aPrefixMap.Clear(); });

  // Prefix size should not less than 4-bytes or greater than 32-bytes
  for (auto iter = aPrefixMap.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Key() < PREFIX_SIZE_FIXED || iter.Key() > COMPLETE_SIZE) {
      return NS_ERROR_FAILURE;
    }
  }

  // Clear old prefixSet before setting new one.
  mFixedPrefixSet->SetPrefixes(nullptr, 0);
  mVLPrefixSet.Clear();

  // 4-bytes prefixes are handled by nsUrlClassifierPrefixSet.
  nsCString* prefixes = aPrefixMap.Get(PREFIX_SIZE_FIXED);
  if (prefixes) {
    NS_ENSURE_TRUE(prefixes->Length() % PREFIX_SIZE_FIXED == 0,
                   NS_ERROR_FAILURE);

    uint32_t numPrefixes = prefixes->Length() / PREFIX_SIZE_FIXED;

    // Prefixes are lexicographically-sorted, so the interger array
    // passed to nsUrlClassifierPrefixSet should also follow the same order.
    // Reverse byte order in-place in Little-Endian platform.
#if MOZ_LITTLE_ENDIAN()
    char* begin = prefixes->BeginWriting();
    char* end = prefixes->EndWriting();

    while (begin != end) {
      uint32_t* p = reinterpret_cast<uint32_t*>(begin);
      *p = BigEndian::readUint32(begin);
      begin += sizeof(uint32_t);
    }
#endif
    const uint32_t* arrayPtr =
        reinterpret_cast<const uint32_t*>(prefixes->BeginReading());

    nsresult rv = mFixedPrefixSet->SetPrefixes(arrayPtr, numPrefixes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
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

nsresult VariableLengthPrefixSet::GetPrefixes(PrefixStringMap& aPrefixMap) {
  MutexAutoLock lock(mLock);

  // 4-bytes prefixes are handled by nsUrlClassifierPrefixSet.
  FallibleTArray<uint32_t> array;
  nsresult rv = mFixedPrefixSet->GetPrefixesNative(array);
  NS_ENSURE_SUCCESS(rv, rv);

  size_t count = array.Length();
  if (count) {
    UniquePtr<nsCString> prefixes(new nsCString());
    if (!prefixes->SetLength(PREFIX_SIZE_FIXED * count, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Writing integer array to character array
    uint32_t* begin = reinterpret_cast<uint32_t*>(prefixes->BeginWriting());
    for (uint32_t i = 0; i < count; i++) {
      begin[i] = NativeEndian::swapToBigEndian(array[i]);
    }

    aPrefixMap.Put(PREFIX_SIZE_FIXED, prefixes.release());
  }

  // Copy variable-length prefix set
  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    aPrefixMap.Put(iter.Key(), new nsCString(*iter.Data()));
  }

  return NS_OK;
}

// This is used by V2 protocol which prefixes are either 4-bytes or 32-bytes.
nsresult VariableLengthPrefixSet::GetFixedLengthPrefixes(
    FallibleTArray<uint32_t>* aPrefixes,
    FallibleTArray<nsCString>* aCompletes) {
  MOZ_ASSERT(aPrefixes || aCompletes);
  MOZ_ASSERT_IF(aPrefixes, aPrefixes->IsEmpty());
  MOZ_ASSERT_IF(aCompletes, aCompletes->IsEmpty());

  if (aPrefixes) {
    nsresult rv = mFixedPrefixSet->GetPrefixesNative(*aPrefixes);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (aCompletes) {
    nsCString* completes = mVLPrefixSet.Get(COMPLETE_SIZE);
    if (completes) {
      uint32_t count = completes->Length() / COMPLETE_SIZE;
      if (!aCompletes->SetCapacity(count, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      for (uint32_t i = 0; i < count; i++) {
        // SetCapacity was just called, these cannot fail.
        (void)aCompletes->AppendElement(
            Substring(*completes, i * COMPLETE_SIZE, COMPLETE_SIZE), fallible);
      }
    }
  }

  return NS_OK;
}

// It should never be the case that more than one hash prefixes match a given
// full hash. However, if that happens, this method returns any one of them.
// It does not guarantee which one of those will be returned.
nsresult VariableLengthPrefixSet::Matches(uint32_t aPrefix,
                                          const nsACString& aFullHash,
                                          uint32_t* aLength) const {
  MutexAutoLock lock(mLock);

  // Only allow full-length hash to check if match any of the prefix
  MOZ_ASSERT(aFullHash.Length() == COMPLETE_SIZE);
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = 0;

  // Check if it matches 4-bytes prefixSet first
  bool found = false;
  nsresult rv = mFixedPrefixSet->Contains(aPrefix, &found);
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

nsresult VariableLengthPrefixSet::IsEmpty(bool* aEmpty) const {
  MutexAutoLock lock(mLock);

  NS_ENSURE_ARG_POINTER(aEmpty);

  mFixedPrefixSet->IsEmpty(aEmpty);
  *aEmpty = *aEmpty && mVLPrefixSet.IsEmpty();

  return NS_OK;
}

nsresult VariableLengthPrefixSet::LoadPrefixes(nsCOMPtr<nsIInputStream>& in) {
  MutexAutoLock lock(mLock);

  // First read prefixes from fixed-length prefix set
  nsresult rv = mFixedPrefixSet->LoadPrefixes(in);
  NS_ENSURE_SUCCESS(rv, rv);

  // Then read prefixes from variable-length prefix set
  uint32_t magic;
  uint32_t read;

  rv = in->Read(reinterpret_cast<char*>(&magic), sizeof(uint32_t), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  if (magic != PREFIXSET_VERSION_MAGIC) {
    LOG(("[%s] Version magic mismatch, not loading", mName.get()));
    return NS_ERROR_FILE_CORRUPTED;
  }

  mVLPrefixSet.Clear();

  uint32_t count;
  rv = in->Read(reinterpret_cast<char*>(&count), sizeof(uint32_t), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  uint32_t totalPrefixes = 0;
  for (; count > 0; count--) {
    uint8_t prefixSize;
    rv = in->Read(reinterpret_cast<char*>(&prefixSize), sizeof(uint8_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint8_t), NS_ERROR_FAILURE);

    if (prefixSize < PREFIX_SIZE || prefixSize > COMPLETE_SIZE) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    uint32_t stringLength;
    rv = in->Read(reinterpret_cast<char*>(&stringLength), sizeof(uint32_t),
                  &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

    NS_ENSURE_TRUE(stringLength % prefixSize == 0, NS_ERROR_FILE_CORRUPTED);
    uint32_t prefixCount = stringLength / prefixSize;

    UniquePtr<nsCString> vlPrefixes(new nsCString());
    if (!vlPrefixes->SetLength(stringLength, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = in->Read(reinterpret_cast<char*>(vlPrefixes->BeginWriting()),
                  stringLength, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == stringLength, NS_ERROR_FAILURE);

    mVLPrefixSet.Put(prefixSize, vlPrefixes.release());
    totalPrefixes += prefixCount;
    LOG(("[%s] Loaded %u %u-byte prefixes", mName.get(), prefixCount,
         prefixSize));
  }

  LOG(("[%s] Loading VLPrefixSet successful (%u total prefixes)", mName.get(),
       totalPrefixes));

  return NS_OK;
}

uint32_t VariableLengthPrefixSet::CalculatePreallocateSize() const {
  uint32_t fileSize = 0;

  // Size of fixed length prefix set.
  fileSize += mFixedPrefixSet->CalculatePreallocateSize();

  // Size of variable length prefix set.
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

nsresult VariableLengthPrefixSet::WritePrefixes(
    nsCOMPtr<nsIOutputStream>& out) const {
  MutexAutoLock lock(mLock);

  // First, write fixed length prefix set
  nsresult rv = mFixedPrefixSet->WritePrefixes(out);
  NS_ENSURE_SUCCESS(rv, rv);

  // Then, write variable length prefix set
  uint32_t written;
  uint32_t writelen = sizeof(uint32_t);
  uint32_t magic = PREFIXSET_VERSION_MAGIC;
  rv = out->Write(reinterpret_cast<char*>(&magic), writelen, &written);
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

    rv = out->Write(const_cast<char*>(vlPrefixes.BeginReading()), stringLength,
                    &written);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(stringLength == written, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

bool VariableLengthPrefixSet::BinarySearch(const nsACString& aFullHash,
                                           const nsACString& aPrefixes,
                                           uint32_t aPrefixSize) const {
  const char* fullhash = aFullHash.BeginReading();
  const char* prefixes = aPrefixes.BeginReading();
  int32_t begin = 0, end = aPrefixes.Length() / aPrefixSize;

  while (end > begin) {
    int32_t mid = (begin + end) >> 1;
    int cmp = memcmp(fullhash, prefixes + mid * aPrefixSize, aPrefixSize);
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
                                        nsISupports* aData, bool aAnonymize) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t amount = SizeOfIncludingThis(UrlClassifierMallocSizeOf);

  return aHandleReport->Callback(
      EmptyCString(), mMemoryReportPath, KIND_HEAP, UNITS_BYTES, amount,
      nsLiteralCString("Memory used by the variable-length prefix set for a "
                       "URL classifier."),
      aData);
}

size_t VariableLengthPrefixSet::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  MutexAutoLock lock(mLock);

  size_t n = 0;
  n += aMallocSizeOf(this);

  n += mFixedPrefixSet->SizeOfIncludingThis(moz_malloc_size_of) -
       aMallocSizeOf(mFixedPrefixSet);

  n += mVLPrefixSet.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mVLPrefixSet.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Data()->SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

}  // namespace mozilla::safebrowsing
