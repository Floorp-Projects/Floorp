/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The storage stream provides an internal buffer that can be filled by a
 * client using a single output stream.  One or more independent input streams
 * can be created to read the data out non-destructively.  The implementation
 * uses a segmented buffer internally to avoid realloc'ing of large buffers,
 * with the attendant performance loss and heap fragmentation.
 */

#ifndef _nsStorageStream_h_
#define _nsStorageStream_h_

#include "nsIStorageStream.h"
#include "nsIOutputStream.h"
#include "nsMemory.h"
#include "mozilla/Attributes.h"

#define NS_STORAGESTREAM_CID                       \
{ /* 669a9795-6ff7-4ed4-9150-c34ce2971b63 */       \
  0x669a9795,                                      \
  0x6ff7,                                          \
  0x4ed4,                                          \
  {0x91, 0x50, 0xc3, 0x4c, 0xe2, 0x97, 0x1b, 0x63} \
}

#define NS_STORAGESTREAM_CONTRACTID "@mozilla.org/storagestream;1"

class nsSegmentedBuffer;

class nsStorageStream MOZ_FINAL : public nsIStorageStream,
                                  public nsIOutputStream
{
public:
    nsStorageStream();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTORAGESTREAM
    NS_DECL_NSIOUTPUTSTREAM

    friend class nsStorageInputStream;

private:
    ~nsStorageStream();

    nsSegmentedBuffer* mSegmentedBuffer;
    uint32_t           mSegmentSize;       // All segments, except possibly the last, are of this size
                                           //   Must be power-of-2
    uint32_t           mSegmentSizeLog2;   // log2(mSegmentSize)
    bool               mWriteInProgress;   // true, if an un-Close'ed output stream exists
    int32_t            mLastSegmentNum;    // Last segment # in use, -1 initially
    char*              mWriteCursor;       // Pointer to next byte to be written
    char*              mSegmentEnd;        // Pointer to one byte after end of segment
                                           //   containing the write cursor
    uint32_t           mLogicalLength;     // Number of bytes written to stream

    NS_METHOD Seek(int32_t aPosition);
    uint32_t SegNum(uint32_t aPosition)    {return aPosition >> mSegmentSizeLog2;}
    uint32_t SegOffset(uint32_t aPosition) {return aPosition & (mSegmentSize - 1);}
};

#endif //  _nsStorageStream_h_
