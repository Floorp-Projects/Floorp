/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
    if (mSegAllocator == nsnull) {
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
        return nsnull;

    if (mSegmentArray == nsnull) {
        PRUint32 bytes = mSegmentArrayCount * sizeof(char*);
        mSegmentArray = (char**)nsMemory::Alloc(bytes);
        if (mSegmentArray == nsnull)
            return nsnull;
        memset(mSegmentArray, 0, bytes);
    }
    
    if (IsFull()) {
        PRUint32 newArraySize = mSegmentArrayCount * 2;
        PRUint32 bytes = newArraySize * sizeof(char*);
        char** newSegArray = (char**)nsMemory::Realloc(mSegmentArray, bytes);
        if (newSegArray == nsnull)
            return nsnull;
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

PRBool
nsSegmentedBuffer::DeleteLastSegment()
{
    PRInt32 last = ModSegArraySize(mLastSegmentIndex - 1);
    NS_ASSERTION(mSegmentArray[last] != nsnull, "deleting bad segment");
    (void)mSegAllocator->Free(mSegmentArray[last]);
    mSegmentArray[last] = nsnull;
    mLastSegmentIndex = last;
    return (PRBool)(mLastSegmentIndex == mFirstSegmentIndex);
}

PRBool
nsSegmentedBuffer::ReallocLastSegment(size_t newSize)
{
    PRInt32 last = ModSegArraySize(mLastSegmentIndex - 1);
    NS_ASSERTION(mSegmentArray[last] != nsnull, "realloc'ing bad segment");
    char *newSegment =
        (char*)mSegAllocator->Realloc(mSegmentArray[last], newSize);
    if (newSegment) {
        mSegmentArray[last] = newSegment;
        return PR_TRUE;
    } else {
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
        nsMemory::Free(mSegmentArray);
        mSegmentArray = nsnull;
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
