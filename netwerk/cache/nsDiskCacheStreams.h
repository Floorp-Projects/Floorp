/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsDiskCacheStreams_h_
#define _nsDiskCacheStreams_h_

#include "mozilla/MemoryReporting.h"
#include "nsDiskCacheBinding.h"

#include "nsCache.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"

#include "mozilla/Atomics.h"

class nsDiskCacheInputStream;
class nsDiskCacheDevice;

class nsDiskCacheStreamIO : public nsIOutputStream {
public:
    nsDiskCacheStreamIO(nsDiskCacheBinding *   binding);
    
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM

    nsresult    GetInputStream(uint32_t offset, nsIInputStream ** inputStream);
    nsresult    GetOutputStream(uint32_t offset, nsIOutputStream ** outputStream);

    nsresult    ClearBinding();
    
    void        IncrementInputStreamCount() { mInStreamCount++; }
    void        DecrementInputStreamCount()
                {
                    mInStreamCount--;
                    NS_ASSERTION(mInStreamCount >= 0, "mInStreamCount has gone negative");
                }

    size_t     SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

    // GCC 2.95.2 requires this to be defined, although we never call it.
    // and OS/2 requires that it not be private
    nsDiskCacheStreamIO() { NS_NOTREACHED("oops"); }

private:
    virtual ~nsDiskCacheStreamIO();

    nsresult    OpenCacheFile(int flags, PRFileDesc ** fd);
    nsresult    ReadCacheBlocks(uint32_t bufferSize);
    nsresult    FlushBufferToFile();
    void        UpdateFileSize();
    void        DeleteBuffer();
    nsresult    CloseOutputStream();
    nsresult    SeekAndTruncate(uint32_t offset);

    nsDiskCacheBinding *        mBinding;       // not an owning reference
    nsDiskCacheDevice *         mDevice;
    mozilla::Atomic<int32_t>                     mInStreamCount;
    PRFileDesc *                mFD;

    uint32_t                    mStreamEnd;     // current size of data
    uint32_t                    mBufSize;       // current end of buffer
    char *                      mBuffer;
    bool                        mOutputStreamIsOpen;
};

#endif // _nsDiskCacheStreams_h_
