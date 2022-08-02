/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
#include "mozilla/Mutex.h"

#define NS_STORAGESTREAM_CID                         \
  { /* 669a9795-6ff7-4ed4-9150-c34ce2971b63 */       \
    0x669a9795, 0x6ff7, 0x4ed4, {                    \
      0x91, 0x50, 0xc3, 0x4c, 0xe2, 0x97, 0x1b, 0x63 \
    }                                                \
  }

#define NS_STORAGESTREAM_CONTRACTID "@mozilla.org/storagestream;1"

class nsSegmentedBuffer;

class nsStorageStream final : public nsIStorageStream, public nsIOutputStream {
 public:
  nsStorageStream();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTORAGESTREAM
  NS_DECL_NSIOUTPUTSTREAM

  friend class nsStorageInputStream;

 private:
  ~nsStorageStream();

  mozilla::Mutex mMutex{"nsStorageStream"};
  nsSegmentedBuffer* mSegmentedBuffer MOZ_GUARDED_BY(mMutex) = nullptr;
  // All segments, except possibly the last, are of this size.  Must be
  // power-of-2
  uint32_t mSegmentSize MOZ_GUARDED_BY(mMutex) = 0;
  // log2(mSegmentSize)
  uint32_t mSegmentSizeLog2 MOZ_GUARDED_BY(mMutex) = 0;
  // true, if an un-Close'ed output stream exists
  bool mWriteInProgress MOZ_GUARDED_BY(mMutex) = false;
  // Last segment # in use, -1 initially
  int32_t mLastSegmentNum MOZ_GUARDED_BY(mMutex) = -1;
  // Pointer to next byte to be written
  char* mWriteCursor MOZ_GUARDED_BY(mMutex) = nullptr;
  // Pointer to one byte after end of segment containing the write cursor
  char* mSegmentEnd MOZ_GUARDED_BY(mMutex) = nullptr;
  // Number of bytes written to stream
  uint32_t mLogicalLength MOZ_GUARDED_BY(mMutex) = 0;
  // number of input streams actively reading a segment.
  uint32_t mActiveSegmentBorrows MOZ_GUARDED_BY(mMutex) = 0;

  nsresult SetLengthLocked(uint32_t aLength) MOZ_REQUIRES(mMutex);
  nsresult Seek(int32_t aPosition) MOZ_REQUIRES(mMutex);
  uint32_t SegNum(uint32_t aPosition) MOZ_REQUIRES(mMutex) {
    return aPosition >> mSegmentSizeLog2;
  }
  uint32_t SegOffset(uint32_t aPosition) MOZ_REQUIRES(mMutex) {
    return aPosition & (mSegmentSize - 1);
  }
};

#endif  //  _nsStorageStream_h_
