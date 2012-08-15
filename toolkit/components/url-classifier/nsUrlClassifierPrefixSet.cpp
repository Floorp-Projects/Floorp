//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsIRandomGenerator.h"
#include "nsIFile.h"
#include "nsToolkitCompsCID.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
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

class nsPrefixSetReporter : public nsIMemoryReporter
{
public:
  nsPrefixSetReporter(nsUrlClassifierPrefixSet* aParent, const nsACString& aName);
  virtual ~nsPrefixSetReporter() {};

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

private:
  nsCString mPath;
  nsUrlClassifierPrefixSet* mParent;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPrefixSetReporter, nsIMemoryReporter)

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(StoragePrefixSetMallocSizeOf,
                                     "storage/prefixset")

nsPrefixSetReporter::nsPrefixSetReporter(nsUrlClassifierPrefixSet* aParent,
                                         const nsACString& aName)
: mParent(aParent)
{
  mPath.Assign(NS_LITERAL_CSTRING("explicit/storage/prefixset"));
  if (!aName.IsEmpty()) {
    mPath.Append("/");
    mPath.Append(aName);
  }
}

NS_IMETHODIMP
nsPrefixSetReporter::GetProcess(nsACString& aProcess)
{
  aProcess.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsPrefixSetReporter::GetPath(nsACString& aPath)
{
  aPath.Assign(mPath);
  return NS_OK;
}

NS_IMETHODIMP
nsPrefixSetReporter::GetKind(PRInt32* aKind)
{
  *aKind = nsIMemoryReporter::KIND_HEAP;
  return NS_OK;
}

NS_IMETHODIMP
nsPrefixSetReporter::GetUnits(PRInt32* aUnits)
{
  *aUnits = nsIMemoryReporter::UNITS_BYTES;
  return NS_OK;
}

NS_IMETHODIMP
nsPrefixSetReporter::GetAmount(PRInt64* aAmount)
{
  *aAmount = mParent->SizeOfIncludingThis(StoragePrefixSetMallocSizeOf);
  return NS_OK;
}

NS_IMETHODIMP
nsPrefixSetReporter::GetDescription(nsACString& aDescription)
{
  aDescription.Assign(NS_LITERAL_CSTRING("Memory used by a PrefixSet for "
                                         "UrlClassifier, in bytes."));
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsUrlClassifierPrefixSet, nsIUrlClassifierPrefixSet)

nsUrlClassifierPrefixSet::nsUrlClassifierPrefixSet()
  : mHasPrefixes(false)
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierPrefixSetLog)
    gUrlClassifierPrefixSetLog = PR_NewLogModule("UrlClassifierPrefixSet");
#endif
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::Init(const nsACString& aName)
{
  mReporter = new nsPrefixSetReporter(this, aName);
  NS_RegisterMemoryReporter(mReporter);

  return NS_OK;
}

nsUrlClassifierPrefixSet::~nsUrlClassifierPrefixSet()
{
  NS_UnregisterMemoryReporter(mReporter);
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::SetPrefixes(const PRUint32* aArray, PRUint32 aLength)
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
nsUrlClassifierPrefixSet::MakePrefixSet(const PRUint32* aPrefixes, PRUint32 aLength)
{
  if (aLength == 0) {
    return NS_OK;
  }

#ifdef DEBUG
  for (PRUint32 i = 1; i < aLength; i++) {
    MOZ_ASSERT(aPrefixes[i] >= aPrefixes[i-1]);
  }
#endif

  mIndexPrefixes.Clear();
  mIndexStarts.Clear();
  mDeltas.Clear();

  mIndexPrefixes.AppendElement(aPrefixes[0]);
  mIndexStarts.AppendElement(mDeltas.Length());

  PRUint32 numOfDeltas = 0;
  PRUint32 currentItem = aPrefixes[0];
  for (PRUint32 i = 1; i < aLength; i++) {
    if ((numOfDeltas >= DELTAS_LIMIT) ||
          (aPrefixes[i] - currentItem >= MAX_INDEX_DIFF)) {
      mIndexStarts.AppendElement(mDeltas.Length());
      mIndexPrefixes.AppendElement(aPrefixes[i]);
      numOfDeltas = 0;
    } else {
      PRUint16 delta = aPrefixes[i] - currentItem;
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
nsUrlClassifierPrefixSet::GetPrefixes(PRUint32* aCount,
                                      PRUint32** aPrefixes)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  NS_ENSURE_ARG_POINTER(aPrefixes);
  *aPrefixes = nullptr;

  nsTArray<PRUint32> aArray;
  PRUint32 prefixLength = mIndexPrefixes.Length();

  for (PRUint32 i = 0; i < prefixLength; i++) {
    PRUint32 prefix = mIndexPrefixes[i];
    PRUint32 start = mIndexStarts[i];
    PRUint32 end = (i == (prefixLength - 1)) ? mDeltas.Length()
                                             : mIndexStarts[i + 1];
    if (end > mDeltas.Length()) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    aArray.AppendElement(prefix);
    for (PRUint32 j = start; j < end; j++) {
      prefix += mDeltas[j];
      aArray.AppendElement(prefix);
    }
  }

  NS_ASSERTION(mIndexStarts.Length() + mDeltas.Length() == aArray.Length(),
               "Lengths are inconsistent");

  PRUint32 itemCount = aArray.Length();

  PRUint32* retval = static_cast<PRUint32*>(nsMemory::Alloc(itemCount * sizeof(PRUint32)));
  NS_ENSURE_TRUE(retval, NS_ERROR_OUT_OF_MEMORY);
  for (PRUint32 i = 0; i < itemCount; i++) {
    retval[i] = aArray[i];
  }

  *aCount = itemCount;
  *aPrefixes = retval;

  return NS_OK;
}

PRUint32 nsUrlClassifierPrefixSet::BinSearch(PRUint32 start,
                                             PRUint32 end,
                                             PRUint32 target)
{
  while (start != end && end >= start) {
    PRUint32 i = start + ((end - start) >> 1);
    PRUint32 value = mIndexPrefixes[i];
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
nsUrlClassifierPrefixSet::Contains(PRUint32 aPrefix, bool* aFound)
{
  *aFound = false;

  if (!mHasPrefixes) {
    return NS_OK;
  }

  PRUint32 target = aPrefix;

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

  PRUint32 i = BinSearch(0, mIndexPrefixes.Length() - 1, target);
  if (mIndexPrefixes[i] > target && i > 0) {
    i--;
  }

  // Now search through the deltas for the target.
  PRUint32 diff = target - mIndexPrefixes[i];
  PRUint32 deltaIndex = mIndexStarts[i];
  PRUint32 deltaSize  = mDeltas.Length();
  PRUint32 end = (i + 1 < mIndexStarts.Length()) ? mIndexStarts[i+1]
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

size_t
nsUrlClassifierPrefixSet::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf)
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
  PRUint32 magic;
  PRInt32 read;

  read = PR_Read(fileFd, &magic, sizeof(PRUint32));
  NS_ENSURE_TRUE(read == sizeof(PRUint32), NS_ERROR_FAILURE);

  if (magic == PREFIXSET_VERSION_MAGIC) {
    PRUint32 indexSize;
    PRUint32 deltaSize;

    read = PR_Read(fileFd, &indexSize, sizeof(PRUint32));
    NS_ENSURE_TRUE(read == sizeof(PRUint32), NS_ERROR_FILE_CORRUPTED);
    read = PR_Read(fileFd, &deltaSize, sizeof(PRUint32));
    NS_ENSURE_TRUE(read == sizeof(PRUint32), NS_ERROR_FILE_CORRUPTED);

    if (indexSize == 0) {
      LOG(("stored PrefixSet is empty!"));
      return NS_ERROR_FAILURE;
    }

    if (deltaSize > (indexSize * DELTAS_LIMIT)) {
      return NS_ERROR_FILE_CORRUPTED;
    }

    mIndexStarts.SetLength(indexSize);
    mIndexPrefixes.SetLength(indexSize);
    mDeltas.SetLength(deltaSize);

    PRInt32 toRead = indexSize*sizeof(PRUint32);
    read = PR_Read(fileFd, mIndexPrefixes.Elements(), toRead);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FILE_CORRUPTED);
    read = PR_Read(fileFd, mIndexStarts.Elements(), toRead);
    NS_ENSURE_TRUE(read == toRead, NS_ERROR_FILE_CORRUPTED);
    if (deltaSize > 0) {
      toRead = deltaSize*sizeof(PRUint16);
      read = PR_Read(fileFd, mDeltas.Elements(), toRead);
      NS_ENSURE_TRUE(read == toRead, NS_ERROR_FILE_CORRUPTED);
    }

    mHasPrefixes = true;
  } else {
    LOG(("Version magic mismatch, not loading"));
    return NS_ERROR_FAILURE;
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
      PRInt64 size = 4 * sizeof(PRUint32);
      size += 2 * mIndexStarts.Length() * sizeof(PRUint32);
      size +=     mDeltas.Length() * sizeof(PRUint16);

      mozilla::fallocate(fileFd, size);
  }

  PRInt32 written;
  PRInt32 writelen = sizeof(PRUint32);
  PRUint32 magic = PREFIXSET_VERSION_MAGIC;
  written = PR_Write(fileFd, &magic, writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  PRUint32 indexSize = mIndexStarts.Length();
  PRUint32 deltaSize = mDeltas.Length();
  written = PR_Write(fileFd, &indexSize, writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  written = PR_Write(fileFd, &deltaSize, writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);

  writelen = indexSize * sizeof(PRUint32);
  written = PR_Write(fileFd, mIndexPrefixes.Elements(), writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  written = PR_Write(fileFd, mIndexStarts.Elements(), writelen);
  NS_ENSURE_TRUE(written == writelen, NS_ERROR_FAILURE);
  if (deltaSize > 0) {
    writelen = deltaSize * sizeof(PRUint16);
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
