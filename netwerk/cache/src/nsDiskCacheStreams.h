/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsDiskCacheStreams.h, released June 13, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan <gordon@netscape.com>
 */


#ifndef _nsDiskCacheStreams_h_
#define _nsDiskCacheStreams_h_

#include "nsDiskCacheBinding.h"

#include "nsIStreamIO.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

class nsDiskCacheInputStream;
class nsDiskCacheOutputStream;
class nsDiskCacheDevice;

class nsDiskCacheStreamIO : public nsIStreamIO {


// XXX we're implementing nsIStreamIO to leverage the AsyncRead on the FileTransport thread

public:
             nsDiskCacheStreamIO(nsDiskCacheBinding *   binding);
    virtual ~nsDiskCacheStreamIO();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMIO
    
    // Create(nsDiskCacheBinding );
    
    void        CloseInputStream(nsDiskCacheInputStream *  inputStream);
    void        CloseOutputStream(nsDiskCacheOutputStream *  outputStream);
        
    nsresult    Write( const char * buffer,
                       PRUint32     count,
                       PRUint32 *   bytesWritten);

    nsresult    Seek(PRInt32 whence, PRInt32 offset);
    nsresult    Tell(PRUint32 * position);    
    nsresult    SetEOF();

    PRBool      EnsureLocalFile();
    nsresult    OpenCacheFile(PRIntn flags, PRFileDesc ** fd);
    nsresult    ReadCacheBlocks();
    nsresult    FlushBufferToFile(PRBool  clearBuffer);
    PRUint32    WriteToBuffer(const char * buffer, PRUint32 count);

private:

    nsDiskCacheBinding *        mBinding;
    nsDiskCacheDevice *         mDevice;
    nsDiskCacheOutputStream *   mOutStream;
    PRCList                     mInStreamQ;
    nsCOMPtr<nsILocalFile>      mLocalFile;
    PRFileDesc *                mFD;

    PRUint32                    mStreamPos;     // for Output Streams
    PRUint32                    mStreamEnd;
    PRUint32                    mBufPos;        // current mark in buffer
    PRUint32                    mBufEnd;        // current end of data in buffer
    PRUint32                    mBufSize;       // current end of buffer
    PRUint32                    mBufOffset;     // stream position of buffer start
    PRBool                      mBufDirty;
    char *                      mBuffer;
    
};

#endif // _nsDiskCacheStreams_h_
