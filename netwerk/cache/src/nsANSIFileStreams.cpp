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

NS_IMPL_ISUPPORTS1(nsANSIInputStream, nsIInputStream);

nsANSIInputStream::nsANSIInputStream(FILE* file) : mFile(file)
{
    NS_INIT_ISUPPORTS();
}

nsANSIInputStream::~nsANSIInputStream()
{
    Close();
}

/* void close (); */
NS_IMETHODIMP nsANSIInputStream::Close()
{
    if (mFile) {
        ::fclose(mFile);
        mFile = nsnull;
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

/* unsigned long available (); */
NS_IMETHODIMP nsANSIInputStream::Available(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] unsigned long read (in charPtr buf, in unsigned long count); */
NS_IMETHODIMP nsANSIInputStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
    if (mFile) {
        *result = ::fread(buf, 1, count, mFile);
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

/* [noscript] unsigned long readSegments (in nsWriteSegmentFun writer, in voidPtr closure, in unsigned long count); */
NS_IMETHODIMP nsANSIInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean nonBlocking; */
NS_IMETHODIMP nsANSIInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

/* attribute nsIInputStreamObserver observer; */
NS_IMETHODIMP nsANSIInputStream::GetObserver(nsIInputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsANSIInputStream::SetObserver(nsIInputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS1(nsANSIOutputStream, nsIOutputStream);

nsANSIOutputStream::nsANSIOutputStream(FILE* file) : mFile(file)
{
    NS_INIT_ISUPPORTS();
}

nsANSIOutputStream::~nsANSIOutputStream()
{
    Close();
}

/* void close (); */
NS_IMETHODIMP nsANSIOutputStream::Close()
{
    if (mFile) {
        ::fclose(mFile);
        mFile = nsnull;
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

/* void flush (); */
NS_IMETHODIMP nsANSIOutputStream::Flush()
{
    if (mFile) {
        ::fflush(mFile);
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

/* unsigned long write (in string buf, in unsigned long count); */
NS_IMETHODIMP nsANSIOutputStream::Write(const char *buffer, PRUint32 count, PRUint32 *result)
{
    if (mFile) {
        *result = ::fwrite(buffer, 1, count, mFile);
        return NS_OK;
    }
    return NS_BASE_STREAM_CLOSED;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsANSIOutputStream::WriteFrom(nsIInputStream *input, PRUint32 count, PRUint32 *actualCount)
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

/* [noscript] unsigned long writeSegments (in nsReadSegmentFun reader, in voidPtr closure, in unsigned long count); */
NS_IMETHODIMP nsANSIOutputStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean nonBlocking; */
NS_IMETHODIMP nsANSIOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
        *aNonBlocking = PR_FALSE;
        return NS_OK;
}
NS_IMETHODIMP nsANSIOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIOutputStreamObserver observer; */
NS_IMETHODIMP nsANSIOutputStream::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsANSIOutputStream::SetObserver(nsIOutputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
