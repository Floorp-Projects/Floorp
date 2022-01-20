/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include <algorithm>

using namespace mozilla;

// MOZ_LOG=UrlClassifierPrefixSet:5
static LazyLogModule gUrlClassifierPrefixSetLog("UrlClassifierPrefixSet");
#define LOG(args) \
  MOZ_LOG(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gUrlClassifierPrefixSetLog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(nsUrlClassifierPrefixSet, nsIUrlClassifierPrefixSet)

nsUrlClassifierPrefixSet::nsUrlClassifierPrefixSet()
    : mLock("nsUrlClassifierPrefixSet.mLock"), mTotalPrefixes(0) {}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::Init(const nsACString& aName) {
  mName = aName;

  return NS_OK;
}

nsUrlClassifierPrefixSet::~nsUrlClassifierPrefixSet() {}

void nsUrlClassifierPrefixSet::Clear() {
  LOG(("[%s] Clearing PrefixSet", mName.get()));
  mIndexDeltas.Clear();
  mIndexPrefixes.Clear();
  mTotalPrefixes = 0;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::SetPrefixes(const uint32_t* aArray,
                                      uint32_t aLength) {
  MutexAutoLock lock(mLock);

  nsresult rv = NS_OK;
  Clear();

  if (aLength > 0) {
    rv = MakePrefixSet(aArray, aLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Clear();  // clear out any leftovers
    }
  }

  MOZ_ASSERT_IF(mIndexDeltas.IsEmpty(),
                mIndexPrefixes.Length() == mTotalPrefixes);
  MOZ_ASSERT_IF(!mIndexDeltas.IsEmpty(),
                mIndexPrefixes.Length() == mIndexDeltas.Length());
  return rv;
}

nsresult nsUrlClassifierPrefixSet::MakePrefixSet(const uint32_t* aPrefixes,
                                                 uint32_t aLength) {
  mLock.AssertCurrentThreadOwns();

  MOZ_ASSERT(aPrefixes);
  MOZ_ASSERT(aLength > 0);

#ifdef DEBUG
  for (uint32_t i = 1; i < aLength; i++) {
    MOZ_ASSERT(aPrefixes[i] >= aPrefixes[i - 1]);
  }
#endif

  uint32_t totalDeltas = 0;

  // Request one memory space to store all the prefixes may lead to
  // memory allocation failure on certain platforms(See Bug 1046038).
  // So if required size to store all the prefixes exceeds defined
  // threshold(512k), we divide prefixes into delta chunks instead. Note that
  // the potential overhead of this approach is that it may reuqire more memory
  // compared to store all prefixes in one array because of jemalloc's
  // implementation.
  if (aLength * sizeof(uint32_t) <
      StaticPrefs::browser_safebrowsing_prefixset_max_array_size()) {
    // Not over the threshold, store all prefixes into mIndexPrefixes.
    // mIndexDeltas is empty in this case.
    mIndexPrefixes.SetCapacity(aLength);
    for (uint32_t i = 0; i < aLength; i++) {
      mIndexPrefixes.AppendElement(aPrefixes[i]);
    }
  } else {
    // Apply delta algorithm to split prefixes into smaller delta chunk.

    // We estimate the capacity of mIndexPrefixes & mIndexDeltas by assuming
    // each element in mIndexDeltas stores DELTAS_LIMITS deltas, so the
    // number of indexed prefixes is round up of
    // TotalPrefixes / (DELTA_LIMIT + 1)
    // The estimation only works when the number of prefixes are over a
    // certain limit, which means, arrays in mIndexDeltas are always full.
    uint32_t estimateCapacity =
        (aLength + (DELTAS_LIMIT + 1) - 1) / (DELTAS_LIMIT + 1);
    mIndexPrefixes.SetCapacity(estimateCapacity);
    mIndexDeltas.SetCapacity(estimateCapacity);

    mIndexPrefixes.AppendElement(aPrefixes[0]);
    mIndexDeltas.AppendElement();
    mIndexDeltas.LastElement().SetCapacity(DELTAS_LIMIT);

    uint32_t numOfDeltas = 0;
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
        mIndexDeltas.LastElement().SetCapacity(DELTAS_LIMIT);

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

    MOZ_ASSERT(mIndexPrefixes.Length() == mIndexDeltas.Length());
  }

  if (totalDeltas == 0) {
    // We have to clear mIndexDeltas here because it is still possible
    // that the delta generation algorithm produces no deltas at all. When that
    // happens, mIndexDeltas is not empty, which conflicts with the assumption
    // that when there is no delta, mIndexDeltas is empty.
    mIndexDeltas.Clear();
  }
  mTotalPrefixes = aLength;

  LOG(("Total number of indices: %d", aLength));
  LOG(("Total number of deltas: %d", totalDeltas));
  LOG(("Total number of delta chunks: %zu", mIndexDeltas.Length()));

  return NS_OK;
}

nsresult nsUrlClassifierPrefixSet::GetPrefixesNative(
    FallibleTArray<uint32_t>& outArray) {
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

    if (mIndexDeltas.IsEmpty()) {
      continue;
    }

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
nsUrlClassifierPrefixSet::GetPrefixes(uint32_t* aCount, uint32_t** aPrefixes) {
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
  uint32_t* prefixArray =
      static_cast<uint32_t*>(moz_xmalloc(itemCount * sizeof(uint32_t)));

  memcpy(prefixArray, prefixes.Elements(), sizeof(uint32_t) * itemCount);

  *aCount = itemCount;
  *aPrefixes = prefixArray;

  return NS_OK;
}

uint32_t nsUrlClassifierPrefixSet::BinSearch(uint32_t start, uint32_t end,
                                             uint32_t target) const {
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
nsUrlClassifierPrefixSet::Contains(uint32_t aPrefix, bool* aFound) {
  MutexAutoLock lock(mLock);

  *aFound = false;

  if (IsEmptyInternal()) {
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

  if (!mIndexDeltas.IsEmpty()) {
    uint32_t deltaSize = mIndexDeltas[i].Length();
    uint32_t deltaIndex = 0;

    while (diff > 0 && deltaIndex < deltaSize) {
      diff -= mIndexDeltas[i][deltaIndex];
      deltaIndex++;
    }
  }

  if (diff == 0) {
    *aFound = true;
  }

  return NS_OK;
}

size_t nsUrlClassifierPrefixSet::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
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

bool nsUrlClassifierPrefixSet::IsEmptyInternal() const {
  if (mIndexPrefixes.IsEmpty()) {
    MOZ_ASSERT(mIndexDeltas.IsEmpty() && mTotalPrefixes == 0,
               "If we're empty, there should be no leftovers.");
    return true;
  }

  MOZ_ASSERT(mTotalPrefixes >= mIndexPrefixes.Length());
  return false;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::IsEmpty(bool* aEmpty) {
  MutexAutoLock lock(mLock);

  *aEmpty = IsEmptyInternal();
  return NS_OK;
}

nsresult nsUrlClassifierPrefixSet::LoadPrefixes(nsCOMPtr<nsIInputStream>& in) {
  MutexAutoLock lock(mLock);

  mCanary.Check();
  Clear();

  uint32_t magic;
  uint32_t read;

  nsresult rv =
      in->Read(reinterpret_cast<char*>(&magic), sizeof(uint32_t), &read);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  if (magic == PREFIXSET_VERSION_MAGIC) {
    // Read the number of indexed prefixes
    uint32_t indexSize;
    rv = in->Read(reinterpret_cast<char*>(&indexSize), sizeof(uint32_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

    // Read the number of delta prefixes
    uint32_t deltaSize;
    rv = in->Read(reinterpret_cast<char*>(&deltaSize), sizeof(uint32_t), &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

    if (indexSize == 0) {
      LOG(("[%s] Stored PrefixSet is empty!", mName.get()));
      return NS_OK;
    }

    if (!mIndexPrefixes.SetLength(indexSize, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mTotalPrefixes = indexSize;
    if (deltaSize > (indexSize * DELTAS_LIMIT)) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    // Read index prefixes
    uint32_t toRead = indexSize * sizeof(uint32_t);
    rv = in->Read(reinterpret_cast<char*>(mIndexPrefixes.Elements()), toRead,
                  &read);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FAILURE);

    if (deltaSize) {
      nsTArray<uint32_t> indexStarts;

      if (!indexStarts.SetLength(indexSize, fallible) ||
          !mIndexDeltas.SetLength(indexSize, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // Read index start array to construct mIndexDeltas
      rv = in->Read(reinterpret_cast<char*>(indexStarts.Elements()), toRead,
                    &read);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(read == toRead, NS_ERROR_FAILURE);

      if (indexStarts[0] != 0) {
        return NS_ERROR_FILE_CORRUPTED;
      }

      for (uint32_t i = 0; i < indexSize; i++) {
        uint32_t numInDelta = i == indexSize - 1
                                  ? deltaSize - indexStarts[i]
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
          rv = in->Read(reinterpret_cast<char*>(mIndexDeltas[i].Elements()),
                        toRead, &read);
          NS_ENSURE_SUCCESS(rv, rv);
          NS_ENSURE_TRUE(read == toRead, NS_ERROR_FAILURE);
        }
      }
    } else {
      mIndexDeltas.Clear();
    }
  } else {
    LOG(("[%s] Version magic mismatch, not loading", mName.get()));
    return NS_ERROR_FILE_CORRUPTED;
  }

  MOZ_ASSERT_IF(mIndexDeltas.IsEmpty(),
                mIndexPrefixes.Length() == mTotalPrefixes);
  MOZ_ASSERT_IF(!mIndexDeltas.IsEmpty(),
                mIndexPrefixes.Length() == mIndexDeltas.Length());
  LOG(("[%s] Loading PrefixSet successful (%u total prefixes)", mName.get(),
       mTotalPrefixes));

  return NS_OK;
}

uint32_t nsUrlClassifierPrefixSet::CalculatePreallocateSize() const {
  uint32_t fileSize = 4 * sizeof(uint32_t);
  MOZ_RELEASE_ASSERT(mTotalPrefixes >= mIndexPrefixes.Length());
  uint32_t deltas = mTotalPrefixes - mIndexPrefixes.Length();
  fileSize += mIndexPrefixes.Length() * sizeof(uint32_t);
  if (deltas) {
    MOZ_ASSERT(mIndexPrefixes.Length() == mIndexDeltas.Length());

    fileSize += mIndexPrefixes.Length() * sizeof(uint32_t);
    fileSize += mIndexDeltas.Length() * sizeof(uint32_t);
    fileSize += deltas * sizeof(uint16_t);
  }
  return fileSize;
}

nsresult nsUrlClassifierPrefixSet::WritePrefixes(
    nsCOMPtr<nsIOutputStream>& out) const {
  MutexAutoLock lock(mLock);

  MOZ_ASSERT_IF(mIndexDeltas.IsEmpty(),
                mIndexPrefixes.Length() == mTotalPrefixes);
  MOZ_ASSERT_IF(!mIndexDeltas.IsEmpty(),
                mIndexPrefixes.Length() == mIndexDeltas.Length());

  mCanary.Check();

  uint32_t written;
  uint32_t writelen = sizeof(uint32_t);
  const uint32_t magic = PREFIXSET_VERSION_MAGIC;
  nsresult rv =
      out->Write(reinterpret_cast<const char*>(&magic), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  const uint32_t indexSize = mIndexPrefixes.Length();
  if (mIndexDeltas.IsEmpty()) {
    if (NS_WARN_IF(mTotalPrefixes != indexSize)) {
      LOG(("[%s] mIndexPrefixes doesn't have the same length as mTotalPrefixes",
           mName.get()));
      return NS_ERROR_FAILURE;
    }
  } else {
    if (NS_WARN_IF(mIndexDeltas.Length() != indexSize)) {
      LOG(("[%s] mIndexPrefixes doesn't have the same length as mIndexDeltas",
           mName.get()));
      return NS_ERROR_FAILURE;
    }
  }
  uint32_t totalDeltas = 0;

  // Store the shape of mIndexDeltas by noting at which "count" of total
  // indexes a new subarray starts. This is slightly cumbersome but keeps
  // file format compatibility.
  // If we ever update the format, we can gain space by storing the delta
  // subarray sizes, which fit in bytes.
  nsTArray<uint32_t> indexStarts;
  if (!mIndexDeltas.IsEmpty()) {
    if (!indexStarts.SetCapacity(indexSize + 1, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    indexStarts.AppendElement(0);

    for (uint32_t i = 0; i < indexSize; i++) {
      uint32_t deltaLength = mIndexDeltas[i].Length();
      totalDeltas += deltaLength;
      indexStarts.AppendElement(totalDeltas);
    }
    indexStarts.RemoveLastElement();  // we don't use the last element
    MOZ_ASSERT(indexStarts.Length() == indexSize);
  }

  rv =
      out->Write(reinterpret_cast<const char*>(&indexSize), writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  rv = out->Write(reinterpret_cast<const char*>(&totalDeltas), writelen,
                  &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  writelen = indexSize * sizeof(uint32_t);
  rv = out->Write(reinterpret_cast<const char*>(mIndexPrefixes.Elements()),
                  writelen, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  if (!mIndexDeltas.IsEmpty()) {
    MOZ_ASSERT(!indexStarts.IsEmpty() && totalDeltas > 0);
    rv = out->Write(reinterpret_cast<const char*>(indexStarts.Elements()),
                    writelen, &written);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

    for (uint32_t i = 0; i < indexSize; i++) {
      writelen = mIndexDeltas[i].Length() * sizeof(uint16_t);
      rv = out->Write(reinterpret_cast<const char*>(mIndexDeltas[i].Elements()),
                      writelen, &written);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
    }
  }

  LOG(("[%s] Writing PrefixSet successful", mName.get()));

  return NS_OK;
}
