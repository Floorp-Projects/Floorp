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

#include "nsFileStreams.h"
#include "nsILocalFile.h"
#include "prerror.h"

////////////////////////////////////////////////////////////////////////////////
// nsFileStream

nsFileStream::nsFileStream()
    : mFD(nsnull)
{
    NS_INIT_REFCNT();
}

nsFileStream::~nsFileStream()
{
    Close();
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsFileStream, 
                              nsIBaseStream,
                              nsISeekableStream);

NS_IMETHODIMP
nsFileStream::Close()
{
    if (mFD) {
        PR_Close(mFD);
        mFD = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Seek(mFD, offset, (PRSeekWhence)whence);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileStream::Tell(PRUint32 *result)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Seek(mFD, 0, PR_SEEK_CUR);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    *result = cnt;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsFileInputStream

NS_IMPL_ISUPPORTS_INHERITED2(nsFileInputStream, 
                             nsFileStream,
                             nsIInputStream,
                             nsIFileInputStream);

NS_METHOD
nsFileInputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsFileInputStream* stream = new nsFileInputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsFileInputStream::Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm)
{
    NS_ASSERTION(mFD == nsnull, "already inited");
    if (mFD != nsnull)
        return NS_ERROR_FAILURE;
    nsresult rv;
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv)) return rv;
    if (ioFlags == -1)
        ioFlags = PR_RDONLY;
    if (perm == -1)
        perm = 0;
    return localFile->OpenNSPRFileDesc(ioFlags, perm, &mFD);
}

NS_IMETHODIMP
nsFileInputStream::Close()
{
    return nsFileStream::Close();
}

NS_IMETHODIMP
nsFileInputStream::Available(PRUint32 *result)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 avail = PR_Available(mFD);
    if (avail == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    *result = avail;
    return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Read(mFD, buf, count);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    *result = cnt;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsFileOutputStream

NS_IMPL_ISUPPORTS_INHERITED2(nsFileOutputStream, 
                             nsFileStream,
                             nsIOutputStream,
                             nsIFileOutputStream);
 
NS_METHOD
nsFileOutputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsFileOutputStream* stream = new nsFileOutputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsFileOutputStream::Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm)
{
    NS_ASSERTION(mFD == nsnull, "already inited");
    if (mFD != nsnull)
        return NS_ERROR_FAILURE;
    nsresult rv;
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv)) return rv;
    if (ioFlags == -1)
        ioFlags = PR_WRONLY | PR_CREATE_FILE;
    if (perm == -1)
        perm = 0664;
    return localFile->OpenNSPRFileDesc(ioFlags, perm, &mFD);
}

NS_IMETHODIMP
nsFileOutputStream::Close()
{
    return nsFileStream::Close();
}

NS_IMETHODIMP
nsFileOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Write(mFD, buf, count);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    *result = cnt;
    return NS_OK;
}

NS_IMETHODIMP
nsFileOutputStream::Flush(void)
{
    if (mFD == nsnull)
        return NS_BASE_STREAM_CLOSED;

    PRInt32 cnt = PR_Sync(mFD);
    if (cnt == -1) {
        return NS_ErrorAccordingToNSPR();
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
