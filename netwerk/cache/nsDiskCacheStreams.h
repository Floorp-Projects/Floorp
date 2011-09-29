/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsDiskCacheStreams.h, released
 * June 13, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan <gordon@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

    void        ClearBinding();
    
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
    nsCOMPtr<nsILocalFile>      mLocalFile;
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
