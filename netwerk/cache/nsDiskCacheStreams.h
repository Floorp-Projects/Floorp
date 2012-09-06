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
#include "nsIDiskCacheStreamInternal.h"

#include "pratom.h"

class nsDiskCacheInputStream;
class nsDiskCacheDevice;

class nsDiskCacheStreamIO : public nsIOutputStream, nsIDiskCacheStreamInternal {
public:
             nsDiskCacheStreamIO(nsDiskCacheBinding *   binding);
    virtual ~nsDiskCacheStreamIO();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIDISKCACHESTREAMINTERNAL

    nsresult    GetInputStream(uint32_t offset, nsIInputStream ** inputStream);
    nsresult    GetOutputStream(uint32_t offset, nsIOutputStream ** outputStream);

    nsresult    Seek(int32_t whence, int32_t offset);
    nsresult    Tell(uint32_t * position);    
    nsresult    SetEOF();

    nsresult    ClearBinding();
    
    void        IncrementInputStreamCount() { PR_ATOMIC_INCREMENT(&mInStreamCount); }
    void        DecrementInputStreamCount()
                {
                    PR_ATOMIC_DECREMENT(&mInStreamCount);
                    NS_ASSERTION(mInStreamCount >= 0, "mInStreamCount has gone negative");
                }

    // GCC 2.95.2 requires this to be defined, although we never call it.
    // and OS/2 requires that it not be private
    nsDiskCacheStreamIO() { NS_NOTREACHED("oops"); }
private:
    nsresult    OpenCacheFile(int flags, PRFileDesc ** fd);
    nsresult    ReadCacheBlocks();
    nsresult    FlushBufferToFile();
    void        UpdateFileSize();
    void        DeleteBuffer();

    nsDiskCacheBinding *        mBinding;       // not an owning reference
    nsDiskCacheDevice *         mDevice;
    int32_t                     mInStreamCount;
    nsCOMPtr<nsIFile>           mLocalFile;
    PRFileDesc *                mFD;

    uint32_t                    mStreamPos;     // for Output Streams
    uint32_t                    mStreamEnd;
    uint32_t                    mBufPos;        // current mark in buffer
    uint32_t                    mBufEnd;        // current end of data in buffer
    uint32_t                    mBufSize;       // current end of buffer
    bool                        mBufDirty;      // Where there is unflushed data in the buffer
    bool                        mOutputStreamIsOpen; // Whether the output stream is open (for writing...)
    char *                      mBuffer;
    
};

#endif // _nsDiskCacheStreams_h_
