/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSegmentedBuffer.h"
#include "nsCRT.h"

nsresult
nsSegmentedBuffer::Init(PRUint32 segmentSize, PRUint32 maxSize,
                        nsIMemory* allocator)
{
    if (mSegmentArrayCount != 0)
        return NS_ERROR_FAILURE;        // initialized more than once
    mSegmentSize = segmentSize;
    mMaxSize = maxSize;
    mSegAllocator = allocator;
    if (mSegAllocator == nullptr) {
        mSegAllocator = nsMemory::GetGlobalMemoryService();
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
        return nullptr;

    if (mSegmentArray == nullptr) {
        PRUint32 bytes = mSegmentArrayCount * sizeof(char*);
        mSegmentArray = (char**)nsMemory::Alloc(bytes);
        if (mSegmentArray == nullptr)
            return nullptr;
        memset(mSegmentArray, 0, bytes);
    }
    
    if (IsFull()) {
        PRUint32 newArraySize = mSegmentArrayCount * 2;
        PRUint32 bytes = newArraySize * sizeof(char*);
        char** newSegArray = (char**)nsMemory::Realloc(mSegmentArray, bytes);
        if (newSegArray == nullptr)
            return nullptr;
        mSegmentArray = newSegArray;
        // copy wrapped content to new extension
        if (mFirstSegmentIndex > mLastSegmentIndex) {
            // deal with wrap around case
            memcpy(&mSegmentArray[mSegmentArrayCount],
                   mSegmentArray,
                   mLastSegmentIndex * sizeof(char*));
            memset(mSegmentArray, 0, mLastSegmentIndex * sizeof(char*));
            mLastSegmentIndex += mSegmentArrayCount;
            memset(&mSegmentArray[mLastSegmentIndex], 0,
                   (newArraySize - mLastSegmentIndex) * sizeof(char*));
        }
        else {
            memset(&mSegmentArray[mLastSegmentIndex], 0,
                   (newArraySize - mLastSegmentIndex) * sizeof(char*));
        }
        mSegmentArrayCount = newArraySize;
    }

    char* seg = (char*)mSegAllocator->Alloc(mSegmentSize);
    if (seg == nullptr) {
        return nullptr;
    }
    mSegmentArray[mLastSegmentIndex] = seg;
    mLastSegmentIndex = ModSegArraySize(mLastSegmentIndex + 1);
    return seg;
}

bool
nsSegmentedBuffer::DeleteFirstSegment()
{
    NS_ASSERTION(mSegmentArray[mFirstSegmentIndex] != nullptr, "deleting bad segment");
    (void)mSegAllocator->Free(mSegmentArray[mFirstSegmentIndex]);
    mSegmentArray[mFirstSegmentIndex] = nullptr;
    PRInt32 last = ModSegArraySize(mLastSegmentIndex - 1);
    if (mFirstSegmentIndex == last) {
        mLastSegmentIndex = last;
        return true;
    }
    else {
        mFirstSegmentIndex = ModSegArraySize(mFirstSegmentIndex + 1);
        return false;
    }
}

bool
nsSegmentedBuffer::DeleteLastSegment()
{
    PRInt32 last = ModSegArraySize(mLastSegmentIndex - 1);
    NS_ASSERTION(mSegmentArray[last] != nullptr, "deleting bad segment");
    (void)mSegAllocator->Free(mSegmentArray[last]);
    mSegmentArray[last] = nullptr;
    mLastSegmentIndex = last;
    return (bool)(mLastSegmentIndex == mFirstSegmentIndex);
}

bool
nsSegmentedBuffer::ReallocLastSegment(size_t newSize)
{
    PRInt32 last = ModSegArraySize(mLastSegmentIndex - 1);
    NS_ASSERTION(mSegmentArray[last] != nullptr, "realloc'ing bad segment");
    char *newSegment =
        (char*)mSegAllocator->Realloc(mSegmentArray[last], newSize);
    if (newSegment) {
        mSegmentArray[last] = newSegment;
        return true;
    } else {
        return false;
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
        nsMemory::Free(mSegmentArray);
        mSegmentArray = nullptr;
    }
    mSegmentArrayCount = NS_SEGMENTARRAY_INITIAL_COUNT;
    mFirstSegmentIndex = mLastSegmentIndex = 0;
}

#if 0
void
TestSegmentedBuffer()
{
    nsSegmentedBuffer* buf = new nsSegmentedBuffer();
    NS_ASSERTION(buf, "out of memory");
    buf->Init(4, 16);
    char* seg;
    bool empty;
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
