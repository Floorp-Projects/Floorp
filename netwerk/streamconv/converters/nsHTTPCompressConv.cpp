/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 cindent et: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Dick <ddick@cpan.org>
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

#include "nsHTTPCompressConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "prlog.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIByteArrayInputStream.h"
#include "nsIStringStream.h"

static NS_METHOD
DiscardSegments(nsIInputStream *input,
                void *closure,
                const char *buf,
                PRUint32 offset,
                PRUint32 count,
                PRUint32 *countRead)
{
    *countRead = count;
    return NS_OK;
}

// nsISupports implementation
NS_IMPL_THREADSAFE_ISUPPORTS2(nsHTTPCompressConv, nsIStreamConverter, nsIStreamListener)

// nsFTPDirListingConv methods
nsHTTPCompressConv::nsHTTPCompressConv()
    : mListener(nsnull)
    , mMode(HTTP_COMPRESS_IDENTITY)
    , mOutBuffer(NULL)
    , mInpBuffer(NULL)
    , mOutBufferLen(0)
    , mInpBufferLen(0)
    , mCheckHeaderDone(PR_FALSE)
    , mStreamEnded(PR_FALSE)
    , mStreamInitialized(PR_FALSE)
    , mLen(0)
    , hMode(0)
    , mSkipCount(0)
    , mFlags(0)
{
}

nsHTTPCompressConv::~nsHTTPCompressConv()
{
    NS_IF_RELEASE(mListener);

    if (mInpBuffer)
        nsMemory::Free(mInpBuffer);

    if (mOutBuffer)
        nsMemory::Free(mOutBuffer);

    // For some reason we are not getting Z_STREAM_END.  But this was also seen
    //    for mozilla bug 198133.  Need to handle this case.
    if ((mStreamInitialized == PR_TRUE) && (mStreamEnded == PR_FALSE))
        inflateEnd (&d_stream);
}

NS_IMETHODIMP
nsHTTPCompressConv::AsyncConvertData(const char *aFromType, 
                                     const char *aToType, 
                                     nsIStreamListener *aListener, 
                                     nsISupports *aCtxt)
{
    if (!PL_strncasecmp(aFromType, HTTP_COMPRESS_TYPE, sizeof(HTTP_COMPRESS_TYPE)-1) ||
        !PL_strncasecmp(aFromType, HTTP_X_COMPRESS_TYPE, sizeof(HTTP_X_COMPRESS_TYPE)-1))
        mMode = HTTP_COMPRESS_COMPRESS;

    else if (!PL_strncasecmp(aFromType, HTTP_GZIP_TYPE, sizeof(HTTP_COMPRESS_TYPE)-1) ||
             !PL_strncasecmp(aFromType, HTTP_X_GZIP_TYPE, sizeof(HTTP_X_GZIP_TYPE)-1))
        mMode = HTTP_COMPRESS_GZIP;

    else if (!PL_strncasecmp(aFromType, HTTP_DEFLATE_TYPE, sizeof(HTTP_DEFLATE_TYPE)-1))
        mMode = HTTP_COMPRESS_DEFLATE;

    // hook ourself up with the receiving listener. 
    mListener = aListener;
    NS_ADDREF(mListener);

    mAsyncConvContext = aCtxt;
    return NS_OK; 
} 

NS_IMETHODIMP
nsHTTPCompressConv::OnStartRequest(nsIRequest* request, nsISupports *aContext)
{
    return mListener->OnStartRequest(request, aContext);
} 

NS_IMETHODIMP
nsHTTPCompressConv::OnStopRequest(nsIRequest* request, nsISupports *aContext, 
                                  nsresult aStatus)
{
    return mListener->OnStopRequest(request, aContext, aStatus);
} 

NS_IMETHODIMP
nsHTTPCompressConv::OnDataAvailable(nsIRequest* request, 
                                    nsISupports *aContext, 
                                    nsIInputStream *iStr, 
                                    PRUint32 aSourceOffset, 
                                    PRUint32 aCount)
{
    nsresult rv = NS_ERROR_FAILURE;
    PRUint32 streamLen = aCount;

    if (streamLen == 0)
    {
        NS_ERROR("count of zero passed to OnDataAvailable");
        return NS_ERROR_UNEXPECTED;
    }

    if (mStreamEnded)
    {
        // Hmm... this may just indicate that the data stream is done and that
        // what's left is either metadata or padding of some sort.... throwing
        // it out is probably the safe thing to do.
        PRUint32 n;
        return iStr->ReadSegments(DiscardSegments, nsnull, streamLen, &n);
    }

    switch (mMode)
    {
        case HTTP_COMPRESS_GZIP:
            streamLen = check_header(iStr, streamLen, &rv);

            if (rv != NS_OK)
                return rv;

            if (streamLen == 0)
                return NS_OK;

        case HTTP_COMPRESS_DEFLATE:

            if (mInpBuffer != NULL && streamLen > mInpBufferLen)
            {
                mInpBuffer = (unsigned char *) nsMemory::Realloc(mInpBuffer, mInpBufferLen = streamLen);
               
                if (mOutBufferLen < streamLen * 2)
                    mOutBuffer = (unsigned char *) nsMemory::Realloc(mOutBuffer, mOutBufferLen = streamLen * 3);

                if (mInpBuffer == NULL || mOutBuffer == NULL)
                    return NS_ERROR_OUT_OF_MEMORY;
            }

            if (mInpBuffer == NULL)
                mInpBuffer = (unsigned char *) nsMemory::Alloc(mInpBufferLen = streamLen);

            if (mOutBuffer == NULL)
                mOutBuffer = (unsigned char *) nsMemory::Alloc(mOutBufferLen = streamLen * 3);

            if (mInpBuffer == NULL || mOutBuffer == NULL)
                return NS_ERROR_OUT_OF_MEMORY;

            iStr->Read((char *)mInpBuffer, streamLen, &rv);

            if (NS_FAILED(rv))
                return rv;

            if (mMode == HTTP_COMPRESS_DEFLATE)
            {
                if (!mStreamInitialized)
                {
                    memset(&d_stream, 0, sizeof (d_stream));
                
                    if (inflateInit(&d_stream) != Z_OK)
                        return NS_ERROR_FAILURE;

                    mStreamInitialized = PR_TRUE;
                }
                d_stream.next_in = mInpBuffer;
                d_stream.avail_in = (uInt)streamLen;

                mDummyStreamInitialised = PR_FALSE;
                for (;;)
                {
                    d_stream.next_out = mOutBuffer;
                    d_stream.avail_out = (uInt)mOutBufferLen;
 
                    int code = inflate(&d_stream, Z_NO_FLUSH);
                    unsigned bytesWritten = (uInt)mOutBufferLen - d_stream.avail_out;

                    if (code == Z_STREAM_END)
                    {
                        if (bytesWritten)
                        {
                            rv = do_OnDataAvailable(request, aContext, aSourceOffset, (char *)mOutBuffer, bytesWritten);
                            if (NS_FAILED (rv))
                                return rv;
                        }
                        
                        inflateEnd(&d_stream);
                        mStreamEnded = PR_TRUE;
                        break;
                    }
                    else if (code == Z_OK)
                    {
                        if (bytesWritten)
                        {
                            rv = do_OnDataAvailable(request, aContext, aSourceOffset, (char *)mOutBuffer, bytesWritten);
                            if (NS_FAILED (rv))
                                return rv;
                        }
                    }
                    else if (code == Z_BUF_ERROR)
                    {
                        if (bytesWritten)
                        {
                            rv = do_OnDataAvailable(request, aContext, aSourceOffset, (char *)mOutBuffer, bytesWritten);
                            if (NS_FAILED (rv))
                                return rv;
                        }
                        break;
                    }
                    else if (code == Z_DATA_ERROR)
                    {
                        // some servers (notably Apache with mod_deflate) don't generate zlib headers
                        // insert a dummy header and try again
                        static char dummy_head[2] =
                        {
                            0x8 + 0x7 * 0x10,
                            (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
                        };
                        inflateReset(&d_stream);
                        d_stream.next_in = (Bytef*) dummy_head;
                        d_stream.avail_in = sizeof(dummy_head);

                        code = inflate(&d_stream, Z_NO_FLUSH);
                        if (code != Z_OK)
                            return NS_ERROR_FAILURE;

                        // stop an endless loop caused by non-deflate data being labelled as deflate
                        if (mDummyStreamInitialised) {
                            NS_ERROR("endless loop detected");
                            return NS_ERROR_FAILURE;
                        }
                        mDummyStreamInitialised = PR_TRUE;
                        // reset stream pointers to our original data
                        d_stream.next_in = mInpBuffer;
                        d_stream.avail_in = (uInt)streamLen;
                    }    
                    else
                        return NS_ERROR_FAILURE;
                } /* for */
            }
            else
            {
                if (!mStreamInitialized)
                {
                    memset(&d_stream, 0, sizeof (d_stream));
                
                    if (inflateInit2(&d_stream, -MAX_WBITS) != Z_OK)
                        return NS_ERROR_FAILURE;

                    mStreamInitialized = PR_TRUE;
                }

                d_stream.next_in  = mInpBuffer;
                d_stream.avail_in = (uInt)streamLen;

                for (;;)
                {
                    d_stream.next_out  = mOutBuffer;
                    d_stream.avail_out = (uInt)mOutBufferLen;
 
                    int code = inflate (&d_stream, Z_NO_FLUSH);
                    unsigned bytesWritten = (uInt)mOutBufferLen - d_stream.avail_out;

                    if (code == Z_STREAM_END)
                    {
                        if (bytesWritten)
                        {
                            rv = do_OnDataAvailable(request, aContext, aSourceOffset, (char *)mOutBuffer, bytesWritten);
                            if (NS_FAILED (rv))
                                return rv;
                        }
                        
                        inflateEnd(&d_stream);
                        mStreamEnded = PR_TRUE;
                        break;
                    }
                    else if (code == Z_OK)
                    {
                        if (bytesWritten)
                        {
                            rv = do_OnDataAvailable(request, aContext, aSourceOffset, (char *)mOutBuffer, bytesWritten);
                            if (NS_FAILED (rv))
                                return rv;
                        }
                    }
                    else if (code == Z_BUF_ERROR)
                    {
                        if (bytesWritten)
                        {
                            rv = do_OnDataAvailable(request, aContext, aSourceOffset, (char *)mOutBuffer, bytesWritten);
                            if (NS_FAILED (rv))
                                return rv;
                        }
                        break;
                    }
                    else
                        return NS_ERROR_FAILURE;
                } /* for */
            } /* gzip */
            break;

        default: 
            rv = mListener->OnDataAvailable(request, aContext, iStr, aSourceOffset, aCount);
            if (NS_FAILED (rv))
                return rv;
    } /* switch */

	return NS_OK;
} /* OnDataAvailable */


// XXX/ruslan: need to implement this too

NS_IMETHODIMP
nsHTTPCompressConv::Convert(nsIInputStream *aFromStream, 
                            const char *aFromType, 
                            const char *aToType, 
                            nsISupports *aCtxt, 
                            nsIInputStream **_retval)
{ 
    return NS_ERROR_NOT_IMPLEMENTED;
} 

nsresult
nsHTTPCompressConv::do_OnDataAvailable(nsIRequest* request, nsISupports *aContext, PRUint32 aSourceOffset, char *buffer, PRUint32 aCount)
{
    nsresult rv;

    nsCOMPtr<nsIByteArrayInputStream> convertedStreamSup;

    char *lBuf = (char *) nsMemory::Alloc (aCount);
    if (lBuf == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(lBuf, buffer, aCount);

    rv = NS_NewByteArrayInputStream(getter_AddRefs(convertedStreamSup), lBuf, aCount);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIInputStream> convertedStream = do_QueryInterface(convertedStreamSup, &rv);
    if (NS_FAILED(rv))
        return rv;

    return mListener->OnDataAvailable(request, aContext, convertedStream, aSourceOffset, aCount);
}

#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

static unsigned gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

PRUint32
nsHTTPCompressConv::check_header(nsIInputStream *iStr, PRUint32 streamLen, nsresult *rs)
{
    nsresult rv;
    enum  { GZIP_INIT = 0, GZIP_OS, GZIP_EXTRA0, GZIP_EXTRA1, GZIP_EXTRA2, GZIP_ORIG, GZIP_COMMENT, GZIP_CRC };
    char c;

    *rs = NS_OK;

    if (mCheckHeaderDone)
        return streamLen;

    while (streamLen)
    {
        switch (hMode)
        {
            case GZIP_INIT:
                iStr->Read (&c, 1, &rv);
                streamLen--;
                
                if (mSkipCount == 0 && ((unsigned)c & 0377) != gz_magic[0])
                {
                    *rs = NS_ERROR_FAILURE;
                    return 0;
                }

                if (mSkipCount == 1 && ((unsigned)c & 0377) != gz_magic[1])
                {
                    *rs = NS_ERROR_FAILURE;
                    return 0;
                }

                if (mSkipCount == 2 && ((unsigned)c & 0377) != Z_DEFLATED)
                {
                    *rs = NS_ERROR_FAILURE;
                    return 0;
                }

                mSkipCount++;
                if (mSkipCount == 4)
                {
                    mFlags = (unsigned) c & 0377;
                    if (mFlags & RESERVED)
                    {
                        *rs = NS_ERROR_FAILURE;
                        return 0;
                    }
                    hMode = GZIP_OS;
                    mSkipCount = 0;
                }
                break;

            case GZIP_OS:
                iStr->Read(&c, 1, &rv);
                streamLen--;
                mSkipCount++;

                if (mSkipCount == 6)
                    hMode = GZIP_EXTRA0;
                break;
        
            case GZIP_EXTRA0:
                if (mFlags & EXTRA_FIELD)
                {
                    iStr->Read(&c, 1, &rv);
                    streamLen--;
                    mLen = (uInt) c & 0377;
                    hMode = GZIP_EXTRA1;
                }
                else
                    hMode = GZIP_ORIG;
                break;

            case GZIP_EXTRA1:
                iStr->Read(&c, 1, &rv);
                streamLen--;
                mLen = ((uInt) c & 0377) << 8;
                mSkipCount = 0;
                hMode = GZIP_EXTRA2;
                break;

            case GZIP_EXTRA2:
                if (mSkipCount == mLen)
                    hMode = GZIP_ORIG;
                else
                {
                    iStr->Read(&c, 1, &rv);
                    streamLen--;
                    mSkipCount++;
                }
                break;

            case GZIP_ORIG:
                if (mFlags & ORIG_NAME)
                {
                    iStr->Read(&c, 1, &rv);
                    streamLen--;
                    if (c == 0)
                        hMode = GZIP_COMMENT;
                }
                else
                    hMode = GZIP_COMMENT;
                break;

            case GZIP_COMMENT:
                if (mFlags & GZIP_COMMENT)
                {
                    iStr->Read(&c, 1, &rv);
                    streamLen--;
                    if (c == 0)
                    {
                        hMode = GZIP_CRC;
                        mSkipCount = 0;
                    }
                }
                else
                {
                    hMode = GZIP_CRC;
                    mSkipCount = 0;
                }
                break;

            case GZIP_CRC:
                if (mFlags & HEAD_CRC)
                {
                    iStr->Read(&c, 1, &rv);
                    streamLen--;
                    mSkipCount++;
                    if (mSkipCount == 2)
                    {
                        mCheckHeaderDone = PR_TRUE;
                        return streamLen;
                    }
                }
                else
                {
                    mCheckHeaderDone = PR_TRUE;
                    return streamLen;
                }
            break;
        }
    }
    return streamLen;
}

nsresult
NS_NewHTTPCompressConv(nsHTTPCompressConv **aHTTPCompressConv)
{
    NS_PRECONDITION(aHTTPCompressConv != nsnull, "null ptr");

    if (!aHTTPCompressConv)
        return NS_ERROR_NULL_POINTER;

    *aHTTPCompressConv = new nsHTTPCompressConv();

    if (!*aHTTPCompressConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aHTTPCompressConv);
    return NS_OK;
}
