/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nscore.h"
#include "nsISizeOfHandler.h"
#include "nsIAtom.h"
#include "plhash.h"

class nsSizeOfHandler : public nsISizeOfHandler {
public:
  nsSizeOfHandler();
  virtual ~nsSizeOfHandler();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISizeOfHandler
  NS_IMETHOD Init();
  NS_IMETHOD RecordObject(void* aObject, PRBool* aResult);
  NS_IMETHOD AddSize(nsIAtom* aKey, PRUint32 aSize);
  NS_IMETHOD Report(nsISizeofReportFunc aFunc, void* aArg);
  NS_IMETHOD GetTotals(PRUint32* aCountResult, PRUint32* aTotalSizeResult);

protected:
  PRUint32 mTotalSize;
  PRUint32 mTotalCount;
  PLHashTable* mSizeTable;
  PLHashTable* mObjectTable;

  static PRIntn PR_CALLBACK RemoveObjectEntry(PLHashEntry* he, PRIntn i, void* arg);
  static PRIntn PR_CALLBACK RemoveSizeEntry(PLHashEntry* he, PRIntn i, void* arg);
  static PRIntn PR_CALLBACK ReportEntry(PLHashEntry* he, PRIntn i, void* arg);
};

class SizeOfDataStats {
public:
  SizeOfDataStats(nsIAtom* aType, PRUint32 aSize);
  ~SizeOfDataStats();

  void Update(PRUint32 aSize);

  nsIAtom* mType;               // type
  PRUint32 mCount;              // # of objects of this type
  PRUint32 mTotalSize;          // total size of all objects of this type
  PRUint32 mMinSize;            // smallest size for this type
  PRUint32 mMaxSize;            // largest size for this type
};

//----------------------------------------------------------------------

MOZ_DECL_CTOR_COUNTER(SizeOfDataStats)

SizeOfDataStats::SizeOfDataStats(nsIAtom* aType, PRUint32 aSize)
  : mType(aType),
    mCount(1),
    mTotalSize(aSize),
    mMinSize(aSize),
    mMaxSize(aSize)
{
  MOZ_COUNT_CTOR(SizeOfDataStats);
  NS_IF_ADDREF(mType);
}

SizeOfDataStats::~SizeOfDataStats()
{
  MOZ_COUNT_DTOR(SizeOfDataStats);
  NS_IF_RELEASE(mType);
}

void
SizeOfDataStats::Update(PRUint32 aSize)
{
  mCount++;
  if (aSize < mMinSize) mMinSize = aSize;
  if (aSize > mMaxSize) mMaxSize = aSize;
  mTotalSize += aSize;
}

//----------------------------------------------------------------------

#define POINTER_HASH_KEY(_atom) (PLHashNumber) NS_PTR_TO_INT32(_atom)

static PLHashNumber
PointerHashKey(nsIAtom* key)
{
  return POINTER_HASH_KEY(key);
}

static PRIntn
PointerCompareKeys(void* key1, void* key2)
{
  return key1 == key2;
}

nsSizeOfHandler::nsSizeOfHandler()
  : mTotalSize(0),
    mTotalCount(0)
{
  NS_INIT_REFCNT();
  mTotalSize = 0;
  mSizeTable = PL_NewHashTable(32, (PLHashFunction) PointerHashKey,
                               (PLHashComparator) PointerCompareKeys,
                               (PLHashComparator) nsnull,
                               nsnull, nsnull);
  mObjectTable = PL_NewHashTable(32, (PLHashFunction) PointerHashKey,
                                 (PLHashComparator) PointerCompareKeys,
                                 (PLHashComparator) nsnull,
                                 nsnull, nsnull);
}

PRIntn
nsSizeOfHandler::RemoveObjectEntry(PLHashEntry* he, PRIntn i, void* arg)
{
  // Remove and free this entry and continue enumerating
  return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
}

PRIntn
nsSizeOfHandler::RemoveSizeEntry(PLHashEntry* he, PRIntn i, void* arg)
{
  if (he->value) {
    SizeOfDataStats* stats = (SizeOfDataStats*) he->value;
    he->value = nsnull;
    delete stats;
  }

  // Remove and free this entry and continue enumerating
  return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
}

nsSizeOfHandler::~nsSizeOfHandler()
{
  if (nsnull != mObjectTable) {
    PL_HashTableEnumerateEntries(mObjectTable, RemoveObjectEntry, 0);
    PL_HashTableDestroy(mObjectTable);
  }
  if (nsnull != mSizeTable) {
    PL_HashTableEnumerateEntries(mSizeTable, RemoveSizeEntry, 0);
    PL_HashTableDestroy(mSizeTable);
  }
}

NS_IMPL_ISUPPORTS1(nsSizeOfHandler, nsISizeOfHandler)

NS_IMETHODIMP
nsSizeOfHandler::Init()
{
  if (mObjectTable) {
    PL_HashTableEnumerateEntries(mObjectTable, RemoveObjectEntry, 0); 
  }
  if (mSizeTable) {
    PL_HashTableEnumerateEntries(mSizeTable, RemoveSizeEntry, 0);
  }
  mTotalCount = 0;
  mTotalSize = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsSizeOfHandler::RecordObject(void* aAddr, PRBool* aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  PRBool result = PR_TRUE;
  if (mObjectTable && aAddr) {
    PLHashNumber hashCode = POINTER_HASH_KEY(aAddr);
    PLHashEntry** hep = PL_HashTableRawLookup(mObjectTable, hashCode, aAddr);
    PLHashEntry* he = *hep;
    if (!he) {
      // We've never seen it before. Add it to the table
      (void) PL_HashTableRawAdd(mObjectTable, hep, hashCode, aAddr, aAddr);
      result = PR_FALSE;
    }
  }

  *aResult = result;
  return NS_OK;
}

NS_IMETHODIMP
nsSizeOfHandler::AddSize(nsIAtom* aType, PRUint32 aSize)
{
  PLHashNumber hashCode = POINTER_HASH_KEY(aType);
  PLHashEntry** hep = PL_HashTableRawLookup(mSizeTable, hashCode, aType);
  PLHashEntry* he = *hep;
  if (he) {
    // Stats already exist
    SizeOfDataStats* stats = (SizeOfDataStats*) he->value;
    stats->Update(aSize);
  }
  else {
    // Make new stats for the new frame type
    SizeOfDataStats* newStats = new SizeOfDataStats(aType, aSize);
    PL_HashTableRawAdd(mSizeTable, hep, hashCode, aType, newStats);
  }
  mTotalCount++;
  mTotalSize += aSize;
  return NS_OK;
}

struct ReportArgs {
  nsISizeOfHandler* mHandler;
  nsISizeofReportFunc mFunc;
  void* mArg;
};

PRIntn
nsSizeOfHandler::ReportEntry(PLHashEntry* he, PRIntn i, void* arg)
{
  ReportArgs* ra = (ReportArgs*) arg;
  if (he && ra && ra->mFunc) {
    SizeOfDataStats* stats = (SizeOfDataStats*) he->value;
    if (stats) {
      (*ra->mFunc)(ra->mHandler, stats->mType, stats->mCount,
                   stats->mTotalSize, stats->mMinSize, stats->mMaxSize,
                   ra->mArg);
    }
  }
  return HT_ENUMERATE_NEXT;
}

NS_IMETHODIMP
nsSizeOfHandler::Report(nsISizeofReportFunc aFunc, void* aArg)
{
  ReportArgs ra;
  ra.mHandler = this;
  ra.mFunc = aFunc;
  ra.mArg = aArg;
  PL_HashTableEnumerateEntries(mSizeTable, ReportEntry, (void*) &ra);
  return NS_OK;
}

NS_IMETHODIMP
nsSizeOfHandler::GetTotals(PRUint32* aTotalCountResult,
                           PRUint32* aTotalSizeResult)
{
  if (!aTotalCountResult || !aTotalSizeResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aTotalCountResult = mTotalCount;
  *aTotalSizeResult = mTotalSize;
  return NS_OK;
}

NS_COM nsresult
NS_NewSizeOfHandler(nsISizeOfHandler** aInstancePtrResult)
{
  if (!aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsISizeOfHandler *it = new nsSizeOfHandler();
  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsISizeOfHandler), (void **) aInstancePtrResult);
}
