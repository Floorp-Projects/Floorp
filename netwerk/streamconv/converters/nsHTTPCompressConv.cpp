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
 * Contributor(s): ruslan
 */

#include "nsHTTPCompressConv.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "prlog.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsIByteArrayInputStream.h"
#include "nsIStringStream.h"
#include "zlib.h"

#include <ctype.h>

// nsISupports implementation
NS_IMPL_ISUPPORTS2 (nsHTTPCompressConv, nsIStreamConverter, nsIStreamListener);

// nsFTPDirListingConv methods
nsHTTPCompressConv::nsHTTPCompressConv ()
    :   mListener (nsnull),
        mInpBuffer  (NULL), mInpBufferLen (0),
        mOutBuffer  (NULL), mOutBufferLen (0),
        mMode (HTTP_COMPRESS_IDENTITY)
{
    NS_INIT_ISUPPORTS ();
}

nsHTTPCompressConv::~nsHTTPCompressConv ()
{
    NS_IF_RELEASE(mListener);

	if (mInpBuffer != NULL)
		nsAllocator::Free (mInpBuffer);

    if (mOutBuffer != NULL)
		nsAllocator::Free (mOutBuffer);
}

NS_IMETHODIMP
nsHTTPCompressConv::AsyncConvertData (
							const PRUnichar *aFromType, 
							const PRUnichar *aToType, 
							nsIStreamListener *aListener, 
							nsISupports *aCtxt)
{
	nsString2 from (aFromType);
	nsString2 to   ( aToType );

	char * fromStr = from.ToNewCString ();
	char *   toStr =   to.ToNewCString ();

	if (!PL_strncasecmp (fromStr, HTTP_COMPRESS_TYPE  , strlen (HTTP_COMPRESS_TYPE  ))
        ||
        !PL_strncasecmp (fromStr, HTTP_X_COMPRESS_TYPE, strlen (HTTP_X_COMPRESS_TYPE)))
        mMode = HTTP_COMPRESS_COMPRESS;
    else
	if (!PL_strncasecmp (fromStr, HTTP_GZIP_TYPE   , strlen (HTTP_COMPRESS_TYPE))
        ||
        !PL_strncasecmp (fromStr, HTTP_X_GZIP_TYPE , strlen (HTTP_X_GZIP_TYPE)))
        mMode = HTTP_COMPRESS_GZIP;
    else
	if (!PL_strncasecmp (fromStr, HTTP_DEFLATE_TYPE, strlen (HTTP_DEFLATE_TYPE)))
        mMode = HTTP_COMPRESS_DEFLATE;

	nsAllocator::Free (fromStr);
	nsAllocator::Free (  toStr);

    // hook ourself up with the receiving listener. 
    mListener = aListener;
    NS_ADDREF (mListener);

    mAsyncConvContext = aCtxt;
	
    return NS_OK; 
} 

NS_IMETHODIMP
nsHTTPCompressConv::OnStartRequest (nsIChannel *aChannel, nsISupports *aContext)
{
    return mListener -> OnStartRequest (aChannel, aContext);
} 

NS_IMETHODIMP
nsHTTPCompressConv::OnStopRequest  (nsIChannel *aChannel, nsISupports *aContext, nsresult status, const PRUnichar *errorMsg)
{
    return mListener -> OnStopRequest  (aChannel, aContext, status, errorMsg);
} 

NS_IMETHODIMP
nsHTTPCompressConv::OnDataAvailable ( 
							  nsIChannel *aChannel, 
							  nsISupports *aContext, 
							  nsIInputStream *iStr, 
							  PRUint32 aSourceOffset, 
							  PRUint32 aCount)
{
    nsresult rv = NS_ERROR_FAILURE;
	PRUint32 streamLen;

    rv = iStr -> Available (&streamLen);
    if (NS_FAILED (rv))
		return rv;

	if (streamLen == 0)
		return NS_OK;

    switch (mMode)
    {
        case HTTP_COMPRESS_DEFLATE :
        case HTTP_COMPRESS_GZIP    :
        case HTTP_COMPRESS_COMPRESS:

            if (mInpBuffer != NULL && streamLen > mInpBufferLen)
            {
                mInpBuffer = (unsigned char *) nsAllocator::Realloc (mInpBuffer, mInpBufferLen = streamLen);
                
                if (mOutBufferLen < streamLen * 2)
                    mInpBuffer = (unsigned char *) nsAllocator::Realloc (mOutBuffer, mInpBufferLen = streamLen * 3);

                if (mInpBuffer == NULL || mOutBuffer == NULL)
                    return NS_ERROR_OUT_OF_MEMORY;
            }

            if (mInpBuffer == NULL)
                mInpBuffer = (unsigned char *) nsAllocator::Alloc (mInpBufferLen = streamLen);

            if (mOutBuffer == NULL)
                mOutBuffer = (unsigned char *) nsAllocator::Alloc (mOutBufferLen = streamLen * 3);

            if (mInpBuffer == NULL || mOutBuffer == NULL)
                return NS_ERROR_OUT_OF_MEMORY;

            iStr -> Read ((char *)mInpBuffer, streamLen, &rv);

            if (NS_FAILED (rv))
                return rv;

            if (mMode == HTTP_COMPRESS_COMPRESS)
            {
                unsigned long uLen = mOutBufferLen;
                int code = uncompress (mOutBuffer, &uLen, mInpBuffer, mInpBufferLen);
                if (code == Z_BUF_ERROR)
                {
                    mOutBuffer = (unsigned char *) nsAllocator::Realloc (mOutBuffer, mOutBufferLen *= 3);
                    if (mOutBuffer == NULL)
                        return NS_ERROR_OUT_OF_MEMORY;
                    
                    code = uncompress (mOutBuffer, &uLen, mInpBuffer, mInpBufferLen);
                }
                if (code != Z_OK)
                    return NS_ERROR_FAILURE;

                rv = do_OnDataAvailable (aChannel, aContext, aSourceOffset, (char *)mOutBuffer, (PRUint32) uLen);
                if (NS_FAILED (rv))
                    return rv;

            }
            else
            {    
                z_stream d_stream;
                memset (&d_stream, 0, sizeof (d_stream));

                if (inflateInit (&d_stream) != Z_OK)
                    return NS_ERROR_FAILURE;

                d_stream.next_in  = mInpBuffer;
                d_stream.avail_in = (uInt)mInpBufferLen;

                for ( ; ; )
                {
                    d_stream.next_out  = mOutBuffer;
	                d_stream.avail_out = (uInt)mOutBufferLen;

                    int code = inflate  (&d_stream, Z_NO_FLUSH);
                    if (code == Z_STREAM_END)
                    {
                        rv = do_OnDataAvailable (aChannel, aContext, aSourceOffset, (char *)mOutBuffer, d_stream.total_out);
                        if (NS_FAILED (rv))
                            return rv;
                        break;
                    }
                    else
                    if (code == Z_OK)
                    {
                        rv = do_OnDataAvailable (aChannel, aContext, aSourceOffset, (char *)mOutBuffer, d_stream.total_out);
                        if (NS_FAILED (rv))
                            return rv;
                    }
                    else
                        return NS_ERROR_FAILURE;
                } /* for */
            } /* gzip */
            break;

        default: 
            rv = mListener -> OnDataAvailable (aChannel, aContext, iStr, aSourceOffset, aCount);
			if (NS_FAILED (rv))
			    return rv;
    } /* switch */

	return NS_OK;
} /* OnDataAvailable */


// XXX/ruslan: need to implement this too

NS_IMETHODIMP
nsHTTPCompressConv::Convert (
						  nsIInputStream *aFromStream, 
						  const PRUnichar *aFromType, 
						  const PRUnichar *aToType, 
						  nsISupports *aCtxt, 
						  nsIInputStream **_retval)
{ 
    return NS_ERROR_NOT_IMPLEMENTED;
} 

nsresult
NS_NewHTTPCompressConv (nsHTTPCompressConv ** aHTTPCompressConv)
{
    NS_PRECONDITION(aHTTPCompressConv != nsnull, "null ptr");

    if (! aHTTPCompressConv)
        return NS_ERROR_NULL_POINTER;

    *aHTTPCompressConv = new nsHTTPCompressConv ();

    if (! *aHTTPCompressConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aHTTPCompressConv);
    return NS_OK;
}

nsresult
nsHTTPCompressConv::do_OnDataAvailable (nsIChannel *aChannel, nsISupports *aContext, PRUint32 aSourceOffset, char *buffer, PRUint32 aCount)
{
    nsresult rv;

    nsIInputStream * convertedStream = nsnull;
	nsIByteArrayInputStream * convertedStreamSup = nsnull;

	rv = NS_NewByteArrayInputStream (&convertedStreamSup, buffer, aCount);
	if (NS_FAILED (rv)) 
	    return rv;

    rv = convertedStreamSup -> QueryInterface (NS_GET_IID (nsIInputStream), (void**)&convertedStream);
	NS_RELEASE (convertedStreamSup);
 
	if (NS_FAILED (rv))
	    return rv;

	rv = mListener -> OnDataAvailable (aChannel, aContext, convertedStream, aSourceOffset, aCount);

    return rv;
}
