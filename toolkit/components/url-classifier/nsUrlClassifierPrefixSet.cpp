//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *   Mehdi Mulani <mars.martian+bugmail@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierPrefixSet.h"
#include "nsIRandomGenerator.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsToolkitCompsCID.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/FileUtils.h"
#include "prlog.h"

using namespace mozilla;

// NSPR_LOG_MODULES=UrlClassifierPrefixSet:5
#if defined(PR_LOGGING)
static const PRLogModuleInfo *gUrlClassifierPrefixSetLog = nsnull;
#define LOG(args) PR_LOG(gUrlClassifierPrefixSetLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierPrefixSetLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (PR_FALSE)
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUrlClassifierPrefixSet, nsIUrlClassifierPrefixSet)

nsUrlClassifierPrefixSet::nsUrlClassifierPrefixSet()
  : mPrefixSetLock("mPrefixSetLock"),
    mSetIsReady(mPrefixSetLock, "mSetIsReady"),
    mHasPrefixes(PR_FALSE),
    mRandomKey(0)
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierPrefixSetLog)
    gUrlClassifierPrefixSetLog = PR_NewLogModule("UrlClassifierPrefixSet");
#endif

  nsresult rv = InitKey();
  if (NS_FAILED(rv)) {
    LOG(("Failed to initialize PrefixSet"));
  }
}

nsresult
nsUrlClassifierPrefixSet::InitKey()
{
  nsCOMPtr<nsIRandomGenerator> rg =
    do_GetService("@mozilla.org/security/random-generator;1");
  NS_ENSURE_STATE(rg);

  PRUint8 *temp;
  nsresult rv = rg->GenerateRandomBytes(sizeof(mRandomKey), &temp);
  NS_ENSURE_SUCCESS(rv, rv);
  memcpy(&mRandomKey, temp, sizeof(mRandomKey));
  NS_Free(temp);

  LOG(("Initialized PrefixSet, key = %X", mRandomKey));

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::SetPrefixes(const PRUint32 * aArray, PRUint32 aLength)
{
  {
    MutexAutoLock lock(mPrefixSetLock);
    if (mHasPrefixes) {
      LOG(("Clearing PrefixSet"));
      mDeltas.Clear();
      mIndexPrefixes.Clear();
      mIndexStarts.Clear();
      mHasPrefixes = PR_FALSE;
    }
  }
  if (aLength > 0) {
    // Ensure they are sorted before adding
    nsTArray<PRUint32> prefixes;
    prefixes.AppendElements(aArray, aLength);
    prefixes.Sort();
    AddPrefixes(prefixes.Elements(), prefixes.Length());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::AddPrefixes(const PRUint32 * prefixes, PRUint32 aLength)
{
  if (aLength == 0) {
    return NS_OK;
  }

  nsTArray<PRUint32> mNewIndexPrefixes(mIndexPrefixes);
  nsTArray<PRUint32> mNewIndexStarts(mIndexStarts);
  nsTArray<PRUint16> mNewDeltas(mDeltas);

  mNewIndexPrefixes.AppendElement(prefixes[0]);
  mNewIndexStarts.AppendElement(mNewDeltas.Length());

  PRUint32 numOfDeltas = 0;
  PRUint32 currentItem = prefixes[0];
  for (PRUint32 i = 1; i < aLength; i++) {
    if ((numOfDeltas >= DELTAS_LIMIT) ||
          (prefixes[i] - currentItem >= MAX_INDEX_DIFF)) {
      mNewIndexStarts.AppendElement(mNewDeltas.Length());
      mNewIndexPrefixes.AppendElement(prefixes[i]);
      numOfDeltas = 0;
    } else {
      PRUint16 delta = prefixes[i] - currentItem;
      mNewDeltas.AppendElement(delta);
      numOfDeltas++;
    }
    currentItem = prefixes[i];
  }

  mNewIndexPrefixes.Compact();
  mNewIndexStarts.Compact();
  mNewDeltas.Compact();

  LOG(("Total number of indices: %d", mNewIndexPrefixes.Length()));
  LOG(("Total number of deltas: %d", mNewDeltas.Length()));

  MutexAutoLock lock(mPrefixSetLock);

  // This just swaps some pointers
  mIndexPrefixes.SwapElements(mNewIndexPrefixes);
  mIndexStarts.SwapElements(mNewIndexStarts);
  mDeltas.SwapElements(mNewDeltas);

  mHasPrefixes = PR_TRUE;
  mSetIsReady.NotifyAll();

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
nsUrlClassifierPrefixSet::Contains(PRUint32 aPrefix, PRBool * aFound)
{
  *aFound = PR_FALSE;

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
  PRUint32 end = (i + 1 < mIndexStarts.Length()) ? mIndexStarts[i+1]
                                                 : mDeltas.Length();
  while (diff > 0 && deltaIndex < end) {
    diff -= mDeltas[deltaIndex];
    deltaIndex++;
  }

  if (diff == 0) {
    *aFound = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::EstimateSize(PRUint32 * aSize)
{
  MutexAutoLock lock(mPrefixSetLock);
  *aSize = sizeof(PRBool);
  if (mHasPrefixes) {
    *aSize += sizeof(PRUint16) * mDeltas.Length();
    *aSize += sizeof(PRUint32) * mIndexPrefixes.Length();
    *aSize += sizeof(PRUint32) * mIndexStarts.Length();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::IsEmpty(PRBool * aEmpty)
{
  MutexAutoLock lock(mPrefixSetLock);
  *aEmpty = !mHasPrefixes;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::GetKey(PRUint32 * aKey)
 {
   MutexAutoLock lock(mPrefixSetLock);
   *aKey = mRandomKey;
   return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::Probe(PRUint32 aPrefix, PRUint32 aKey,
                                PRBool* aReady, PRBool* aFound)
{
  MutexAutoLock lock(mPrefixSetLock);

  // We might have raced here with a LoadPrefixSet call,
  // loading a saved PrefixSet with another key than the one used to probe us.
  // This must occur exactly between the GetKey call and the Probe call.
  // This could cause a false negative immediately after browser start.
  // Claim we are still busy loading instead.
  if (aKey != mRandomKey) {
    LOG(("Potential race condition detected, avoiding"));
    *aReady = PR_FALSE;
    return NS_OK;
  }

  // check whether we are opportunistically probing or should wait
  if (*aReady) {
    // we should block until we are ready
    while (!mHasPrefixes) {
      LOG(("Set is empty, probe must wait"));
      mSetIsReady.Wait();
    }
  } else {
    // opportunistic probe -> check if set is loaded
    if (mHasPrefixes) {
      *aReady = PR_TRUE;
    } else {
      return NS_OK;
    }
  }

  nsresult rv = Contains(aPrefix, aFound);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsUrlClassifierPrefixSet::LoadFromFd(AutoFDClose & fileFd)
{
  PRUint32 magic;
  PRInt32 read;

  read = PR_Read(fileFd, &magic, sizeof(PRUint32));
  NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);

  if (magic == PREFIXSET_VERSION_MAGIC) {
    PRUint32 indexSize;
    PRUint32 deltaSize;

    read = PR_Read(fileFd, &mRandomKey, sizeof(PRUint32));
    NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);
    read = PR_Read(fileFd, &indexSize, sizeof(PRUint32));
    NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);
    read = PR_Read(fileFd, &deltaSize, sizeof(PRUint32));
    NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);

    if (indexSize == 0) {
      LOG(("stored PrefixSet is empty!"));
      return NS_ERROR_FAILURE;
    }

    nsTArray<PRUint32> mNewIndexPrefixes;
    nsTArray<PRUint32> mNewIndexStarts;
    nsTArray<PRUint16> mNewDeltas;

    mNewIndexStarts.SetLength(indexSize);
    mNewIndexPrefixes.SetLength(indexSize);
    mNewDeltas.SetLength(deltaSize);

    read = PR_Read(fileFd, mNewIndexPrefixes.Elements(), indexSize*sizeof(PRUint32));
    NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);
    read = PR_Read(fileFd, mNewIndexStarts.Elements(), indexSize*sizeof(PRUint32));
    NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);
    if (deltaSize > 0) {
      read = PR_Read(fileFd, mNewDeltas.Elements(), deltaSize*sizeof(PRUint16));
      NS_ENSURE_TRUE(read > 0, NS_ERROR_FAILURE);
    }

    MutexAutoLock lock(mPrefixSetLock);

    mIndexPrefixes.SwapElements(mNewIndexPrefixes);
    mIndexStarts.SwapElements(mNewIndexStarts);
    mDeltas.SwapElements(mNewDeltas);

    mHasPrefixes = PR_TRUE;
    mSetIsReady.NotifyAll();
  } else {
    LOG(("Version magic mismatch, not loading"));
    return NS_ERROR_FAILURE;
  }

  LOG(("Loading PrefixSet successful"));

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::LoadFromFile(nsIFile * aFile)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> file(do_QueryInterface(aFile, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  AutoFDClose fileFd;
  rv = file->OpenNSPRFileDesc(PR_RDONLY, 0, &fileFd);
  NS_ENSURE_SUCCESS(rv, rv);

  return LoadFromFd(fileFd);
}

nsresult
nsUrlClassifierPrefixSet::StoreToFd(AutoFDClose & fileFd)
{
  PRInt32 written;
  PRUint32 magic = PREFIXSET_VERSION_MAGIC;
  written = PR_Write(fileFd, &magic, sizeof(PRUint32));
  NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);

  written = PR_Write(fileFd, &mRandomKey, sizeof(PRUint32));
  NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);

  PRUint32 indexSize = mIndexStarts.Length();
  PRUint32 deltaSize = mDeltas.Length();
  written = PR_Write(fileFd, &indexSize, sizeof(PRUint32));
  NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);
  written = PR_Write(fileFd, &deltaSize, sizeof(PRUint32));
  NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);

  written = PR_Write(fileFd, mIndexPrefixes.Elements(), indexSize * sizeof(PRUint32));
  NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);
  written = PR_Write(fileFd, mIndexStarts.Elements(), indexSize * sizeof(PRUint32));
  NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);
  if (deltaSize > 0) {
    written = PR_Write(fileFd, mDeltas.Elements(), deltaSize * sizeof(PRUint16));
    NS_ENSURE_TRUE(written > 0, NS_ERROR_FAILURE);
  }

  LOG(("Saving PrefixSet successful\n"));

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierPrefixSet::StoreToFile(nsIFile * aFile)
{
  if (!mHasPrefixes) {
    LOG(("Attempt to serialize empty PrefixSet"));
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  nsCOMPtr<nsILocalFile> file(do_QueryInterface(aFile, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  AutoFDClose fileFd;
  rv = file->OpenNSPRFileDesc(PR_RDWR | PR_TRUNCATE | PR_CREATE_FILE,
                              0644, &fileFd);
  NS_ENSURE_SUCCESS(rv, rv);

  MutexAutoLock lock(mPrefixSetLock);

  return StoreToFd(fileFd);
}
