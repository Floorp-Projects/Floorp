/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBinHexDecoder.h"
#include "nsIServiceManager.h"
#include "nsIStreamConverterService.h"
#include "nsCRT.h"
#include "nsIPipe.h"
#include "nsMimeTypes.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "prnetdb.h"
#include "nsIURI.h"
#include "nsIURL.h"

#include "nsIMIMEService.h"
#include "nsMimeTypes.h"
#include "nsCExternalHandlerService.h"

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

#define DATA_BUFFER_SIZE (4096*2) 

#define NS_STREAM_CONVERTER_SEGMENT_SIZE   (4*1024)
#define NS_STREAM_CONVERTER_BUFFER_SIZE    (32*1024)

// sadly I couldn't find char defintions for CR LF elsehwere in the code (they are defined as strings in nsCRT.h)
#define CR  '\015'
#define LF '\012'

nsBinHexDecoder::nsBinHexDecoder() : mCRC(0), mFileCRC(0), mOctetin(26), 
  mDonePos(3), mInCRC(0), mCount(0), mMarker(0), mPosInbuff(0), 
  mPosOutputBuff(0), mState(0)
{
  NS_INIT_ISUPPORTS();
  mDataBuffer = nsnull;
  mOutgoingBuffer = nsnull;

  mOctetBuf.val 	= 0;
  mHeader.type = 0;
  mHeader.creator = 0;
  mHeader.flags = 0;
  mHeader.dlen = 0;
  mHeader.rlen = 0;
}

nsBinHexDecoder::~nsBinHexDecoder()
{
  if (mDataBuffer)
    nsMemory::Free(mDataBuffer);
  if (mOutgoingBuffer)
    nsMemory::Free(mOutgoingBuffer);
}

NS_IMPL_ADDREF(nsBinHexDecoder);
NS_IMPL_RELEASE(nsBinHexDecoder);

NS_INTERFACE_MAP_BEGIN(nsBinHexDecoder)
   NS_INTERFACE_MAP_ENTRY(nsIStreamConverter)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


// The binhex 4.0 decoder table....

static char binhex_decode[256] = 
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, -1, -1,
	13, 14, 15, 16, 17, 18, 19, -1, 20, 21, -1, -1, -1, -1, -1, -1,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, -1,
	37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46, 47, -1, -1, -1, -1,
	48, 49, 50, 51, 52, 53, 54, -1, 55, 56, 57, 58, 59, 60, -1, -1,
	61, 62, 63, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

#define BHEXVAL(c) (binhex_decode[(unsigned char) c])

//////////////////////////////////////////////////////
// nsIStreamConverter methods...
//////////////////////////////////////////////////////

NS_IMETHODIMP
nsBinHexDecoder::Convert(nsIInputStream *aFromStream,
                          const PRUnichar *aFromType,
                          const PRUnichar *aToType,
                          nsISupports *aCtxt, 
                          nsIInputStream **aResultStream) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinHexDecoder::AsyncConvertData(const PRUnichar *aFromType, 
                                   const PRUnichar *aToType,
                                   nsIStreamListener *aListener, 
                                   nsISupports *aCtxt)
{
  NS_ASSERTION(aListener && aFromType && aToType, 
               "null pointer passed into bin hex converter");

  // hook up our final listener. this guy gets the various On*() calls we want to throw
  // at him.
  //
  mNextListener = aListener;
  return (aListener) ? NS_OK : NS_ERROR_FAILURE;
}

//////////////////////////////////////////////////////
// nsIStreamListener methods...
//////////////////////////////////////////////////////
NS_IMETHODIMP
nsBinHexDecoder::OnDataAvailable(nsIRequest* request, 
                                  nsISupports *aCtxt,
                                  nsIInputStream *aStream, 
                                  PRUint32 aSourceOffset, 
                                  PRUint32 aCount)
{
  nsresult rv = NS_OK;

  if (mOutputStream && mDataBuffer && aCount > 0)
  {
    PRUint32 numBytesRead = 0; 
    PRUint32 numBytesWritten = 0;
    while (aCount > 0) // while we still have bytes to copy...
    {
      aStream->Read(mDataBuffer, PR_MIN(aCount, DATA_BUFFER_SIZE - 1), &numBytesRead);
      if (aCount >= numBytesRead)
        aCount -= numBytesRead; // subtract off the number of bytes we just read
      else
        aCount = 0;

      // Process this new chunk of bin hex data...
      ProcessNextChunk(request, aCtxt, numBytesRead);
    }
  }

  return rv;
}

nsresult nsBinHexDecoder::ProcessNextState(nsIRequest * aRequest, nsISupports * aContext)
{
	nsresult status = NS_OK;
	PRUint16 	tmpcrc, cval;
	unsigned char  ctmp, c = mRlebuf;
	
	/* do CRC */
	ctmp = mInCRC ? c : 0;
	cval = mCRC & 0xf000;
	tmpcrc = ((PRUint16) (mCRC << 4) | (ctmp >> 4)) ^ (cval | (cval >> 7) | (cval >> 12));
	cval = tmpcrc & 0xf000;
	mCRC = ((PRUint16) (tmpcrc << 4) | (ctmp & 0x0f)) ^ (cval | (cval >> 7) | (cval >> 12));

	/* handle state */
	switch (mState) 
	{
		case BINHEX_STATE_START:
			mState = BINHEX_STATE_FNAME;
			mCount = 1;

      // c & 63 returns the length of mName. So if we need the length, that's how
      // you can figure it out...for now we are storing it in the first byte of mName
			*(mName) = (c & 63);     
			break;
			
		case BINHEX_STATE_FNAME:
			mName[mCount] = c;
			
			if (mCount++ > *(mName)) // the first byte of mName is the length...
			{			
        // okay we've figured out the file name....set the content type on the channel
        // based on the file name AND issue our delayed on start request....
        // be sure to skip the first byte of mName which is the size of the file name
        
        SetContentType(aRequest, &mName[1]);
        // now propogate the on start request
        mNextListener->OnStartRequest(aRequest, aContext);

				mState = BINHEX_STATE_HEADER;
				mCount = 0;
			}
			break;
			
		case BINHEX_STATE_HEADER:
			((char *) &mHeader)[mCount] = c;
			if (++mCount == 18) 
			{
				if (sizeof(binhex_header) != 18)	/* fix an alignment problem in some OSes */
				{
					char *p = (char *)&mHeader;
					p += 19;
					for (c = 0; c < 8; c++)
          {
						*p = *(p-2);	p--;
          }
 				}

        mState = BINHEX_STATE_HCRC;
				mInCRC = 1;
				mCount = 0;
			}
			break;
			
		case BINHEX_STATE_DFORK:
		case BINHEX_STATE_RFORK:
			mOutgoingBuffer[mPosOutputBuff++] = c;
			if (-- mCount == 0) 
			{
  			/* only output data fork in the non-mac system.			*/
				if (mState == BINHEX_STATE_DFORK)
				{
          PRUint32 numBytesWritten = 0; 
          mOutputStream->Write(mOutgoingBuffer, mPosOutputBuff, &numBytesWritten);
          if (numBytesWritten != mPosOutputBuff) 
            status = NS_ERROR_FAILURE;

          // now propogate the data we just wrote
          mNextListener->OnDataAvailable(aRequest, aContext, mInputStream, 0, numBytesWritten);
				}
				else
					status = NS_OK;				/* do nothing for resource fork.	*/

				mPosOutputBuff = 0;
				
				if (status != NS_OK)
					mState = BINHEX_STATE_DONE;
				else
				{
					mState ++;
				}
				
        mInCRC = 1;
			}
			else if (mPosOutputBuff >= DATA_BUFFER_SIZE)
			{				
				if (mState == BINHEX_STATE_DFORK)
				{
          PRUint32 numBytesWritten = 0; 
          mOutputStream->Write(mOutgoingBuffer, mPosOutputBuff, &numBytesWritten);
          if (numBytesWritten != mPosOutputBuff) 
            status = NS_ERROR_FAILURE;

          mNextListener->OnDataAvailable(aRequest, aContext, mInputStream, 0, numBytesWritten);
          mPosOutputBuff = 0;
        }							
			}
			break;
			
		case BINHEX_STATE_HCRC:
		case BINHEX_STATE_DCRC:
		case BINHEX_STATE_RCRC:
			if (!mCount++) 
			{
				mFileCRC = (unsigned short) c << 8;
			} 
			else 
			{
				if ((mFileCRC | c) != mCRC) 
				{
					mState = BINHEX_STATE_DONE;
					break;
				}
				
				/* passed the CRC check!!!*/
				mCRC = 0;
				if (++ mState == BINHEX_STATE_FINISH) 
				{ 
          // when we reach the finished state...fire an on stop request on the event listener...
          mNextListener->OnStopRequest(aRequest, aContext, NS_OK);
          mNextListener = 0;
      
          /* 	now We are done with everything.	*/		
					mState++;
					break;
				}
				
				if (mState == BINHEX_STATE_DFORK) 
				{			
					mCount = PR_ntohl(mHeader.dlen);
				}
				else
				{
          // we aren't processing the resurce Fork. uncomment this line if we make this converter
          // smart enough to do this in the future.
					// mCount = PR_ntohl(mHeader.rlen);	/* it should in host byte order */
          mCount = 0; 
				}
				
				if (mCount) 
				{
					mInCRC = 0;						
				} 
				else 
				{
					/* nothing inside, so skip to the next state. */
					mState ++;
				}
			}
			break;
	}
	
	return NS_OK;
}

nsresult nsBinHexDecoder::ProcessNextChunk(nsIRequest * aRequest, nsISupports * aContext, PRUint32 numBytesInBuffer)
{
	PRBool	foundStart;
	PRInt16 octetpos, c = 0;
	PRUint32 		val;
  mPosInDataBuffer  = 0; // use member variable.

  NS_ENSURE_TRUE(numBytesInBuffer > 0, NS_ERROR_FAILURE);
	
	//	if it is the first time, seek to the right start place. 
	if (mState == BINHEX_STATE_START)
	{
		 foundStart = PR_FALSE;
		// go through the line, until we get a ':'
		while (mPosInDataBuffer < numBytesInBuffer)
		{
			c = mDataBuffer[mPosInDataBuffer++];
			while (c == CR || c == LF)
			{
				if (mPosInDataBuffer >= numBytesInBuffer)
					break;
																
				c = mDataBuffer[mPosInDataBuffer++];
				if (c == ':')
				{
					foundStart = PR_TRUE;
					break;
				}
			}
			if (foundStart)	break;		/* we got the start point. */
		}
		
		if (mPosInDataBuffer >= numBytesInBuffer)
			return NS_OK;			/* we meet buff end before we get the start point, wait till next fills. */
		
		if (c != ':')
			return NS_ERROR_FAILURE;		/* can't find the start character.	*/
	}
	
	while (mState != BINHEX_STATE_DONE) 
	{
		/* fill in octetbuf */
		do 
		{
			if (mPosInDataBuffer >= numBytesInBuffer)
				return NS_OK;			/* end of buff, go on for the nxet calls. */
					
			c = GetNextChar(numBytesInBuffer);
			if (c == 0)	return NS_OK;
				 
			if ((val = BHEXVAL(c)) == -1) 
			{
				/* we incount an invalid character.	*/
				if (c) 
				{
					/* rolling back. */
					mDonePos --;
					if (mOctetin >= 14)		mDonePos--;
					if (mOctetin >= 20) 	mDonePos--;
				}
				break;
			}
			mOctetBuf.val |= val << mOctetin;
		} 
		while ((mOctetin -= 6) > 2);
			
		/* handle decoded characters -- run length encoding (rle) detection */

#ifndef XP_MAC
		mOctetBuf.val = PR_ntohl(mOctetBuf.val);
#endif

		for (octetpos = 0; octetpos < mDonePos; ++octetpos) 
		{
			c = mOctetBuf.c[octetpos];
			
			if (c == 0x90 && !mMarker++) 
				continue;
						
			if (mMarker) 
			{
				if (c == 0) 
				{
					mRlebuf = 0x90;
					ProcessNextState(aRequest, aContext);
				} 
				else 
				{
					while (--c > 0)				/* we are in the run lenght mode */ 
					{
						ProcessNextState(aRequest, aContext);
					}
				}
				mMarker = 0;
			} 
			else 
			{
				mRlebuf = (unsigned char) c;
				ProcessNextState(aRequest, aContext);
			}		
			
			if (mState >= BINHEX_STATE_DONE) 
				break;
		}
		
		/* prepare for next 3 characters.	*/
		if (mDonePos < 3 && mState < BINHEX_STATE_DONE) 
			mState = BINHEX_STATE_DONE;
					
		mOctetin = 26;
		mOctetBuf.val 	= 0;
	}

  return 	NS_OK;
}

PRInt16 nsBinHexDecoder::GetNextChar(PRUint32 numBytesInBuffer)
{
	char c = 0;
	
	while (mPosInDataBuffer < numBytesInBuffer)
	{
		c = mDataBuffer[mPosInDataBuffer++];
		if (c != LF && c != CR)
			break;
	}
	return (c == LF || c == CR) ? 0 : (int) c;
}

//////////////////////////////////////////////////////
// nsIRequestObserver methods...
//////////////////////////////////////////////////////

NS_IMETHODIMP
nsBinHexDecoder::OnStartRequest(nsIRequest* request, nsISupports *aCtxt) 
{
  nsresult rv = NS_OK;

  NS_ENSURE_TRUE(mNextListener, NS_ERROR_FAILURE);

  mDataBuffer = (char *) nsMemory::Alloc((sizeof(char) * DATA_BUFFER_SIZE));
  mOutgoingBuffer = (char *) nsMemory::Alloc((sizeof(char) * DATA_BUFFER_SIZE));
  if (!mDataBuffer || !mOutgoingBuffer) return NS_ERROR_FAILURE; // out of memory;

  // now we want to create a pipe which we'll use to write our converted data...
  rv = NS_NewPipe(getter_AddRefs(mInputStream), getter_AddRefs(mOutputStream),
                  NS_STREAM_CONVERTER_SEGMENT_SIZE,
                  NS_STREAM_CONVERTER_BUFFER_SIZE,
                  PR_TRUE, PR_TRUE);

  // don't propogate the on start request to mNextListener until we have determined the content type.
  return rv;
}

// Given the current request and the fileName we discovered inside the bin hex decoding,
// figure out the content type and set it on the channel associated with the request.
nsresult nsBinHexDecoder::SetContentType(nsIRequest * aRequest, const char * fileName)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) 
  { 
    NS_WARNING("QI failed"); 
    return NS_ERROR_FAILURE; 
  }

  nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString contentType;
  if (fileName)
  {
    // extract the extension from fileName and look it up.
    const char * fileExt = PL_strrchr(fileName, '.');
    if (fileExt)
      mimeService->GetTypeFromExtension(fileExt, getter_Copies(contentType));
    mContentType = contentType;
  }

  if (mContentType.IsEmpty())
  {
    // get the url for the channel.....
    nsCOMPtr<nsIURI> uri; 
    channel->GetURI(getter_AddRefs(uri));
    if (uri)
    {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri);
      if (url)
      {
        nsXPIDLCString fileExt;
        rv = url->GetFileExtension(getter_Copies(fileExt));
        if (NS_SUCCEEDED(rv) && *(fileExt.get()))
        {
          rv = mimeService->GetTypeFromExtension(fileExt, getter_Copies(contentType));
          if (NS_SUCCEEDED(rv) && *(contentType.get()))
          mContentType = contentType;
        }
      }
    }
  } // if the content type is empty

  // if we STILL don't have a content type....say the content type is unknown...
  // AND to avoid a recursive loop, if after extracting the data fork, we didn't get a valid
  // content type and still think it's mac binhex, then reset to unknown.
  if (mContentType.IsEmpty() || mContentType.Equals("application/mac-binhex40")) 
  {
    mContentType = UNKNOWN_CONTENT_TYPE;
  }

  // now set the content type on the channel.
  channel->SetContentType(mContentType.get());

  return NS_OK;
}


NS_IMETHODIMP
nsBinHexDecoder::OnStopRequest(nsIRequest* request, nsISupports *aCtxt,
                                nsresult aStatus)
{
  nsresult rv = NS_OK;

  if (!mNextListener) return NS_ERROR_FAILURE;
  // don't do anything here...we'll fire our own on stop request when we are done
  // processing the data....

  return rv;
}
