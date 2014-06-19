/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsIRandomGenerator.h"
#include "nsIFile.h"
#include "nsToolkitCompsCID.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/FileUtils.h"
#include "prlog.h"

using namespace mozilla;

// NSPR_LOG_MODULES=UrlClassifierPrefixSet:5
#if defined(PR_LOGGING)
static const PRLogModuleInfo *gUrlClassifierPrefixSetLog = nullptr;
#define LOG(args) PR_LOG(gUrlClassifierPrefixSetLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierPrefixSetLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#endif

NS_IMPL_ISUPPORTS(
  nsUrlClassifierPrefixSet, nsIUrlClassifierPrefixSet, nsIMemoryReporter)

nsUrlClassifierPrefixSet::nsUrlClassifierPrefixSet()
  : mHasPrefixes(false)
  , mMemoryReportPath()
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierPrefixSetLog)
    gUrlClassifierPrefixSetLog = PR_NewLogModule("UrlClassifierPrefixSet");
#endif
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
  if (aLength <= 0) {
    if (mHasPrefixes) {
      LOG(("Clearing PrefixSet"));
      mDeltas.Clear();
      mIndexPrefixes.Clear();
      mIndexStarts.Clear();
      mHasPrefixes = false;
    }
  } else {
    return MakePrefixSet(aArray, aLength);
  }

  return NS_OK;
}

nsresult
nsUrlClassifierPrefixSet::MakePrefixSet(const uint32_t* aPrefixes, uint32_t aLength)
{
  if (aLength == 0) {
    return NS_OK;
  }

#ifdef DEBUG
  for (uint32_t i = 1; i < aLength; i++) {
    MOZ_ASSERT(aPrefixes[i] >= aPrefixes[i-1]);
  }
#endif

  mIndexPrefixes.Clear();
  mIndexStarts.Clear();
  mDeltas.Clear();

  mIndexPrefixes.AppendElement(aPrefixes[0]);
  mIndexStarts.AppendElement(mDeltas.Length());

  uint32_t numOfDeltas = 0;
  uint32_t currentItem = aPrefixes[0];
  for (uint32_t i = 1; i < aLength; i++) {
    if ((numOfDeltas >= DELTAS_LIMIT) ||
          (aPrefixes[i] - currentItem >= MAX_INDEX_DIFF)) {
      mIndexStarts.AppendElement(mDeltas.Length());
      mIndexPrefixes.AppendElement(aPrefixes[i]);
      numOfDeltas = 0;
    } else {
      uint16_t delta = aPrefixes[i] - currentItem;
      mDeltas.AppendElement(delta);
      numOfDeltas++;
    }
    currentItem = aPrefixes[i];
  }

  mIndexPrefixes.Compact();
  mIndexStarts.Compact();
  mDeltas.Compact();

  LOG(("Total number of indices: %d", mIndexPrefixes.Length()));
  LOG(("Total number of deltas: %d", mDeltas.Length()));

  mHasPrefixes = true;

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::GetPrefixes(uint32_t* aCount,
                                      uint32_t** aPrefixes)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  NS_ENSURE_ARG_POINTER(aPrefixes);
  *aPrefixes = nullptr;

  uint64_t itemCount = mIndexStarts.Length() + mDeltas.Length();
  uint32_t* prefixArray = static_cast<uint32_t*>(nsMemory::Alloc(itemCount * sizeof(uint32_t)));
  NS_ENSURE_TRUE(prefixArray, NS_ERROR_OUT_OF_MEMORY);

  uint32_t prefixIdxLength = mIndexPrefixes.Length();
  uint32_t prefixCnt = 0;

  for (uint32_t i = 0; i < prefixIdxLength; i++) {
    uint32_t prefix = mIndexPrefixes[i];
    uint32_t start = mIndexStarts[i];
    uint32_t end = (i == (prefixIdxLength - 1)) ? mDeltas.Length()
                                                : mIndexStarts[i + 1];
    if (end > mDeltas.Length() || (start > end)) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    prefixArray[prefixCnt++] = prefix;
    for (uint32_t j = start; j < end; j++) {
      prefix += mDeltas[j];
      prefixArray[prefixCnt++] = prefix;
    }
  }

  NS_ASSERTION(itemCount == prefixCnt, "Lengths are inconsistent");

  *aCount = itemCount;
  *aPrefixes = prefixArray;

  return NS_OK;
}

uint32_t nsUrlClassifierPrefixSet::BinSearch(uint32_t start,
                                             uint32_t end,
                                             uint32_t target)
{
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
  *aFound = false;

  if (!mHasPrefixes) {
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
  uint32_t deltaIndex = mIndexStarts[i];
  uint32_t deltaSize  = mDeltas.Length();
  uint32_t end = (i + 1 < mIndexStarts.Length()) ? mIndexStarts[i+1]
                                                 : deltaSize;

  // Sanity check the read values
  if (end > deltaSize) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  while (diff > 0 && deltaIndex < end) {
    diff -= mDeltas[deltaIndex];
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
  return aHandleReport->Callback(
    EmptyCString(), mMemoryReportPath, KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(UrlClassifierMallocSizeOf),
    NS_LITERAL_CSTRING("Memory used by the prefix set for a URL classifier."),
    aData);
}

size_t
nsUrlClassifierPrefixSet::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = 0;
  n += aMallocSizeOf(this);
  n += mDeltas.SizeOfExcludingThis(aMallocSizeOf);
  n += mIndexPrefixes.SizeOfExcludingThis(aMallocSizeOf);
  n += mIndexStarts.SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::IsEmpty(bool * aEmpty)
{
  *aEmpty = !mHasPrefixes;
  return NS_OK;
}

nsresult
nsUrlClassifierPrefixSet::LoadFromFd(AutoFDClose& fileFd)
{
  uint32_t magic;
  int32_t read;

  read = PR_Read(fileFd, &magic, sizeof(uint32_t));
  NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FAILURE);

  if (magic == PREFIXSET_VERSION_MAGIC) {
    uint32_t indexSize;
    uint32_t deltaSize;

    read = PR_Read(fileFd, &indexSize, sizeof(uint32_t));
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FILE_CORRUPTED);
    read = PR_Read(fileFd, &deltaSize, sizeof(uint32_t));
    NS_ENSURE_TRUE(read == sizeof(uint32_t), NS_ERROR_FILE_CORRUPTED);

    if (indexSize == 0) {
      LOG(("stored PrefixSet is empty!"));
      return NS_OK;
    }

    if (deltaSize > (indexSize * DELTAS_LIMIT)) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    mIndexStarts.SetLength(indexSize);
    mIndexPrefixes.SetLength(indexSize);
    mDeltas.SetLength(deltaSize);

    int32_t toRead = indexSize*sizeof(uint32_t);
    read = PR_Read(fileFd, mIndexPrefixes.Elements(), toRead);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FILE_CORRUPTED);
    read = PR_Read(fileFd, mIndexStarts.Elements(), toRead);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FILE_CORRUPTED);
    if (indexSize != 0 && mIndexStarts[0] != 0) {
      return NS_ERROR_FILE_CORRUPTED;
    }
    if (deltaSize > 0) {
      toRead = deltaSize*sizeof(uint16_t);
      read = PR_Read(fileFd, mDeltas.Elements(), toRead);
      NS_ENSURE_TRUE(read == toRead, NS_ERROR_FILE_CORRUPTED);
    }

    mHasPrefixes = true;
  } else {
    LOG(("Version magic mismatch, not loading"));
    return NS_ERROR_FILE_CORRUPTED;
  }

  LOG(("Loading PrefixSet successful"));

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::LoadFromFile(nsIFile* aFile)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_PS_FILELOAD_TIME> timer;

  nsresult rv;
  AutoFDClose fileFd;
  rv = aFile->OpenNSPRFileDesc(PR_RDONLY | nsIFile::OS_READAHEAD,
                               0, &fileFd.rwget());
  NS_ENSURE_SUCCESS(rv, rv);

  return LoadFromFd(fileFd);
}

nsresult
nsUrlClassifierPrefixSet::StoreToFd(AutoFDClose& fileFd)
{
  {
      Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_PS_FALLOCATE_TIME> timer;
      int64_t size = 4 * sizeof(uint32_t);
      size += 2 * mIndexStarts.Length() * sizeof(uint32_t);
      size +=     mDeltas.Length() * sizeof(uint16_t);

      mozilla::fallocate(fileFd, size);
  }

  int32_t written;
  int32_t writelen = sizeof(uint32_t);
  uint32_t magic = PREFIXSET_VERSION_MAGIC;
  written = PR_Write(fileFd, &magic, writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  uint32_t indexSize = mIndexStarts.Length();
  uint32_t deltaSize = mDeltas.Length();
  written = PR_Write(fileFd, &indexSize, writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  written = PR_Write(fileFd, &deltaSize, writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  writelen = indexSize * sizeof(uint32_t);
  written = PR_Write(fileFd, mIndexPrefixes.Elements(), writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  written = PR_Write(fileFd, mIndexStarts.Elements(), writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  if (deltaSize > 0) {
    writelen = deltaSize * sizeof(uint16_t);
    written = PR_Write(fileFd, mDeltas.Elements(), writelen);
    NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  }

  LOG(("Saving PrefixSet successful\n"));

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::StoreToFile(nsIFile* aFile)
{
  AutoFDClose fileFd;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDWR | PR_TRUNCATE | PR_CREATE_FILE,
                                        0644, &fileFd.rwget());
  NS_ENSURE_SUCCESS(rv, rv);

  return StoreToFd(fileFd);
}
