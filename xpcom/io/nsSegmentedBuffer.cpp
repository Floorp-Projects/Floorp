/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsSegmentedBuffer.h"
#include "nsCRT.h"

#ifdef DEBUG
#define DEBUG_MEMSET(addr, value, count) nsCRT::memset(addr, value, count)
#else
#define DEBUG_MEMSET(addr, value, count) /* nothing */
#endif

nsSegmentedBuffer::nsSegmentedBuffer(PRUint32 segmentSize, PRUint32 maxSize,
                                     nsIAllocator* allocator)
    : mSegmentSize(segmentSize), mMaxSize(maxSize), 
      mSegAllocator(allocator), mSegmentArray(nsnull),
      mSegmentArrayCount(NS_SEGMENTARRAY_INITIAL_COUNT),
      mFirstSegmentIndex(0), mLastSegmentIndex(0)
{
    if (mSegAllocator == nsnull) {
        mSegAllocator = nsAllocator::GetGlobalAllocator();
    }
    else {
        NS_ADDREF(mSegAllocator);
    }
}

nsSegmentedBuffer::~nsSegmentedBuffer()
{
    Empty();
    NS_RELEASE(mSegAllocator);
}

char*
nsSegmentedBuffer::AppendNewSegment()
{
    if (mSegmentArray == nsnull) {
        PRUint32 bytes = mSegmentArrayCount * sizeof(char*);
        mSegmentArray = (char**)nsAllocator::Alloc(bytes);
        if (mSegmentArray == nsnull)
            return nsnull;
        DEBUG_MEMSET(mSegmentArray, 0, bytes);
    }

    if (IsFull()) {
        PRUint32 newArraySize = mSegmentArrayCount * 2;
        PRUint32 bytes = newArraySize * sizeof(char*);
        if (bytes > mMaxSize)
            return nsnull;
        char** newSegArray = (char**)nsAllocator::Realloc(mSegmentArray, bytes);
        if (newSegArray == nsnull)
            return nsnull;
        mSegmentArray = newSegArray;
        // copy wrapped content to new extension
        if (mFirstSegmentIndex > mLastSegmentIndex) {
            // deal with wrap around case
            nsCRT::memcpy(&mSegmentArray[mSegmentArrayCount],
                          mSegmentArray,
                          mLastSegmentIndex * sizeof(char*));
            DEBUG_MEMSET(mSegmentArray, 0, mLastSegmentIndex * sizeof(char*));
            mLastSegmentIndex += mSegmentArrayCount;
            DEBUG_MEMSET(&mSegmentArray[mLastSegmentIndex],
                         0, (newArraySize - mLastSegmentIndex) * sizeof(char*));
        }
        mSegmentArrayCount = newArraySize;
    }

    char* seg = (char*)mSegAllocator->Alloc(mSegmentSize);
    if (seg == nsnull) {
        return nsnull;
    }
    mSegmentArray[mLastSegmentIndex] = seg;
    mLastSegmentIndex = ModSegArraySize(mLastSegmentIndex + 1);
    return seg;
}

PRBool
nsSegmentedBuffer::DeleteFirstSegment()
{
    NS_ASSERTION(mSegmentArray[mFirstSegmentIndex] != nsnull, "deleting bad segment");
    (void)mSegAllocator->Free(mSegmentArray[mFirstSegmentIndex]);
    mSegmentArray[mFirstSegmentIndex] = nsnull;
    PRInt32 last = ModSegArraySize(mLastSegmentIndex - 1);
    if (mFirstSegmentIndex == last) {
        mLastSegmentIndex = last;
        return PR_TRUE;
    }
    else {
        mFirstSegmentIndex = ModSegArraySize(mFirstSegmentIndex + 1);
        return PR_FALSE;
    }
}

void
nsSegmentedBuffer::Empty()
{
    if (mSegmentArray) {
        for (PRUint32 i = 0; i < mSegmentArrayCount; i++) {
            if (mSegmentArray[i])
                mSegAllocator->Free(mSegmentArray[i]);
        }
        nsAllocator::Free(mSegmentArray);
        mSegmentArray = nsnull;
    }
    mSegmentArrayCount = NS_SEGMENTARRAY_INITIAL_COUNT;
}

#ifdef DEBUG
NS_COM void
TestSegmentedBuffer()
{
    nsSegmentedBuffer* mgr = new nsSegmentedBuffer(4, 8);
    NS_ASSERTION(mgr, "out of memory");
    mgr->AppendNewSegment();
    mgr->AppendNewSegment();
    mgr->AppendNewSegment();
    mgr->DeleteFirstSegment();
    mgr->DeleteFirstSegment();
    mgr->AppendNewSegment();
    mgr->AppendNewSegment();
    mgr->AppendNewSegment();
    mgr->DeleteFirstSegment();
    delete mgr;
}
#endif

////////////////////////////////////////////////////////////////////////////////
