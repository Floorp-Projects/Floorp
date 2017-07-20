/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsToolkitCompsCID.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsISeekableStream.h"
#include "nsIBufferedStreams.h"
#include "nsIFileStreams.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Telemetry.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include <algorithm>

using namespace mozilla;

// MOZ_LOG=UrlClassifierPrefixSet:5
static LazyLogModule gUrlClassifierPrefixSetLog("UrlClassifierPrefixSet");
#define LOG(args) MOZ_LOG(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(
  nsUrlClassifierPrefixSet, nsIUrlClassifierPrefixSet, nsIMemoryReporter)

// Definition required due to std::max<>()
const uint32_t nsUrlClassifierPrefixSet::MAX_BUFFER_SIZE;

nsUrlClassifierPrefixSet::nsUrlClassifierPrefixSet()
  : mLock("nsUrlClassifierPrefixSet.mLock")
  , mTotalPrefixes(0)
  , mMemoryReportPath()
{
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::Init(const nsACString& aName)
{
  mMemoryReportPath =
    nsPrintfCString(
      "explicit/storage/prefix-set/%s",
      (!aName.IsEmpty() ? PromiseFlatCString(aName).get() : "?!")
    );

  RegisterWeakMemoryReporter(this);

  return NS_OK;
}

nsUrlClassifierPrefixSet::~nsUrlClassifierPrefixSet()
{
  UnregisterWeakMemoryReporter(this);
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::SetPrefixes(const uint32_t* aArray, uint32_t aLength)
{
  MutexAutoLock lock(mLock);

  nsresult rv = NS_OK;

  if (aLength <= 0) {
    if (mIndexPrefixes.Length() > 0) {
      LOG(("Clearing PrefixSet"));
      mIndexDeltas.Clear();
      mIndexPrefixes.Clear();
      mTotalPrefixes = 0;
    }
  } else {
    rv = MakePrefixSet(aArray, aLength);
  }

  return rv;
}

nsresult
nsUrlClassifierPrefixSet::MakePrefixSet(const uint32_t* aPrefixes, uint32_t aLength)
{
  mLock.AssertCurrentThreadOwns();

  if (aLength == 0) {
    return NS_OK;
  }

#ifdef DEBUG
  for (uint32_t i = 1; i < aLength; i++) {
    MOZ_ASSERT(aPrefixes[i] >= aPrefixes[i-1]);
  }
#endif

  mIndexPrefixes.Clear();
  mIndexDeltas.Clear();
  mTotalPrefixes = aLength;

  mIndexPrefixes.AppendElement(aPrefixes[0]);
  mIndexDeltas.AppendElement();

  uint32_t numOfDeltas = 0;
  uint32_t totalDeltas = 0;
  uint32_t previousItem = aPrefixes[0];
  for (uint32_t i = 1; i < aLength; i++) {
    if ((numOfDeltas >= DELTAS_LIMIT) ||
          (aPrefixes[i] - previousItem >= MAX_INDEX_DIFF)) {
      // Compact the previous element.
      // Note there is always at least one element when we get here,
      // because we created the first element before the loop.
      mIndexDeltas.LastElement().Compact();
      if (!mIndexDeltas.AppendElement(fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (!mIndexPrefixes.AppendElement(aPrefixes[i], fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      numOfDeltas = 0;
    } else {
      uint16_t delta = aPrefixes[i] - previousItem;
      if (!mIndexDeltas.LastElement().AppendElement(delta, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      numOfDeltas++;
      totalDeltas++;
    }
    previousItem = aPrefixes[i];
  }

  mIndexDeltas.LastElement().Compact();
  mIndexDeltas.Compact();
  mIndexPrefixes.Compact();

  LOG(("Total number of indices: %d", aLength));
  LOG(("Total number of deltas: %d", totalDeltas));
  LOG(("Total number of delta chunks: %" PRIuSIZE, mIndexDeltas.Length()));

  return NS_OK;
}

nsresult
nsUrlClassifierPrefixSet::GetPrefixesNative(FallibleTArray<uint32_t>& outArray)
{
  MutexAutoLock lock(mLock);

  if (!outArray.SetLength(mTotalPrefixes, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t prefixIdxLength = mIndexPrefixes.Length();
  uint32_t prefixCnt = 0;

  for (uint32_t i = 0; i < prefixIdxLength; i++) {
    uint32_t prefix = mIndexPrefixes[i];

    if (prefixCnt >= mTotalPrefixes) {
      return NS_ERROR_FAILURE;
    }
    outArray[prefixCnt++] = prefix;

    for (uint32_t j = 0; j < mIndexDeltas[i].Length(); j++) {
      prefix += mIndexDeltas[i][j];
      if (prefixCnt >= mTotalPrefixes) {
        return NS_ERROR_FAILURE;
      }
      outArray[prefixCnt++] = prefix;
    }
  }

  NS_ASSERTION(mTotalPrefixes == prefixCnt, "Lengths are inconsistent");
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::GetPrefixes(uint32_t* aCount,
                                      uint32_t** aPrefixes)
{
  // No need to get mLock here because this function does not directly touch
  // the class's data members. (GetPrefixesNative() will get mLock, however.)

  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  NS_ENSURE_ARG_POINTER(aPrefixes);
  *aPrefixes = nullptr;

  FallibleTArray<uint32_t> prefixes;
  nsresult rv = GetPrefixesNative(prefixes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint64_t itemCount = prefixes.Length();
  uint32_t* prefixArray = static_cast<uint32_t*>(moz_xmalloc(itemCount * sizeof(uint32_t)));
  NS_ENSURE_TRUE(prefixArray, NS_ERROR_OUT_OF_MEMORY);

  memcpy(prefixArray, prefixes.Elements(), sizeof(uint32_t) * itemCount);

  *aCount = itemCount;
  *aPrefixes = prefixArray;

  return NS_OK;
}

uint32_t nsUrlClassifierPrefixSet::BinSearch(uint32_t start,
                                             uint32_t end,
                                             uint32_t target)
{
  mLock.AssertCurrentThreadOwns();

  while (start != end && end >= start) {
    uint32_t i = start + ((end - start) >> 1);
    uint32_t value = mIndexPrefixes[i];
    if (value < target) {
      start = i + 1;
    } else if (value > target) {
      end = i - 1;
    } else {
      return i;
    }
  }
  return end;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::Contains(uint32_t aPrefix, bool* aFound)
{
  MutexAutoLock lock(mLock);

  *aFound = false;

  if (mIndexPrefixes.Length() == 0) {
    return NS_OK;
  }

  uint32_t target = aPrefix;

  // We want to do a "Price is Right" binary search, that is, we want to find
  // the index of the value either equal to the target or the closest value
  // that is less than the target.
  //
  if (target < mIndexPrefixes[0]) {
    return NS_OK;
  }

  // |binsearch| does not necessarily return the correct index (when the
  // target is not found) but rather it returns an index at least one away
  // from the correct index.
  // Because of this, we need to check if the target lies before the beginning
  // of the indices.

  uint32_t i = BinSearch(0, mIndexPrefixes.Length() - 1, target);
  if (mIndexPrefixes[i] > target && i > 0) {
    i--;
  }

  // Now search through the deltas for the target.
  uint32_t diff = target - mIndexPrefixes[i];
  uint32_t deltaSize  = mIndexDeltas[i].Length();
  uint32_t deltaIndex = 0;

  while (diff > 0 && deltaIndex < deltaSize) {
    diff -= mIndexDeltas[i][deltaIndex];
    deltaIndex++;
  }

  if (diff == 0) {
    *aFound = true;
  }

  return NS_OK;
}

MOZ_DEFINE_MALLOC_SIZE_OF(UrlClassifierMallocSizeOf)

NS_IMETHODIMP
nsUrlClassifierPrefixSet::CollectReports(nsIHandleReportCallback* aHandleReport,
                                         nsISupports* aData, bool aAnonymize)
{
  MOZ_ASSERT(NS_IsMainThread());

  // No need to get mLock here because this function does not directly touch
  // the class's data members. (SizeOfIncludingThis() will get mLock, however.)

  aHandleReport->Callback(
    EmptyCString(), mMemoryReportPath, KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(UrlClassifierMallocSizeOf),
    NS_LITERAL_CSTRING("Memory used by the prefix set for a URL classifier."),
    aData);

  return NS_OK;
}

size_t
nsUrlClassifierPrefixSet::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  MutexAutoLock lock(mLock);

  size_t n = 0;
  n += aMallocSizeOf(this);
  n += mIndexDeltas.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mIndexDeltas.Length(); i++) {
    n += mIndexDeltas[i].ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  n += mIndexPrefixes.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::IsEmpty(bool * aEmpty)
{
  MutexAutoLock lock(mLock);

  *aEmpty = (mIndexPrefixes.Length() == 0);
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::LoadFromFile(nsIFile* aFile)
{
  MutexAutoLock lock(mLock);

  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_PS_FILELOAD_TIME> timer;

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
  nsCOMPtr<nsIInputStream> in = NS_BufferInputStream(localInFile, bufferSize);

  rv = LoadPrefixes(in);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::StoreToFile(nsIFile* aFile)
{
  MutexAutoLock lock(mLock);

  nsCOMPtr<nsIOutputStream> localOutFile;
  nsresult rv = NS_NewLocalFileOutputStream(getter_AddRefs(localOutFile), aFile,
                                            PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t fileSize;

  // Preallocate the file storage
  {
    nsCOMPtr<nsIFileOutputStream> fos(do_QueryInterface(localOutFile));
    Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_PS_FALLOCATE_TIME> timer;

    fileSize = CalculatePreallocateSize();

    // Ignore failure, the preallocation is a hint and we write out the entire
    // file later on
    Unused << fos->Preallocate(fileSize);
  }

  // Convert to buffered stream
  nsCOMPtr<nsIOutputStream> out =
    NS_BufferOutputStream(localOutFile, std::min(fileSize, MAX_BUFFER_SIZE));

  rv = WritePrefixes(out);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Saving PrefixSet successful\n"));

  return NS_OK;
}

nsresult
nsUrlClassifierPrefixSet::LoadPrefixes(nsIInputStream* in)
{
  uint32_t magic;
  uint32_t read;

  nsresult rv = in->Read(reinterpret_cast<char*>(&magic), sizeof(uint32_t), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  if (magic == PREFIXSET_VERSION_MAGIC) {
    uint32_t indexSize;
    uint32_t deltaSize;

    rv = in->Read(reinterpret_cast<char*>(&indexSize), sizeof(uint32_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

    rv = in->Read(reinterpret_cast<char*>(&deltaSize), sizeof(uint32_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

    if (indexSize == 0) {
      LOG(("stored PrefixSet is empty!"));
      return NS_OK;
    }

    if (deltaSize > (indexSize * DELTAS_LIMIT)) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    nsTArray<uint32_t> indexStarts;
    if (!indexStarts.SetLength(indexSize, fallible) ||
        !mIndexPrefixes.SetLength(indexSize, fallible) ||
        !mIndexDeltas.SetLength(indexSize, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mTotalPrefixes = indexSize;

    uint32_t toRead = indexSize*sizeof(uint32_t);
    rv = in->Read(reinterpret_cast<char*>(mIndexPrefixes.Elements()), toRead, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FAILURE);

    rv = in->Read(reinterpret_cast<char*>(indexStarts.Elements()), toRead, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FAILURE);

    if (indexSize != 0 && indexStarts[0] != 0) {
      return NS_ERROR_FILE_CORRUPTED;
    }
    for (uint32_t i = 0; i < indexSize; i++) {
      uint32_t numInDelta = i == indexSize - 1 ? deltaSize - indexStarts[i]
                               : indexStarts[i + 1] - indexStarts[i];
      if (numInDelta > DELTAS_LIMIT) {
        return NS_ERROR_FILE_CORRUPTED;
      }
      if (numInDelta > 0) {
        if (!mIndexDeltas[i].SetLength(numInDelta, fallible)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        mTotalPrefixes += numInDelta;
        toRead = numInDelta * sizeof(uint16_t);
        rv = in->Read(reinterpret_cast<char*>(mIndexDeltas[i].Elements()), toRead, &read);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(read == toRead, NS_ERROR_FAILURE);
      }
    }
  } else {
    LOG(("Version magic mismatch, not loading"));
    return NS_ERROR_FILE_CORRUPTED;
  }

  MOZ_ASSERT(mIndexPrefixes.Length() == mIndexDeltas.Length());
  LOG(("Loading PrefixSet successful"));

  return NS_OK;
}

uint32_t
nsUrlClassifierPrefixSet::CalculatePreallocateSize()
{
  uint32_t fileSize = 4 * sizeof(uint32_t);
  uint32_t deltas = mTotalPrefixes - mIndexPrefixes.Length();
  fileSize += 2 * mIndexPrefixes.Length() * sizeof(uint32_t);
  fileSize += deltas * sizeof(uint16_t);
  return fileSize;
}

nsresult
nsUrlClassifierPrefixSet::WritePrefixes(nsIOutputStream* out)
{
  uint32_t written;
  uint32_t writelen = sizeof(uint32_t);
  uint32_t magic = PREFIXSET_VERSION_MAGIC;
  nsresult rv = out->Write(reinterpret_cast<char*>(&magic), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  uint32_t indexSize = mIndexPrefixes.Length();
  uint32_t indexDeltaSize = mIndexDeltas.Length();
  uint32_t totalDeltas = 0;

  // Store the shape of mIndexDeltas by noting at which "count" of total
  // indexes a new subarray starts. This is slightly cumbersome but keeps
  // file format compatibility.
  // If we ever update the format, we can gain space by storing the delta
  // subarray sizes, which fit in bytes.
  nsTArray<uint32_t> indexStarts;
  indexStarts.AppendElement(0);

  for (uint32_t i = 0; i < indexDeltaSize; i++) {
    uint32_t deltaLength = mIndexDeltas[i].Length();
    totalDeltas += deltaLength;
    if (!indexStarts.AppendElement(totalDeltas, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  rv = out->Write(reinterpret_cast<char*>(&indexSize), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  rv = out->Write(reinterpret_cast<char*>(&totalDeltas), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  writelen = indexSize * sizeof(uint32_t);
  rv = out->Write(reinterpret_cast<char*>(mIndexPrefixes.Elements()), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  rv = out->Write(reinterpret_cast<char*>(indexStarts.Elements()), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  if (totalDeltas > 0) {
    for (uint32_t i = 0; i < indexDeltaSize; i++) {
      writelen = mIndexDeltas[i].Length() * sizeof(uint16_t);
      rv = out->Write(reinterpret_cast<char*>(mIndexDeltas[i].Elements()), writelen, &written);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
    }
  }

  LOG(("Saving PrefixSet successful\n"));

  return NS_OK;
}
