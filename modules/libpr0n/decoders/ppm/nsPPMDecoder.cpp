/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 */

/*
 * Current limitations:
 *
 *  Doesn't handle PGM and PPM files for which the maximum sample value
 *  is something other than 255.
 *
 */

#include <ctype.h>

#include "nsPPMDecoder.h"

#include "nsIInputStream.h"
#include "imgIContainer.h"
#include "imgIContainerObserver.h"

#include "nspr.h"
#include "plstr.h"

#include "nsIComponentManager.h"

#include "nsRect.h"

NS_IMPL_ISUPPORTS1(nsPPMDecoder, imgIDecoder)

/*
 * "F_" stands for "find", so F_TYPE means we're looking for the image type
 * digit.
 */
enum ParseState {
  F_P,
  F_TYPE,
  F_WIDTH,
  F_HEIGHT,
  F_MAXVAL,
  F_INITIALIZE,
  F_TEXTBITDATA,
  F_TEXTDATA,
  F_RAWDATA
};

/*
 * What kind of white space do we skip.
 */
enum Skip {
  NOTHING,
  WHITESPACE,
  SINGLEWHITESPACE,
  TOENDOFLINE
};

/*
 * We look up the digit after the initial 'P' in this string to check
 * whether it's valid; the index is then used with the following
 * constants.
 */
static char ppmTypes[] = "1231456";

enum {
  TYPE_PBM = 0,
  TYPE_PGM = 1,
  TYPE_PPM = 2,
  PPMTYPE = 0x3,
  PPMRAW = 0x4
};

nsPPMDecoder::nsPPMDecoder() :
  mBuffer(nsnull),
  mBufferSize(0),
  mState(F_P),
  mSkip(NOTHING),
  mOldSkip(NOTHING),
  mType(0),
  mRowData(nsnull)
{
}

nsPPMDecoder::~nsPPMDecoder()
{
  if (mBuffer != nsnull)
    PR_Free(mBuffer);
  if (mRowData != nsnull)
    PR_Free(mRowData);
}


/** imgIDecoder methods **/

/* void init (in imgILoad aLoad); */
NS_IMETHODIMP nsPPMDecoder::Init(imgILoad *aLoad)
{
  mImageLoad = aLoad;

  mObserver = do_QueryInterface(aLoad);  // we're holding 2 strong refs to the request.

  mImage = do_CreateInstance("@mozilla.org/image/container;1");
  aLoad->SetImage(mImage);

  mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

/* void close (); */
NS_IMETHODIMP nsPPMDecoder::Close()
{
  if (mObserver) {
    mObserver->OnStopFrame(nsnull, nsnull, mFrame);
    mObserver->OnStopContainer(nsnull, nsnull, mImage);
    mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
  }
  
  return NS_OK;
}

/* void flush (); */
NS_IMETHODIMP nsPPMDecoder::Flush()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsPPMDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  nsresult rv;

  if (mBuffer == nsnull) {
    mBuffer = (char *)PR_Malloc(count);
    mBufferSize = count;
  } else if (mBuffer != nsnull && mBufferSize != count) {
    PR_Free(mBuffer);
    mBuffer = (char *)PR_Malloc(count);
    mBufferSize = count;
  }
  if (!mBuffer)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  // read the data from the input stream...
  PRUint32 readLen;
  rv = inStr->Read(mBuffer, count, &readLen);
  if (NS_FAILED(rv)) return rv;

  if (mState == F_P && mObserver)
    mObserver->OnStartDecode(nsnull, nsnull);

  char *p = mBuffer;
  char *bufferEnd = mBuffer+readLen;
  char *s;
  int n;

  while (p < bufferEnd) {
    if (mSkip == WHITESPACE) {
      while (p < bufferEnd && *p != '#'
	      && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
	p++;
      if (p == bufferEnd)
	break;
      if (*p == '#') {
	mOldSkip = WHITESPACE;
	mSkip = TOENDOFLINE;
	continue;
      }
      mSkip = NOTHING;
    } else if (mSkip == SINGLEWHITESPACE) {
      if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
	return NS_ERROR_FAILURE;
      p++;
      mSkip = NOTHING;
    } else if (mSkip == TOENDOFLINE) {
      while (p < bufferEnd && *p != '\n')
	p++;
      if (p == bufferEnd)
	break;
      mSkip = mOldSkip;
      mOldSkip = NOTHING;
      if (mSkip != NOTHING)
	continue;
    }
    switch (mState) {
    case F_P:
      if (*p != 'P')
	return NS_ERROR_FAILURE;
      p++;
      mState = F_TYPE;
      break;
    case F_TYPE:
      if ((s = PL_strchr(ppmTypes, *p)) == NULL)
	return NS_ERROR_FAILURE;
      mType = s-ppmTypes;
      p++;
      mState = F_WIDTH;
      mDigitCount = 0;
      mSkip = WHITESPACE;
      break;
    case F_WIDTH:
    case F_HEIGHT:
    case F_MAXVAL:
      while (mDigitCount < sizeof(mDigits) && p < bufferEnd && isdigit(*p))
	mDigits[mDigitCount++] = *p++;
      if (mDigitCount == sizeof(mDigits))
	return NS_ERROR_FAILURE;		// number too big
      if (p == bufferEnd)
	break;
      if (*p == '#') {
	mOldSkip = NOTHING;
	mSkip = TOENDOFLINE;
	break;
      }
      mDigits[mDigitCount] = 0;
      n = strtol(mDigits, (char **)NULL, 10);
      if (mState == F_WIDTH) {
	mWidth = n;
	mDigitCount = 0;
	mState = F_HEIGHT;
	mSkip = WHITESPACE;
      } else if (mState == F_HEIGHT) {
	mHeight = n;
	mDigitCount = 0;
	if ((mType & PPMTYPE) != TYPE_PBM) {
	  mState = F_MAXVAL;
	  mSkip = WHITESPACE;
	} else
	  mState = F_INITIALIZE;
      } else if (mState == F_MAXVAL) {
	mMaxValue = n;
	mDigitCount = 0;
	mState = F_INITIALIZE;
      }
      break;
    case F_INITIALIZE:
      mImage->Init(mWidth, mHeight, mObserver);
      if (mObserver)
	mObserver->OnStartContainer(nsnull, nsnull, mImage);
      mFrame->Init(0, 0, mWidth, mHeight, gfxIFormats::RGB, 24);
      mImage->AppendFrame(mFrame);
      if (mObserver)
	mObserver->OnStartFrame(nsnull, nsnull, mFrame);
      mRow = 0;
      mBytesPerRow = 3*mWidth;
      mFrame->GetImageBytesPerRow(&mFrameBytesPerRow);
      mRowData = (PRUint8 *)PR_Malloc(mFrameBytesPerRow);
      mRowDataFill = 0;
      if (mType & PPMRAW) {
	mState = F_RAWDATA;
	mSkip = SINGLEWHITESPACE;
      } else if ((mType & PPMTYPE) == TYPE_PBM) {
	mState = F_TEXTBITDATA;
	mSkip = WHITESPACE;
      } else {
	mState = F_TEXTDATA;
	mSkip = WHITESPACE;
	mDigitCount = 0;
      }
      break;
    case F_TEXTBITDATA:
      {
	PRUint8 c = *p++;
	if (c == '1') {
	  mRowData[mRowDataFill++] = 0;
	  mRowData[mRowDataFill++] = 0;
	  mRowData[mRowDataFill++] = 0;
	} else {
	  mRowData[mRowDataFill++] = 255;
	  mRowData[mRowDataFill++] = 255;
	  mRowData[mRowDataFill++] = 255;
	}
      }
      mSkip = WHITESPACE;
      rv = checkSendRow();
      if (NS_FAILED(rv)) return rv;
      break;
    case F_TEXTDATA:
      while (mDigitCount < sizeof(mDigits) && p < bufferEnd && isdigit(*p))
	mDigits[mDigitCount++] = *p++;
      if (mDigitCount == sizeof(mDigits))
	return NS_ERROR_FAILURE;             // number too big
      if (p == bufferEnd)
	break;
      mDigits[mDigitCount] = 0;
      n = strtol(mDigits, (char **)NULL, 10);
      mDigitCount = 0;
      switch (mType & PPMTYPE) {
      case 1:
	mRowData[mRowDataFill++] = n;
	mRowData[mRowDataFill++] = n;
	mRowData[mRowDataFill++] = n;
	break;
      case 2:
	mRowData[mRowDataFill++] = n;
	break;
      }
      mSkip = WHITESPACE;
      rv = checkSendRow();
      if (NS_FAILED(rv)) return rv;
      break;
    case F_RAWDATA:
      if (mType & PPMRAW) {
	switch (mType & PPMTYPE) {
	case TYPE_PBM:
	  {
	    PRUint32 c = *p++;
	    int i = 0;
	    while (mRowDataFill < mBytesPerRow && i < 8) {
	      if (c & 0x80) {
		mRowData[mRowDataFill++] = 0;
		mRowData[mRowDataFill++] = 0;
		mRowData[mRowDataFill++] = 0;
	      } else {
		mRowData[mRowDataFill++] = 255;
		mRowData[mRowDataFill++] = 255;
		mRowData[mRowDataFill++] = 255;
	      }
	      c <<= 1;
	      i++;
	    }
	  }
	  break;
	case TYPE_PGM:
	  {
	    PRUint8 c = *p++;
	    mRowData[mRowDataFill++] = c;
	    mRowData[mRowDataFill++] = c;
	    mRowData[mRowDataFill++] = c;
	  }
	  break;
	case TYPE_PPM:
	  if (mMaxValue == 255) {
	    PRUint32 bytesInBuffer = bufferEnd-p;
	    PRUint32 bytesNeeded = mBytesPerRow-mRowDataFill;
	    PRUint32 chunk = PR_MIN(bytesInBuffer, bytesNeeded);
	    memcpy(mRowData+mRowDataFill, p, chunk);
	    p += chunk;
	    mRowDataFill += chunk;
	  } else {
	    mRowData[mRowDataFill++] = *p++;
	  }
	  break;
	}
	rv = checkSendRow();
	if (NS_FAILED(rv)) return rv;
      }
      break;
    }
  }
  return NS_OK;
}

NS_METHOD nsPPMDecoder::checkSendRow()
{
  nsresult rv;

  if (mRowDataFill == mBytesPerRow) {
    rv = mFrame->SetImageData(mRowData, mFrameBytesPerRow, mRow*mFrameBytesPerRow);
    if (NS_FAILED(rv)) return rv;
    nsRect r(0, mRow, mWidth, 1);
    if (mObserver)
      mObserver->OnDataAvailable(nsnull, nsnull, mFrame, &r);
    mRow++;
    mRowDataFill = 0;
  }
  return NS_OK;
}
