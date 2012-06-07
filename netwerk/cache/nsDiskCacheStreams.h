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

    nsresult    GetInputStream(PRUint32 offset, nsIInputStream ** inputStream);
    nsresult    GetOutputStream(PRUint32 offset, nsIOutputStream ** outputStream);

    nsresult    CloseOutputStream(nsDiskCacheOutputStream * outputStream);
    nsresult    CloseOutputStreamInternal(nsDiskCacheOutputStream * outputStream);
        
    nsresult    Write( const char * buffer,
                       PRUint32     count,
                       PRUint32 *   bytesWritten);

    nsresult    Seek(PRInt32 whence, PRInt32 offset);
    nsresult    Tell(PRUint32 * position);    
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


    void        Close();
    nsresult    OpenCacheFile(PRIntn flags, PRFileDesc ** fd);
    nsresult    ReadCacheBlocks();
    nsresult    FlushBufferToFile();
    void        UpdateFileSize();
    void        DeleteBuffer();
    nsresult    Flush();


    nsDiskCacheBinding *        mBinding;       // not an owning reference
    nsDiskCacheDevice *         mDevice;
    nsDiskCacheOutputStream *   mOutStream;     // not an owning reference
    PRInt32                     mInStreamCount;
    nsCOMPtr<nsIFile>           mLocalFile;
    PRFileDesc *                mFD;

    PRUint32                    mStreamPos;     // for Output Streams
    PRUint32                    mStreamEnd;
    PRUint32                    mBufPos;        // current mark in buffer
    PRUint32                    mBufEnd;        // current end of data in buffer
    PRUint32                    mBufSize;       // current end of buffer
    bool                        mBufDirty;
    char *                      mBuffer;
    
};

#endif // _nsDiskCacheStreams_h_
