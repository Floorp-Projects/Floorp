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

nsSegmentedBuffer::nsSegmentedBuffer()
    : mSegmentSize(0), mMaxSize(0), 
      mSegAllocator(nsnull), mSegmentArray(nsnull),
      mSegmentArrayCount(0),
      mFirstSegmentIndex(0), mLastSegmentIndex(0)
{
}

nsSegmentedBuffer::~nsSegmentedBuffer()
{
    Empty();
    NS_IF_RELEASE(mSegAllocator);
}

nsresult
nsSegmentedBuffer::Init(PRUint32 segmentSize, PRUint32 maxSize,
                        nsIAllocator* allocator)
{
    if (mSegmentArrayCount != 0)
        return NS_ERROR_FAILURE;        // initialized more than once
    mSegmentSize = segmentSize;
    mMaxSize = maxSize;
    mSegAllocator = allocator;
    if (mSegAllocator == nsnull) {
        mSegAllocator = nsAllocator::GetGlobalAllocator();
    }
    else {
        NS_ADDREF(mSegAllocator);
    }
#if 0 // testing...
    mSegmentArrayCount = 2;
#else
    mSegmentArrayCount = NS_SEGMENTARRAY_INITIAL_COUNT;
#endif
    return NS_OK;
}

char*
nsSegmentedBuffer::AppendNewSegment()
{
    if (GetSize() >= mMaxSize)
        return nsnull;

    if (mSegmentArray == nsnull) {
        PRUint32 bytes = mSegmentArrayCount * sizeof(char*);
        mSegmentArray = (char**)nsAllocator::Alloc(bytes);
        if (mSegmentArray == nsnull)
            return nsnull;
        nsCRT::memset(mSegmentArray, 0, bytes);
    }
    
    if (IsFull()) {
        PRUint32 newArraySize = mSegmentArrayCount * 2;
        PRUint32 bytes = newArraySize * sizeof(char*);
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
            nsCRT::memset(mSegmentArray, 0, mLastSegmentIndex * sizeof(char*));
            mLastSegmentIndex += mSegmentArrayCount;
            nsCRT::memset(&mSegmentArray[mLastSegmentIndex], 0,
                          (newArraySize - mLastSegmentIndex) * sizeof(char*));
        }
        else {
            nsCRT::memset(&mSegmentArray[mLastSegmentIndex], 0,
                          (newArraySize - mLastSegmentIndex) * sizeof(char*));
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
    mFirstSegmentIndex = mLastSegmentIndex = 0;
}

#ifdef DEBUG
NS_COM void
TestSegmentedBuffer()
{
    nsSegmentedBuffer* buf = new nsSegmentedBuffer();
    NS_ASSERTION(buf, "out of memory");
    buf->Init(4, 16);
    char* seg;
    PRBool empty;
    seg = buf->AppendNewSegment();
    NS_ASSERTION(seg, "AppendNewSegment failed");
    seg = buf->AppendNewSegment();
    NS_ASSERTION(seg, "AppendNewSegment failed");
    seg = buf->AppendNewSegment();
    NS_ASSERTION(seg, "AppendNewSegment failed");
    empty = buf->DeleteFirstSegment();
    NS_ASSERTION(!empty, "DeleteFirstSegment failed");
    empty = buf->DeleteFirstSegment();
    NS_ASSERTION(!empty, "DeleteFirstSegment failed");
    seg = buf->AppendNewSegment();
    NS_ASSERTION(seg, "AppendNewSegment failed");
    seg = buf->AppendNewSegment();
    NS_ASSERTION(seg, "AppendNewSegment failed");
    seg = buf->AppendNewSegment();
    NS_ASSERTION(seg, "AppendNewSegment failed");
    empty = buf->DeleteFirstSegment();
    NS_ASSERTION(!empty, "DeleteFirstSegment failed");
    empty = buf->DeleteFirstSegment();
    NS_ASSERTION(!empty, "DeleteFirstSegment failed");
    empty = buf->DeleteFirstSegment();
    NS_ASSERTION(!empty, "DeleteFirstSegment failed");
    empty = buf->DeleteFirstSegment();
    NS_ASSERTION(empty, "DeleteFirstSegment failed");
    delete buf;
}
#endif

////////////////////////////////////////////////////////////////////////////////
