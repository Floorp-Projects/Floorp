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
 * The Original Code is nsANSIFileStreams.cpp, released March 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#include "nsANSIFileStreams.h"

NS_IMPL_ISUPPORTS2(nsANSIInputStream, nsIInputStream, nsISeekableStream);

nsANSIInputStream::nsANSIInputStream() : mFile(nsnull), mSize(0)
{
    NS_INIT_ISUPPORTS();
}

nsANSIInputStream::~nsANSIInputStream()
{
    Close();
}

nsresult nsANSIInputStream::Open(nsILocalFile* file)
{
    nsresult rv;
    rv = file->OpenANSIFileDesc("rb", &mFile);
    if (NS_FAILED(rv)) return rv;

    if (::fseek(mFile, 0, SEEK_END) != 0) return NS_ERROR_FAILURE;
    mSize = ::ftell(mFile);
    ::fseek(mFile, 0, SEEK_SET);
    rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
    return rv;
}

NS_IMETHODIMP nsANSIInputStream::Close()
{
    if (mFile) {
        ::fclose(mFile);
        mFile = nsnull;
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIInputStream::Available(PRUint32 * result)
{
    if (mFile) {
        *result = (mSize - ::ftell(mFile));
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIInputStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
    if (mFile) {
        *result = ::fread(buf, 1, count, mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsANSIInputStream::GetObserver(nsIInputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIInputStream::SetObserver(nsIInputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIInputStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mFile) {
        ::fseek(mFile, offset, whence);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIInputStream::Tell(PRUint32 * result)
{
    if (mFile) {
        *result = ::ftell(mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMPL_ISUPPORTS2(nsANSIOutputStream, nsIOutputStream, nsISeekableStream);

nsANSIOutputStream::nsANSIOutputStream() : mFile(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsANSIOutputStream::~nsANSIOutputStream()
{
    Close();
}

nsresult nsANSIOutputStream::Open(nsILocalFile* file)
{
    nsresult rv = file->OpenANSIFileDesc("wb", &mFile);
    return rv;
}

NS_IMETHODIMP nsANSIOutputStream::Close()
{
    if (mFile) {
        ::fclose(mFile);
        mFile = nsnull;
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIOutputStream::Flush()
{
    if (mFile) {
        ::fflush(mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIOutputStream::Write(const char *buffer, PRUint32 count, PRUint32 *result)
{
    if (mFile) {
        *result = ::fwrite(buffer, 1, count, mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIOutputStream::WriteFrom(nsIInputStream *input, PRUint32 count, PRUint32 *actualCount)
{
    nsresult rv;
    char buffer[BUFSIZ];
    PRUint32 totalCount = count;
    *actualCount = 0;
    while (totalCount > 0) {
        count = (totalCount < BUFSIZ ? totalCount : BUFSIZ);
        rv = input->Read(buffer, count, &count);
        if (NS_FAILED(rv)) break;
        rv = Write(buffer, count, &count);
        if (NS_FAILED(rv)) break;
        totalCount -= count;
        *actualCount += count;
    }
    return rv;
}

NS_IMETHODIMP nsANSIOutputStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}
NS_IMETHODIMP nsANSIOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIOutputStream::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsANSIOutputStream::SetObserver(nsIOutputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIOutputStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mFile) {
        ::fseek(mFile, offset, whence);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIOutputStream::Tell(PRUint32 * result)
{
    if (mFile) {
        *result = ::ftell(mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

///////

NS_IMPL_ISUPPORTS3(nsANSIFileStream, nsIInputStream, nsIOutputStream, nsISeekableStream);

nsANSIFileStream::nsANSIFileStream() : mFile(nsnull), mSize(0)
{
    NS_INIT_ISUPPORTS();
}

nsANSIFileStream::~nsANSIFileStream()
{
    Close();
}

nsresult nsANSIFileStream::Open(nsILocalFile* file)
{
    nsresult rv;
    rv = file->OpenANSIFileDesc("rb+", &mFile);
    if (NS_FAILED(rv)) {
        rv = rv = file->OpenANSIFileDesc("wb+", &mFile);
        if (NS_FAILED(rv)) return rv;
    }
    
    // compute size of file.
    if (::fseek(mFile, 0, SEEK_END) != 0) return NS_ERROR_FAILURE;
    mSize = ::ftell(mFile);
    ::fseek(mFile, 0, SEEK_SET);
    rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
    return rv;
}

NS_IMETHODIMP nsANSIFileStream::Close()
{
    if (mFile) {
        ::fclose(mFile);
        mFile = nsnull;
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIFileStream::Available(PRUint32 * result)
{
    if (mFile) {
        *result = (mSize - ::ftell(mFile));
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIFileStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
    if (mFile) {
        *result = ::fread(buf, 1, count, mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIFileStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsANSIFileStream::GetObserver(nsIInputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::SetObserver(nsIInputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::Flush()
{
    if (mFile) {
        ::fflush(mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIFileStream::Write(const char *buffer, PRUint32 count, PRUint32 *result)
{
    if (mFile) {
        *result = ::fwrite(buffer, 1, count, mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIFileStream::WriteFrom(nsIInputStream *input, PRUint32 count, PRUint32 *actualCount)
{
    char buffer[BUFSIZ];
    PRUint32 totalCount = count;
    *actualCount = 0;
    while (totalCount > 0) {
        count = (totalCount < BUFSIZ ? totalCount : BUFSIZ);
        nsresult rv = input->Read(buffer, count, &count);
        if (NS_FAILED(rv)) return rv;
        rv = Write(buffer, count, &count);
        totalCount -= count;
        *actualCount += count;
    }
    return NS_OK;
}

NS_IMETHODIMP nsANSIFileStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::SetNonBlocking(PRBool aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::SetObserver(nsIOutputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsANSIFileStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mFile) {
        ::fseek(mFile, offset, whence);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}

NS_IMETHODIMP nsANSIFileStream::Tell(PRUint32 * result)
{
    if (mFile) {
        *result = ::ftell(mFile);
        nsresult rv = (ferror(mFile) ? NS_ERROR_FAILURE : NS_OK);
        return rv;
    }
    return NS_BASE_STREAM_CLOSED;
}
