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
 * The Original Code is an implementation of an icon encoder.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#include "nsCRT.h"
#include "Endian.h"
#include "nsBMPEncoder.h"
#include "nsPNGEncoder.h"
#include "nsICOEncoder.h"
#include "prmem.h"
#include "prprf.h"
#include "nsString.h"
#include "nsStreamUtils.h"

using namespace mozilla;
using namespace mozilla::imagelib;

NS_IMPL_THREADSAFE_ISUPPORTS3(nsICOEncoder, imgIEncoder, nsIInputStream, nsIAsyncInputStream)

nsICOEncoder::nsICOEncoder() : mFinished(PR_FALSE),
                               mImageBufferStart(nsnull), 
                               mImageBufferCurr(0),
                               mImageBufferSize(0), 
                               mImageBufferReadPoint(0), 
                               mCallback(nsnull),
                               mCallbackTarget(nsnull), 
                               mNotifyThreshold(0),
                               mUsePNG(PR_TRUE)
{
}

nsICOEncoder::~nsICOEncoder()
{
  if (mImageBufferStart) {
    moz_free(mImageBufferStart);
    mImageBufferStart = nsnull;
    mImageBufferCurr = nsnull;
  }
}

// nsICOEncoder::InitFromData
// Two output options are supported: format=<png|bmp>;bpp=<bpp_value>
// format specifies whether to use png or bitmap format
// bpp specifies the bits per pixel to use where bpp_value can be 24 or 32
NS_IMETHODIMP nsICOEncoder::InitFromData(const PRUint8* aData,
                                         PRUint32 aLength,
                                         PRUint32 aWidth,
                                         PRUint32 aHeight,
                                         PRUint32 aStride,
                                         PRUint32 aInputFormat,
                                         const nsAString& aOutputOptions)
{
  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB) {
    return NS_ERROR_INVALID_ARG;
  }

  // Stride is the padded width of each row, so it better be longer
  if ((aInputFormat == INPUT_FORMAT_RGB &&
       aStride < aWidth * 3) ||
      ((aInputFormat == INPUT_FORMAT_RGBA || aInputFormat == INPUT_FORMAT_HOSTARGB) &&
       aStride < aWidth * 4)) {
    NS_WARNING("Invalid stride for InitFromData");
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  rv = StartImageEncode(aWidth, aHeight, aInputFormat, aOutputOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddImageFrame(aData, aLength, aWidth, aHeight, aStride,
                     aInputFormat, aOutputOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EndImageEncode();
  return rv;
}

// Returns the image buffer size
NS_IMETHODIMP 
nsICOEncoder::GetImageBufferSize(PRUint32 *aOutputSize)
{
  NS_ENSURE_ARG_POINTER(aOutputSize);
  *aOutputSize = mImageBufferSize;
  return NS_OK;
}

// Returns a pointer to the start of the image buffer
NS_IMETHODIMP 
nsICOEncoder::GetImageBuffer(char **aOutputBuffer)
{
  NS_ENSURE_ARG_POINTER(aOutputBuffer);
  *aOutputBuffer = reinterpret_cast<char*>(mImageBufferStart);
  return NS_OK;
}

NS_IMETHODIMP 
nsICOEncoder::AddImageFrame(const PRUint8* aData,
                            PRUint32 aLength,
                            PRUint32 aWidth,
                            PRUint32 aHeight,
                            PRUint32 aStride,
                            PRUint32 aInputFormat, 
                            const nsAString& aFrameOptions)
{
  if (mUsePNG) {

    mContainedEncoder = new nsPNGEncoder();
    nsresult rv;
    nsAutoString noParams;
    rv = mContainedEncoder->InitFromData(aData, aLength, aWidth, aHeight,
                                         aStride, aInputFormat, noParams);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 imageBufferSize;
    mContainedEncoder->GetImageBufferSize(&imageBufferSize);
    mImageBufferSize = ICONFILEHEADERSIZE + ICODIRENTRYSIZE + 
                       imageBufferSize;
    mImageBufferStart = static_cast<PRUint8*>(moz_malloc(mImageBufferSize));
    if (!mImageBufferStart) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mImageBufferCurr = mImageBufferStart;
    mICODirEntry.mBytesInRes = imageBufferSize;

    EncodeFileHeader();
    EncodeInfoHeader();

    char *imageBuffer;
    rv = mContainedEncoder->GetImageBuffer(&imageBuffer);
    NS_ENSURE_SUCCESS(rv, rv);
    memcpy(mImageBufferCurr, imageBuffer, imageBufferSize);
    mImageBufferCurr += imageBufferSize;
  } else {
    mContainedEncoder = new nsBMPEncoder();
    nsresult rv;

    nsAutoString params;
    params.AppendASCII("bpp=");
    params.AppendInt(mICODirEntry.mBitCount);

    rv = mContainedEncoder->InitFromData(aData, aLength, aWidth, aHeight,
                                         aStride, aInputFormat, params);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 andMaskSize = ((mICODirEntry.mWidth + 31) / 32) * 4 * // row AND mask
                           mICODirEntry.mHeight; // num rows

    PRUint32 imageBufferSize;
    mContainedEncoder->GetImageBufferSize(&imageBufferSize);
    mImageBufferSize = ICONFILEHEADERSIZE + ICODIRENTRYSIZE + 
                       imageBufferSize + andMaskSize;
    mImageBufferStart = static_cast<PRUint8*>(moz_malloc(mImageBufferSize));
    if (!mImageBufferStart) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mImageBufferCurr = mImageBufferStart;

    // The icon buffer does not include the BFH at all.
    mICODirEntry.mBytesInRes = imageBufferSize - BFH_LENGTH + andMaskSize;

    // Encode the icon headers
    EncodeFileHeader();
    EncodeInfoHeader();

    char *imageBuffer;
    rv = mContainedEncoder->GetImageBuffer(&imageBuffer);
    NS_ENSURE_SUCCESS(rv, rv);
    memcpy(mImageBufferCurr, imageBuffer + BFH_LENGTH, 
           imageBufferSize - BFH_LENGTH);
    // We need to fix the BMP height to be *2 for the AND mask
    PRUint32 fixedHeight = mICODirEntry.mHeight * 2;
    fixedHeight = NATIVE32_TO_LITTLE(fixedHeight);
    // The height is stored at an offset of 8 from the DIB header
    memcpy(mImageBufferCurr + 8, &fixedHeight, sizeof(fixedHeight));
    mImageBufferCurr += imageBufferSize - BFH_LENGTH;

    // Calculate rowsize in DWORD's
    PRUint32 rowSize = ((mICODirEntry.mWidth + 31) / 32) * 4; // + 31 to round up
    PRInt32 currentLine = mICODirEntry.mHeight;
    
    // Write out the AND mask
    while (currentLine > 0) {
      currentLine--;
      PRUint8* encoded = mImageBufferCurr + currentLine * rowSize;
      PRUint8* encodedEnd = encoded + rowSize;
      while (encoded != encodedEnd) {
        *encoded = 0; // make everything visible
        encoded++;
      }
    }

    mImageBufferCurr += andMaskSize;
  }

  return NS_OK;
}

// See ::InitFromData for other info.
NS_IMETHODIMP nsICOEncoder::StartImageEncode(PRUint32 aWidth,
                                             PRUint32 aHeight,
                                             PRUint32 aInputFormat,
                                             const nsAString& aOutputOptions)
{
  // can't initialize more than once
  if (mImageBufferStart || mImageBufferCurr) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB) {
    return NS_ERROR_INVALID_ARG;
  }

  // Icons are only 1 byte, so make sure our bitmap is in range
  if (aWidth > 255 || aHeight > 255) {
    return NS_ERROR_INVALID_ARG;
  }

  // parse and check any provided output options
  PRUint32 bpp = 24;
  PRBool usePNG = PR_TRUE;
  nsresult rv = ParseOptions(aOutputOptions, &bpp, &usePNG);
  NS_ENSURE_SUCCESS(rv, rv);

  mUsePNG = usePNG;

  InitFileHeader();
  InitInfoHeader(bpp, (PRUint8)aWidth, (PRUint8)aHeight); // range checks above

  return NS_OK;
}

NS_IMETHODIMP nsICOEncoder::EndImageEncode()
{
  // must be initialized
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mFinished = PR_TRUE;
  NotifyListener();

  // if output callback can't get enough memory, it will free our buffer
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

// Parses the encoder options and sets the bits per pixel to use and PNG or BMP
// See InitFromData for a description of the parse options
nsresult
nsICOEncoder::ParseOptions(const nsAString& aOptions, PRUint32* bpp, 
                           PRBool *usePNG)
{
  // If no parsing options just use the default of 24BPP and PNG yes
  if (aOptions.Length() == 0) {
    if (usePNG) {
      *usePNG = PR_TRUE;
    }
    if (bpp) {
      *bpp = 24;
    }
  }

  // Parse the input string into a set of name/value pairs.
  // From format: format=<png|bmp>;bpp=<bpp_value>
  // to format: [0] = format=<png|bmp>, [1] = bpp=<bpp_value>
  nsTArray<nsCString> nameValuePairs;
  if (!ParseString(NS_ConvertUTF16toUTF8(aOptions), ';', nameValuePairs)) {
    return NS_ERROR_INVALID_ARG;
  }

  // For each name/value pair in the set
  for (int i = 0; i < nameValuePairs.Length(); ++i) {

    // Split the name value pair [0] = name, [1] = value
    nsTArray<nsCString> nameValuePair;
    if (!ParseString(nameValuePairs[i], '=', nameValuePair)) {
      return NS_ERROR_INVALID_ARG;
    }
    if (nameValuePair.Length() != 2) {
      return NS_ERROR_INVALID_ARG;
    }

    // Parse the format portion of the string format=<png|bmp>;bpp=<bpp_value>
    if (nameValuePair[0].Equals("format", nsCaseInsensitiveCStringComparator())) {
      if (nameValuePair[1].Equals("png", nsCaseInsensitiveCStringComparator())) {
        *usePNG = PR_TRUE;
      }
      else if (nameValuePair[1].Equals("bmp", nsCaseInsensitiveCStringComparator())) {
        *usePNG = PR_FALSE;
      }
      else {
        return NS_ERROR_INVALID_ARG;
      }
    }

    // Parse the bpp portion of the string format=<png|bmp>;bpp=<bpp_value>
    if (nameValuePair[0].Equals("bpp", nsCaseInsensitiveCStringComparator())) {
      if (nameValuePair[1].Equals("24")) {
        *bpp = 24;
      }
      else if (nameValuePair[1].Equals("32")) {
        *bpp = 32;
      }
      else {
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsICOEncoder::Close()
{
  if (mImageBufferStart) {
    moz_free(mImageBufferStart);
    mImageBufferStart = nsnull;
    mImageBufferSize = 0;
    mImageBufferReadPoint = 0;
    mImageBufferCurr = nsnull;
  }

  return NS_OK;
}

// Obtains the available bytes to read
NS_IMETHODIMP nsICOEncoder::Available(PRUint32 *_retval)
{
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = GetCurrentImageBufferOffset() - mImageBufferReadPoint;
  return NS_OK;
}

// [noscript] Reads bytes which are available
NS_IMETHODIMP nsICOEncoder::Read(char *aBuf, PRUint32 aCount,
                                 PRUint32 *_retval)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

// [noscript] Reads segments
NS_IMETHODIMP nsICOEncoder::ReadSegments(nsWriteSegmentFun aWriter,
                                         void *aClosure, PRUint32 aCount,
                                         PRUint32 *_retval)
{
  PRUint32 maxCount = GetCurrentImageBufferOffset() - mImageBufferReadPoint;
  if (maxCount == 0) {
    *_retval = 0;
    return mFinished ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
  }

  if (aCount > maxCount) {
    aCount = maxCount;
  }

  nsresult rv = aWriter(this, aClosure,
                        reinterpret_cast<const char*>(mImageBufferStart + 
                                                      mImageBufferReadPoint),
                        0, aCount, _retval);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*_retval <= aCount, "bad write count");
    mImageBufferReadPoint += *_retval;
  }
  // errors returned from the writer end here!
  return NS_OK;
}

NS_IMETHODIMP 
nsICOEncoder::IsNonBlocking(PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
nsICOEncoder::AsyncWait(nsIInputStreamCallback *aCallback,
                        PRUint32 aFlags,
                        PRUint32 aRequestedCount,
                        nsIEventTarget *aTarget)
{
  if (aFlags != 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mCallback || mCallbackTarget) {
    return NS_ERROR_UNEXPECTED;
  }

  mCallbackTarget = aTarget;
  // 0 means "any number of bytes except 0"
  mNotifyThreshold = aRequestedCount;
  if (!aRequestedCount) {
    mNotifyThreshold = 1024; // We don't want to notify incessantly
  }

  // We set the callback absolutely last, because NotifyListener uses it to
  // determine if someone needs to be notified.  If we don't set it last,
  // NotifyListener might try to fire off a notification to a null target
  // which will generally cause non-threadsafe objects to be used off the main thread
  mCallback = aCallback;

  // What we are being asked for may be present already
  NotifyListener();
  return NS_OK;
}

NS_IMETHODIMP nsICOEncoder::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

void
nsICOEncoder::NotifyListener()
{
  if (mCallback &&
      (GetCurrentImageBufferOffset() - mImageBufferReadPoint >= mNotifyThreshold ||
       mFinished)) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      NS_NewInputStreamReadyEvent(getter_AddRefs(callback),
                                  mCallback,
                                  mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback");
    // Null the callback first because OnInputStreamReady could reenter
    // AsyncWait
    mCallback = nsnull;
    mCallbackTarget = nsnull;
    mNotifyThreshold = 0;

    callback->OnInputStreamReady(this);
  }
}

// Initializes the icon file header mICOFileHeader
void 
nsICOEncoder::InitFileHeader()
{
  memset(&mICOFileHeader, 0, sizeof(mICOFileHeader));
  mICOFileHeader.mReserved = 0;
  mICOFileHeader.mType = 1;
  mICOFileHeader.mCount = 1;
}

// Initializes the icon directory info header mICODirEntry
void 
nsICOEncoder::InitInfoHeader(PRUint32 aBPP, PRUint8 aWidth, PRUint8 aHeight)
{
  memset(&mICODirEntry, 0, sizeof(mICODirEntry));
  mICODirEntry.mBitCount = aBPP;
  mICODirEntry.mBytesInRes = 0;
  mICODirEntry.mColorCount = 0;
  mICODirEntry.mWidth = aWidth;
  mICODirEntry.mHeight = aHeight;
  mICODirEntry.mImageOffset = ICONFILEHEADERSIZE + ICODIRENTRYSIZE;
  mICODirEntry.mPlanes = 1;
  mICODirEntry.mReserved = 0;
}

// Encodes the icon file header mICOFileHeader
void 
nsICOEncoder::EncodeFileHeader() 
{  
  IconFileHeader littleEndianIFH = mICOFileHeader;
  littleEndianIFH.mReserved = NATIVE16_TO_LITTLE(littleEndianIFH.mReserved);
  littleEndianIFH.mType = NATIVE16_TO_LITTLE(littleEndianIFH.mType);
  littleEndianIFH.mCount = NATIVE16_TO_LITTLE(littleEndianIFH.mCount);

  memcpy(mImageBufferCurr, &littleEndianIFH.mReserved, 
         sizeof(littleEndianIFH.mReserved));
  mImageBufferCurr += sizeof(littleEndianIFH.mReserved);
  memcpy(mImageBufferCurr, &littleEndianIFH.mType, 
         sizeof(littleEndianIFH.mType));
  mImageBufferCurr += sizeof(littleEndianIFH.mType);
  memcpy(mImageBufferCurr, &littleEndianIFH.mCount, 
         sizeof(littleEndianIFH.mCount));
  mImageBufferCurr += sizeof(littleEndianIFH.mCount);
}

// Encodes the icon directory info header mICODirEntry
void 
nsICOEncoder::EncodeInfoHeader()
{
  IconDirEntry littleEndianmIDE = mICODirEntry;

  littleEndianmIDE.mPlanes = NATIVE16_TO_LITTLE(littleEndianmIDE.mPlanes);
  littleEndianmIDE.mBitCount = NATIVE16_TO_LITTLE(littleEndianmIDE.mBitCount);
  littleEndianmIDE.mBytesInRes = 
    NATIVE32_TO_LITTLE(littleEndianmIDE.mBytesInRes);
  littleEndianmIDE.mImageOffset  = 
    NATIVE32_TO_LITTLE(littleEndianmIDE.mImageOffset);

  memcpy(mImageBufferCurr, &littleEndianmIDE.mWidth, 
         sizeof(littleEndianmIDE.mWidth));
  mImageBufferCurr += sizeof(littleEndianmIDE.mWidth);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mHeight, 
         sizeof(littleEndianmIDE.mHeight));
  mImageBufferCurr += sizeof(littleEndianmIDE.mHeight);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mColorCount, 
         sizeof(littleEndianmIDE.mColorCount));
  mImageBufferCurr += sizeof(littleEndianmIDE.mColorCount);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mReserved, 
         sizeof(littleEndianmIDE.mReserved));
  mImageBufferCurr += sizeof(littleEndianmIDE.mReserved);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mPlanes, 
         sizeof(littleEndianmIDE.mPlanes));
  mImageBufferCurr += sizeof(littleEndianmIDE.mPlanes);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mBitCount, 
         sizeof(littleEndianmIDE.mBitCount));
  mImageBufferCurr += sizeof(littleEndianmIDE.mBitCount);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mBytesInRes, 
         sizeof(littleEndianmIDE.mBytesInRes));
  mImageBufferCurr += sizeof(littleEndianmIDE.mBytesInRes);
  memcpy(mImageBufferCurr, &littleEndianmIDE.mImageOffset, 
         sizeof(littleEndianmIDE.mImageOffset));
  mImageBufferCurr += sizeof(littleEndianmIDE.mImageOffset);
}
