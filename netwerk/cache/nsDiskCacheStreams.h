/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsDiskCacheStreams_h_
#define _nsDiskCacheStreams_h_

#include "nsDiskCacheBinding.h"

#include "nsCache.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"

#include "pratom.h"

class nsDiskCacheInputStream;
class nsDiskCacheOutputStream;
class nsDiskCacheDevice;

class nsDiskCacheStreamIO : public nsISupports {
public:
             nsDiskCacheStreamIO(nsDiskCacheBinding *   binding);
    virtual ~nsDiskCacheStreamIO();
    
    NS_DECL_ISUPPORTS

    nsresult    GetInputStream(uint32_t offset, nsIInputStream ** inputStream);
    nsresult    GetOutputStream(uint32_t offset, nsIOutputStream ** outputStream);

    nsresult    CloseOutputStream(nsDiskCacheOutputStream * outputStream);

    nsresult    Write( const char * buffer,
                       uint32_t     count,
                       uint32_t *   bytesWritten);

    nsresult    ClearBinding();
    
    void        IncrementInputStreamCount() { PR_ATOMIC_INCREMENT(&mInStreamCount); }
    void        DecrementInputStreamCount()
                {
                    PR_ATOMIC_DECREMENT(&mInStreamCount);
                    NS_ASSERTION(mInStreamCount >= 0, "mInStreamCount has gone negative");
                }

    size_t     SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf);

    // GCC 2.95.2 requires this to be defined, although we never call it.
    // and OS/2 requires that it not be private
    nsDiskCacheStreamIO() { NS_NOTREACHED("oops"); }

private:

    void        Close();
    nsresult    OpenCacheFile(int flags, PRFileDesc ** fd);
    nsresult    ReadCacheBlocks(uint32_t bufferSize);
    nsresult    FlushBufferToFile();
    void        UpdateFileSize();
    void        DeleteBuffer();
    nsresult    Flush();
    nsresult    SeekAndTruncate(uint32_t offset);

    nsDiskCacheBinding *        mBinding;       // not an owning reference
    nsDiskCacheDevice *         mDevice;
    nsDiskCacheOutputStream *   mOutStream;     // not an owning reference
    int32_t                     mInStreamCount;
    PRFileDesc *                mFD;

    uint32_t                    mStreamEnd;     // current size of data
    uint32_t                    mBufSize;       // current end of buffer
    char *                      mBuffer;
};

#endif // _nsDiskCacheStreams_h_
