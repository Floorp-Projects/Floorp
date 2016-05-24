/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIOService.h"
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
#include <algorithm>

namespace mozilla {
namespace net {

nsBinHexDecoder::nsBinHexDecoder() :
  mState(0), mCRC(0), mFileCRC(0), mOctetin(26),
  mDonePos(3), mInCRC(0), mCount(0), mMarker(0), mPosInbuff(0),
  mPosOutputBuff(0)
{
  mDataBuffer = nullptr;
  mOutgoingBuffer = nullptr;

  mOctetBuf.val = 0;
  mHeader.type = 0;
  mHeader.creator = 0;
  mHeader.flags = 0;
  mHeader.dlen = 0;
  mHeader.rlen = 0;
}

nsBinHexDecoder::~nsBinHexDecoder()
{
  if (mDataBuffer)
    free(mDataBuffer);
  if (mOutgoingBuffer)
    free(mOutgoingBuffer);
}

NS_IMPL_ADDREF(nsBinHexDecoder)
NS_IMPL_RELEASE(nsBinHexDecoder)

NS_INTERFACE_MAP_BEGIN(nsBinHexDecoder)
  NS_INTERFACE_MAP_ENTRY(nsIStreamConverter)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


// The binhex 4.0 decoder table....

static const signed char binhex_decode[256] =
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
                         const char *aFromType,
                         const char *aToType,
                         nsISupports *aCtxt,
                         nsIInputStream **aResultStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBinHexDecoder::AsyncConvertData(const char *aFromType,
                                  const char *aToType,
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
                                 uint64_t aSourceOffset,
                                 uint32_t aCount)
{
  nsresult rv = NS_OK;

  if (mOutputStream && mDataBuffer && aCount > 0)
  {
    uint32_t numBytesRead = 0;
    while (aCount > 0) // while we still have bytes to copy...
    {
      aStream->Read(mDataBuffer, std::min(aCount, nsIOService::gDefaultSegmentSize - 1), &numBytesRead);
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
  uint16_t tmpcrc, cval;
  unsigned char ctmp, c = mRlebuf;

  /* do CRC */
  ctmp = mInCRC ? c : 0;
  cval = mCRC & 0xf000;
  tmpcrc = ((uint16_t) (mCRC << 4) | (ctmp >> 4)) ^ (cval | (cval >> 7) | (cval >> 12));
  cval = tmpcrc & 0xf000;
  mCRC = ((uint16_t) (tmpcrc << 4) | (ctmp & 0x0f)) ^ (cval | (cval >> 7) | (cval >> 12));

  /* handle state */
  switch (mState)
  {
    case BINHEX_STATE_START:
      mState = BINHEX_STATE_FNAME;
      mCount = 0;

      // c & 63 returns the length of mName. So if we need the length, that's how
      // you can figure it out....
      mName.SetLength(c & 63);
      if (mName.Length() != (c & 63)) {
        /* XXX ProcessNextState/ProcessNextChunk aren't rv checked */
        mState = BINHEX_STATE_DONE;
      }
      break;

    case BINHEX_STATE_FNAME:
      mName.BeginWriting()[mCount] = c;

      if (++mCount > mName.Length())
      {
        // okay we've figured out the file name....set the content type on the channel
        // based on the file name AND issue our delayed on start request....

        DetectContentType(aRequest, mName);
        // now propagate the on start request
        mNextListener->OnStartRequest(aRequest, aContext);

        mState = BINHEX_STATE_HEADER;
        mCount = 0;
      }
      break;

    case BINHEX_STATE_HEADER:
      ((char *) &mHeader)[mCount] = c;
      if (++mCount == 18)
      {
        if (sizeof(binhex_header) != 18)  /* fix an alignment problem in some OSes */
        {
          char *p = (char *)&mHeader;
          p += 19;
          for (c = 0; c < 8; c++)
          {
            *p = *(p-2);
            --p;
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
      if (--mCount == 0)
      {
        /* only output data fork in the non-mac system.      */
        if (mState == BINHEX_STATE_DFORK)
        {
          uint32_t numBytesWritten = 0;
          mOutputStream->Write(mOutgoingBuffer, mPosOutputBuff, &numBytesWritten);
          if (int32_t(numBytesWritten) != mPosOutputBuff)
            status = NS_ERROR_FAILURE;

          // now propagate the data we just wrote
          mNextListener->OnDataAvailable(aRequest, aContext, mInputStream, 0, numBytesWritten);
        }
        else
          status = NS_OK;        /* do nothing for resource fork.  */

        mPosOutputBuff = 0;

        if (status != NS_OK)
          mState = BINHEX_STATE_DONE;
        else
          ++mState;

        mInCRC = 1;
      }
      else if (mPosOutputBuff >= (int32_t) nsIOService::gDefaultSegmentSize)
      {
        if (mState == BINHEX_STATE_DFORK)
        {
          uint32_t numBytesWritten = 0;
          mOutputStream->Write(mOutgoingBuffer, mPosOutputBuff, &numBytesWritten);
          if (int32_t(numBytesWritten) != mPosOutputBuff)
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
        mFileCRC = (unsigned short) c << 8;
      else
      {
        if ((mFileCRC | c) != mCRC)
        {
          mState = BINHEX_STATE_DONE;
          break;
        }

        /* passed the CRC check!!!*/
        mCRC = 0;
        if (++mState == BINHEX_STATE_FINISH)
        {
          // when we reach the finished state...fire an on stop request on the event listener...
          mNextListener->OnStopRequest(aRequest, aContext, NS_OK);
          mNextListener = 0;

          /*   now We are done with everything.  */
          ++mState;
          break;
        }

        if (mState == BINHEX_STATE_DFORK)
          mCount = PR_ntohl(mHeader.dlen);
        else
        {
          // we aren't processing the resurce Fork. uncomment this line if we make this converter
          // smart enough to do this in the future.
          // mCount = PR_ntohl(mHeader.rlen);  /* it should in host byte order */
          mCount = 0;
        }

        if (mCount) {
          mInCRC = 0;
        } else {
          /* nothing inside, so skip to the next state. */
          ++mState;
        }
      }
      break;
  }

  return NS_OK;
}

nsresult nsBinHexDecoder::ProcessNextChunk(nsIRequest * aRequest, nsISupports * aContext, uint32_t numBytesInBuffer)
{
  bool foundStart;
  int16_t octetpos, c = 0;
  uint32_t val;
  mPosInDataBuffer = 0; // use member variable.

  NS_ENSURE_TRUE(numBytesInBuffer > 0, NS_ERROR_FAILURE);

  //  if it is the first time, seek to the right start place.
  if (mState == BINHEX_STATE_START)
  {
    foundStart = false;
    // go through the line, until we get a ':'
    while (mPosInDataBuffer < numBytesInBuffer)
    {
      c = mDataBuffer[mPosInDataBuffer++];
      while (c == nsCRT::CR || c == nsCRT::LF)
      {
        if (mPosInDataBuffer >= numBytesInBuffer)
          break;

        c = mDataBuffer[mPosInDataBuffer++];
        if (c == ':')
        {
          foundStart = true;
          break;
        }
      }
      if (foundStart)  break;    /* we got the start point. */
    }

    if (mPosInDataBuffer >= numBytesInBuffer)
      return NS_OK;      /* we meet buff end before we get the start point, wait till next fills. */

    if (c != ':')
      return NS_ERROR_FAILURE;    /* can't find the start character.  */
  }

  while (mState != BINHEX_STATE_DONE)
  {
    /* fill in octetbuf */
    do
    {
      if (mPosInDataBuffer >= numBytesInBuffer)
        return NS_OK;      /* end of buff, go on for the nxet calls. */

      c = GetNextChar(numBytesInBuffer);
      if (c == 0)  return NS_OK;

      if ((val = BHEXVAL(c)) == uint32_t(-1))
      {
        /* we incount an invalid character.  */
        if (c)
        {
          /* rolling back. */
          --mDonePos;
          if (mOctetin >= 14)
            --mDonePos;
          if (mOctetin >= 20)
            --mDonePos;
        }
        break;
      }
      mOctetBuf.val |= val << mOctetin;
    }
    while ((mOctetin -= 6) > 2);

    /* handle decoded characters -- run length encoding (rle) detection */

    // We put decoded chars into mOctetBuf.val in order from high to low (via
    // bitshifting, above).  But we want to byte-address them, so we want the
    // first byte to correspond to the high byte.  In other words, we want
    // these bytes to be in network order.
    mOctetBuf.val = PR_htonl(mOctetBuf.val);

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
          /* we are in the run length mode */
          while (--c > 0)
            ProcessNextState(aRequest, aContext);
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

    /* prepare for next 3 characters.  */
    if (mDonePos < 3 && mState < BINHEX_STATE_DONE)
      mState = BINHEX_STATE_DONE;

    mOctetin = 26;
    mOctetBuf.val = 0;
  }

  return   NS_OK;
}

int16_t nsBinHexDecoder::GetNextChar(uint32_t numBytesInBuffer)
{
  char c = 0;

  while (mPosInDataBuffer < numBytesInBuffer)
  {
    c = mDataBuffer[mPosInDataBuffer++];
    if (c != nsCRT::LF && c != nsCRT::CR)
      break;
  }
  return (c == nsCRT::LF || c == nsCRT::CR) ? 0 : (int) c;
}

//////////////////////////////////////////////////////
// nsIRequestObserver methods...
//////////////////////////////////////////////////////

NS_IMETHODIMP
nsBinHexDecoder::OnStartRequest(nsIRequest* request, nsISupports *aCtxt)
{
  nsresult rv = NS_OK;

  NS_ENSURE_TRUE(mNextListener, NS_ERROR_FAILURE);

  mDataBuffer = (char *) malloc((sizeof(char) * nsIOService::gDefaultSegmentSize));
  mOutgoingBuffer = (char *) malloc((sizeof(char) * nsIOService::gDefaultSegmentSize));
  if (!mDataBuffer || !mOutgoingBuffer) return NS_ERROR_FAILURE; // out of memory;

  // now we want to create a pipe which we'll use to write our converted data...
  rv = NS_NewPipe(getter_AddRefs(mInputStream), getter_AddRefs(mOutputStream),
                  nsIOService::gDefaultSegmentSize,
                  nsIOService::gDefaultSegmentSize,
                  true, true);

  // don't propagate the on start request to mNextListener until we have determined the content type.
  return rv;
}

// Given the fileName we discovered inside the bin hex decoding, figure out the
// content type and set it on the channel associated with the request.  If the
// filename tells us nothing useful, just report an unknown type and let the
// unknown decoder handle things.
nsresult nsBinHexDecoder::DetectContentType(nsIRequest* aRequest,
                                            const nsAFlatCString &aFilename)
{
  if (aFilename.IsEmpty()) {
    // Nothing to do here.
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMIMEService> mimeService(do_GetService("@mozilla.org/mime;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString contentType;

  // extract the extension from aFilename and look it up.
  const char * fileExt = strrchr(aFilename.get(), '.');
  if (!fileExt) {
    return NS_OK;
  }

  mimeService->GetTypeFromExtension(nsDependentCString(fileExt), contentType);

  // Only set the type if it's not empty and, to prevent recursive loops, not the binhex type
  if (!contentType.IsEmpty() && !contentType.Equals(APPLICATION_BINHEX)) {
    channel->SetContentType(contentType);
  } else {
    channel->SetContentType(NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE));
  }

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

} // namespace net
} // namespace mozilla
