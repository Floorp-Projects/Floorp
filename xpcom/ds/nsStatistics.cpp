/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Original Author:
 *   Chris Waterson <waterson@netscape.com>
 */

#include "nscore.h"
#include "nsStatistics.h"
#include "nsISupportsUtils.h"
#include "nsTraceRefcntImpl.h" // for NS_MeanAndStdDev
#include "plhash.h"

inline PLHashNumber
nsStatistics::HashPRInt32(const void* aKey)
{
    return PLHashNumber(NS_PTR_TO_INT32(aKey));
}

nsStatistics::nsStatistics(const char* aTopic)
    : mTopic(aTopic),
      mDistribution(nsnull),
      mCount(0),
      mMinimum(0),
      mMaximum(0),
      mSum(0),
      mSumOfSquares(0)
{
    mDistribution = PL_NewHashTable(24, HashPRInt32, PL_CompareValues, PL_CompareValues, nsnull, nsnull);
}

nsStatistics::~nsStatistics()
{
    PL_HashTableDestroy(mDistribution);
}

void
nsStatistics::Clear()
{
    PL_HashTableDestroy(mDistribution);
    mCount = mMinimum = mMaximum = 0;
    mSum = mSumOfSquares = 0.0;
    mDistribution = PL_NewHashTable(24, HashPRInt32, PL_CompareValues, PL_CompareValues, nsnull, nsnull);
}


void
nsStatistics::Record(PRInt32 aValue)
{
    ++mCount;
    if (aValue < mMinimum)
        mMinimum = aValue;
    if (aValue > mMaximum)
        mMaximum = aValue;
    mSum += aValue;
    mSumOfSquares += aValue * aValue;

    PLHashEntry** hep = PL_HashTableRawLookup(mDistribution, PLHashNumber(aValue),
                                              NS_REINTERPRET_CAST(const void*, aValue));

    if (hep && *hep) {
        PRInt32 count = NS_PTR_TO_INT32((*hep)->value);
        (*hep)->value = NS_REINTERPRET_CAST(void*, ++count);
    }
    else {
        PL_HashTableRawAdd(mDistribution, hep, PLHashNumber(aValue),
                           NS_REINTERPRET_CAST(const void*, aValue),
                           NS_REINTERPRET_CAST(void*, 1));
    }
}


void
nsStatistics::Print(FILE* aFile)
{
    double mean, stddev;
    NS_MeanAndStdDev(mCount, mSum, mSumOfSquares, &mean, &stddev);
    fprintf(aFile, "%s count=%d, minimum=%d, maximum=%d, mean=%0.2f+/-%0.2f\n",
            mTopic, mCount, mMinimum, mMaximum, mean, stddev);

    for (PRInt32 i = mMinimum; i <= mMaximum; ++i) {
        PRUint32 count = NS_PTR_TO_INT32(PL_HashTableLookup(mDistribution, NS_INT32_TO_PTR(i)));
        if (! count)
            continue;

        fprintf(aFile, "  %d: %d\n", i, count);
    }
}
