/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsBufferedStreams.h"

////////////////////////////////////////////////////////////////////////////////
// nsBufferedStream

nsBufferedStream::nsBufferedStream()
    : mBuffer(nsnull),
      mBufferStartOffset(0),
      mCursor(0)
{
    NS_INIT_REFCNT();
}

nsBufferedStream::~nsBufferedStream()
{
    Close();
}

NS_IMPL_ISUPPORTS2(nsBufferedStream, 
                   nsIBaseStream,
                   nsIRandomAccessStore);

nsresult
nsBufferedStream::Init(PRUint32 BufferedSize, nsIBaseStream* stream)
{
    NS_ASSERTION(stream, "need to supply a stream");
    NS_ASSERTION(mStream == nsnull, "already inited");
    mStream = stream;
    mBufferSize = BufferedSize;
    mBufferStartOffset = 0;
    mCursor = 0;
    mBuffer = new char[BufferedSize];
    if (mBuffer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedStream::Close()
{
    if (mStream) {
        return mStream->Close();
        mStream = nsnull;
        delete mBuffer;
        mBuffer = nsnull;
        mBufferSize = 0;
        mBufferStartOffset = 0;
        mCursor = 0;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mStream == nsnull)
        return NS_BASE_STREAM_CLOSED;
    
    if (whence == nsIRandomAccessStore::NS_SEEK_CUR &&
        mCursor + offset < mBufferStartOffset + mBufferSize) {
        mCursor += offset;
        return NS_OK;
    }
    // XXX more...
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedStream::Tell(PRInt32 *result)
{
    if (mStream == nsnull)
        return NS_BASE_STREAM_CLOSED;
    
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferedInputStream

NS_IMPL_ISUPPORTS_INHERITED1(nsBufferedInputStream, 
                             nsBufferedStream,
                             nsIInputStream);

NS_METHOD
nsBufferedInputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsBufferedInputStream* stream = new nsBufferedInputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsBufferedInputStream::Close()
{
    return nsBufferedStream::Close();
}

NS_IMETHODIMP
nsBufferedInputStream::Available(PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedInputStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferedOutputStream

NS_IMPL_ISUPPORTS_INHERITED1(nsBufferedOutputStream, 
                             nsBufferedStream,
                             nsIOutputStream);
 
NS_METHOD
nsBufferedOutputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsBufferedOutputStream* stream = new nsBufferedOutputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsBufferedOutputStream::Close()
{
    return nsBufferedStream::Close();
}

NS_IMETHODIMP
nsBufferedOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedOutputStream::Flush(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
