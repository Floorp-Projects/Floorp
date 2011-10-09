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
 * The Original Code is an implementation of a bitmap encoder.
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
#include "EndianMacros.h"
#include "nsBMPEncoder.h"
#include "prmem.h"
#include "prprf.h"
#include "nsString.h"
#include "nsStreamUtils.h"
#include "nsAutoPtr.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS3(nsBMPEncoder, imgIEncoder, nsIInputStream, nsIAsyncInputStream)

nsBMPEncoder::nsBMPEncoder() : mImageBufferStart(nsnull), 
                               mImageBufferCurr(0),
                               mImageBufferSize(0), 
                               mImageBufferReadPoint(0), 
                               mFinished(PR_FALSE),
                               mCallback(nsnull), 
                               mCallbackTarget(nsnull), 
                               mNotifyThreshold(0)
{
}

nsBMPEncoder::~nsBMPEncoder()
{
  if (mImageBufferStart) {
    moz_free(mImageBufferStart);
    mImageBufferStart = nsnull;
    mImageBufferCurr = nsnull;
  }
}

// nsBMPEncoder::InitFromData
//
// One output option is supported: bpp=<bpp_value>
// bpp specifies the bits per pixel to use where bpp_value can be 24 or 32
NS_IMETHODIMP nsBMPEncoder::InitFromData(const PRUint8* aData,
                                         PRUint32 aLength, // (unused,
                                                           // req'd by JS)
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
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = AddImageFrame(aData, aLength, aWidth, aHeight, aStride,
                     aInputFormat, aOutputOptions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = EndImageEncode();
  return rv;
}

// Just a helper method to make it explicit in calculations that we are dealing
// with bytes and not bits
static inline PRUint32
BytesPerPixel(PRUint32 aBPP)
{
  return aBPP / 8;
}

// Calculates the number of padding bytes that are needed per row of image data
static inline PRUint32
PaddingBytes(PRUint32 aBPP, PRUint32 aWidth)
{
  PRUint32 rowSize = aWidth * BytesPerPixel(aBPP);
  PRUint8 paddingSize = 0;
  if(rowSize % 4) {
    paddingSize = (4 - (rowSize % 4));
  }
  return paddingSize;
}

// See ::InitFromData for other info.
NS_IMETHODIMP nsBMPEncoder::StartImageEncode(PRUint32 aWidth,
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

  // parse and check any provided output options
  PRUint32 bpp;
  nsresult rv = ParseOptions(aOutputOptions, &bpp);
  if (NS_FAILED(rv)) {
    return rv;
  }

  InitFileHeader(bpp, aWidth, aHeight);
  InitInfoHeader(bpp, aWidth, aHeight);

  mImageBufferSize = mBMPFileHeader.filesize;
  mImageBufferStart = static_cast<PRUint8*>(moz_malloc(mImageBufferSize));
  if (!mImageBufferStart) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mImageBufferCurr = mImageBufferStart;

  EncodeFileHeader();
  EncodeInfoHeader();

  return NS_OK;
}

// Returns the image buffer size
NS_IMETHODIMP nsBMPEncoder::GetImageBufferSize(PRUint32 *aOutputSize)
{
  NS_ENSURE_ARG_POINTER(aOutputSize);
  *aOutputSize = mImageBufferSize;
  return NS_OK;
}

// Returns a pointer to the start of the image buffer
NS_IMETHODIMP nsBMPEncoder::GetImageBuffer(char **aOutputBuffer)
{
  NS_ENSURE_ARG_POINTER(aOutputBuffer);
  *aOutputBuffer = reinterpret_cast<char*>(mImageBufferStart);
  return NS_OK;
}

NS_IMETHODIMP nsBMPEncoder::AddImageFrame(const PRUint8* aData,
                                          PRUint32 aLength, // (unused,
                                                            // req'd by JS)
                                          PRUint32 aWidth,
                                          PRUint32 aHeight,
                                          PRUint32 aStride,
                                          PRUint32 aInputFormat,
                                          const nsAString& aFrameOptions)
{
  // must be initialized
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB) {
    return NS_ERROR_INVALID_ARG;
  }

  static fallible_t fallible = fallible_t();
  nsAutoArrayPtr<PRUint8> row(new (fallible) 
                              PRUint8[mBMPInfoHeader.width * 
                              BytesPerPixel(mBMPInfoHeader.bpp)]);
  if (!row) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // write each row: if we add more input formats, we may want to
  // generalize the conversions
  if (aInputFormat == INPUT_FORMAT_HOSTARGB) {
    // BMP requires RGBA with post-multiplied alpha, so we need to convert
    for (PRInt32 y = mBMPInfoHeader.height - 1; y >= 0 ; y --) {
      ConvertHostARGBRow(&aData[y * aStride], row, mBMPInfoHeader.width);
      if(mBMPInfoHeader.bpp == 24) {
        EncodeImageDataRow24(row);
      } else {
        EncodeImageDataRow32(row);
      }
    }
  } else if (aInputFormat == INPUT_FORMAT_RGBA) {
    // RBGA, but we need to strip the alpha
    for (PRInt32 y = 0; y < mBMPInfoHeader.height; y ++) {
      StripAlpha(&aData[y * aStride], row, mBMPInfoHeader.width);
      if (mBMPInfoHeader.bpp == 24) {
        EncodeImageDataRow24(row);
      } else {
        EncodeImageDataRow32(row);
      }
    }
  } else if (aInputFormat == INPUT_FORMAT_RGB) {
    // simple RBG(A), no conversion needed
    for (PRInt32 y = 0; y < mBMPInfoHeader.height; y ++) {
      if (mBMPInfoHeader.bpp == 24) {
        EncodeImageDataRow24(&aData[y * aStride]);
      } else { 
        EncodeImageDataRow32(&aData[y * aStride]);
      }
    }
  } else {
    NS_NOTREACHED("Bad format type");
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}


NS_IMETHODIMP nsBMPEncoder::EndImageEncode()
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


// Parses the encoder options and sets the bits per pixel to use
// See InitFromData for a description of the parse options
nsresult
nsBMPEncoder::ParseOptions(const nsAString& aOptions, PRUint32* bpp)
{
  // If no parsing options just use the default of 24BPP
  if (aOptions.Length() == 0) {
    if (bpp) {
      *bpp = 24;
    }
    return NS_OK;
  }

  // Parse the input string into a set of name/value pairs.
  // From a format like: name=value;bpp=<bpp_value>;name=value
  // to format: [0] = name=value, [1] = bpp=<bpp_value>, [2] = name=value
  nsTArray<nsCString> nameValuePairs;
  if (!ParseString(NS_ConvertUTF16toUTF8(aOptions), ';', nameValuePairs)) {
    return NS_ERROR_INVALID_ARG;
  }

  // For each name/value pair in the set
  for (PRUint32 i = 0; i < nameValuePairs.Length(); ++i) {

    // Split the name value pair [0] = name, [1] = value
    nsTArray<nsCString> nameValuePair;
    if (!ParseString(nameValuePairs[i], '=', nameValuePair)) {
      return NS_ERROR_INVALID_ARG;
    }
    if (nameValuePair.Length() != 2) {
      return NS_ERROR_INVALID_ARG;
    }

    // Parse the bpp portion of the string name=value;bpp=<bpp_value>;name=value
    if (nameValuePair[0].Equals("bpp", nsCaseInsensitiveCStringComparator())) {
      if (nameValuePair[1].Equals("24")) {
        *bpp = 24;
      } else if (nameValuePair[1].Equals("32")) {
        *bpp = 32;
      } else {
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsBMPEncoder::Close()
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
NS_IMETHODIMP nsBMPEncoder::Available(PRUint32 *_retval)
{
  if (!mImageBufferStart || !mImageBufferCurr) {
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = GetCurrentImageBufferOffset() - mImageBufferReadPoint;
  return NS_OK;
}

// [noscript] Reads bytes which are available
NS_IMETHODIMP nsBMPEncoder::Read(char * aBuf, PRUint32 aCount,
                                 PRUint32 *_retval)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

// [noscript] Reads segments
NS_IMETHODIMP nsBMPEncoder::ReadSegments(nsWriteSegmentFun aWriter,
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
nsBMPEncoder::IsNonBlocking(bool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
nsBMPEncoder::AsyncWait(nsIInputStreamCallback *aCallback,
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

NS_IMETHODIMP nsBMPEncoder::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

// nsBMPEncoder::ConvertHostARGBRow
//
//    Our colors are stored with premultiplied alphas, but we need
//    an output with no alpha in machine-independent byte order.
//
void
nsBMPEncoder::ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                                 PRUint32 aPixelWidth)
{
  for (PRUint32 x = 0; x < aPixelWidth; x ++) {
    const PRUint32& pixelIn = ((const PRUint32*)(aSrc))[x];
    PRUint8 *pixelOut = &aDest[x * BytesPerPixel(mBMPInfoHeader.bpp)];

    PRUint8 alpha = (pixelIn & 0xff000000) >> 24;
    if (alpha == 0) {
      pixelOut[0] = pixelOut[1] = pixelOut[2] = 0;
    } else {
      pixelOut[0] = (((pixelIn & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
      pixelOut[1] = (((pixelIn & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
      pixelOut[2] = (((pixelIn & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
      if(mBMPInfoHeader.bpp == 32) {
        pixelOut[3] = alpha;
      }
    }
  }
}

// nsBMPEncoder::StripAlpha
//
//    Input is RGBA, output is RGB
void
nsBMPEncoder::StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
                         PRUint32 aPixelWidth)
{
  for (PRUint32 x = 0; x < aPixelWidth; x ++) {
    const PRUint8* pixelIn = &aSrc[x * 4];
    PRUint8* pixelOut = &aDest[x * 3];
    pixelOut[0] = pixelIn[0];
    pixelOut[1] = pixelIn[1];
    pixelOut[2] = pixelIn[2];
  }
}

void
nsBMPEncoder::NotifyListener()
{
  if (mCallback &&
      (GetCurrentImageBufferOffset() - mImageBufferReadPoint >= 
       mNotifyThreshold || mFinished)) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      NS_NewInputStreamReadyEvent(getter_AddRefs(callback),
                                  mCallback,
                                  mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback");
    // Null the callback first because OnInputStreamReady could
    // reenter AsyncWait
    mCallback = nsnull;
    mCallbackTarget = nsnull;
    mNotifyThreshold = 0;

    callback->OnInputStreamReady(this);
  }
}

// Initializes the BMP file header mBMPFileHeader to the passed in values
void 
nsBMPEncoder::InitFileHeader(PRUint32 aBPP, PRUint32 aWidth, PRUint32 aHeight)
{
  memset(&mBMPFileHeader, 0, sizeof(mBMPFileHeader));
  mBMPFileHeader.signature[0] = 'B';
  mBMPFileHeader.signature[1] = 'M';
  
  mBMPFileHeader.dataoffset = WIN_HEADER_LENGTH;

  // The color table is present only if BPP is <= 8
  if (aBPP <= 8) {
    PRUint32 numColors = 1 << aBPP;
    mBMPFileHeader.dataoffset += 4 * numColors;
    mBMPFileHeader.filesize = mBMPFileHeader.dataoffset + aWidth * aHeight;
  } else {
    mBMPFileHeader.filesize = mBMPFileHeader.dataoffset + (aWidth * 
                              BytesPerPixel(aBPP) + PaddingBytes(aBPP, aWidth)) *
                              aHeight;
  }

  mBMPFileHeader.reserved = 0;
  mBMPFileHeader.bihsize = WIN_BIH_LENGTH;
}

// Initializes the bitmap info header mBMPInfoHeader to the passed in values
void 
nsBMPEncoder::InitInfoHeader(PRUint32 aBPP, PRUint32 aWidth, PRUint32 aHeight)
{
  memset(&mBMPInfoHeader, 0, sizeof(mBMPInfoHeader));
  mBMPInfoHeader.bpp =  aBPP;
  mBMPInfoHeader.planes = 1;
  mBMPInfoHeader.colors = 0;
  mBMPInfoHeader.important_colors = 0;
  mBMPInfoHeader.width = aWidth;
  mBMPInfoHeader.height = aHeight;
  mBMPInfoHeader.compression = 0;
  if (aBPP <= 8) {
    mBMPInfoHeader.image_size = aWidth * aHeight;
  } else {
    mBMPInfoHeader.image_size = (aWidth * BytesPerPixel(aBPP) + 
                                 PaddingBytes(aBPP, aWidth)) * aHeight;
  }
  mBMPInfoHeader.xppm = 0;
  mBMPInfoHeader.yppm = 0;
}

// Encodes the BMP file header mBMPFileHeader
void 
nsBMPEncoder::EncodeFileHeader() 
{  
  mozilla::imagelib::BMPFILEHEADER littleEndianBFH = mBMPFileHeader;
  littleEndianBFH.filesize = NATIVE32_TO_LITTLE(littleEndianBFH.filesize);
  littleEndianBFH.reserved = NATIVE32_TO_LITTLE(littleEndianBFH.reserved);
  littleEndianBFH.dataoffset= NATIVE32_TO_LITTLE(littleEndianBFH.dataoffset);
  littleEndianBFH.bihsize = NATIVE32_TO_LITTLE(littleEndianBFH.bihsize);

  memcpy(mImageBufferCurr, &littleEndianBFH.signature, 
         sizeof(littleEndianBFH.signature));
  mImageBufferCurr += sizeof(littleEndianBFH.signature);
  memcpy(mImageBufferCurr, &littleEndianBFH.filesize, 
         sizeof(littleEndianBFH.filesize));
  mImageBufferCurr += sizeof(littleEndianBFH.filesize);
  memcpy(mImageBufferCurr, &littleEndianBFH.reserved, 
         sizeof(littleEndianBFH.reserved));
  mImageBufferCurr += sizeof(littleEndianBFH.reserved);
  memcpy(mImageBufferCurr, &littleEndianBFH.dataoffset, 
         sizeof(littleEndianBFH.dataoffset));
  mImageBufferCurr += sizeof(littleEndianBFH.dataoffset);
  memcpy(mImageBufferCurr, &littleEndianBFH.bihsize, 
         sizeof(littleEndianBFH.bihsize));
  mImageBufferCurr += sizeof(littleEndianBFH.bihsize);
}

// Encodes the BMP infor header mBMPInfoHeader
void 
nsBMPEncoder::EncodeInfoHeader()
{
  mozilla::imagelib::BMPINFOHEADER littleEndianmBIH = mBMPInfoHeader;
  littleEndianmBIH.width =  NATIVE32_TO_LITTLE(littleEndianmBIH.width);
  littleEndianmBIH.height = NATIVE32_TO_LITTLE(littleEndianmBIH.height); 
  littleEndianmBIH.planes = NATIVE16_TO_LITTLE(littleEndianmBIH.planes);
  littleEndianmBIH.bpp = NATIVE16_TO_LITTLE(littleEndianmBIH.bpp);
  littleEndianmBIH.compression = NATIVE32_TO_LITTLE(
                                 littleEndianmBIH.compression);
  littleEndianmBIH.image_size = NATIVE32_TO_LITTLE(
                                littleEndianmBIH.image_size);
  littleEndianmBIH.xppm = NATIVE32_TO_LITTLE(littleEndianmBIH.xppm);
  littleEndianmBIH.yppm = NATIVE32_TO_LITTLE(littleEndianmBIH.yppm);
  littleEndianmBIH.colors = NATIVE32_TO_LITTLE(littleEndianmBIH.colors);
  littleEndianmBIH.important_colors = NATIVE32_TO_LITTLE(
                                      littleEndianmBIH.important_colors);

  if (mBMPFileHeader.bihsize == 12) { // OS/2 Bitmap
    memcpy(mImageBufferCurr, &littleEndianmBIH.width, 2);
    mImageBufferCurr += 2; // Uint16 in OS/2 BMPs
    memcpy(mImageBufferCurr, &littleEndianmBIH.height, 2);
    mImageBufferCurr += 2; // Uint16 in OS/2 BMPs
    memcpy(mImageBufferCurr, &littleEndianmBIH.planes, 
           sizeof(littleEndianmBIH.planes));
    mImageBufferCurr += sizeof(littleEndianmBIH.planes);
    memcpy(mImageBufferCurr, &littleEndianmBIH.bpp, 
           sizeof(littleEndianmBIH.bpp));
    mImageBufferCurr += sizeof(littleEndianmBIH.bpp);
  }
  else {
    memcpy(mImageBufferCurr, &littleEndianmBIH.width, 
           sizeof(littleEndianmBIH.width));
    mImageBufferCurr += sizeof(littleEndianmBIH.width);
    memcpy(mImageBufferCurr, &littleEndianmBIH.height, 
           sizeof(littleEndianmBIH.height));
    mImageBufferCurr += sizeof(littleEndianmBIH.height);
    memcpy(mImageBufferCurr, &littleEndianmBIH.planes, 
           sizeof(littleEndianmBIH.planes));
    mImageBufferCurr += sizeof(littleEndianmBIH.planes);
    memcpy(mImageBufferCurr, &littleEndianmBIH.bpp, 
           sizeof(littleEndianmBIH.bpp));
    mImageBufferCurr += sizeof(littleEndianmBIH.bpp);
    memcpy(mImageBufferCurr, &littleEndianmBIH.compression, 
           sizeof(littleEndianmBIH.compression));
    mImageBufferCurr += sizeof(littleEndianmBIH.compression);
    memcpy(mImageBufferCurr, &littleEndianmBIH.image_size, 
           sizeof(littleEndianmBIH.image_size));
    mImageBufferCurr += sizeof(littleEndianmBIH.image_size);
    memcpy(mImageBufferCurr, &littleEndianmBIH.xppm, 
           sizeof(littleEndianmBIH.xppm));
    mImageBufferCurr += sizeof(littleEndianmBIH.xppm);
    memcpy(mImageBufferCurr, &littleEndianmBIH.yppm, 
           sizeof(littleEndianmBIH.yppm));
    mImageBufferCurr += sizeof(littleEndianmBIH.yppm);
    memcpy(mImageBufferCurr, &littleEndianmBIH.colors, 
           sizeof(littleEndianmBIH.colors));
    mImageBufferCurr += sizeof(littleEndianmBIH.colors);
    memcpy(mImageBufferCurr, &littleEndianmBIH.important_colors, 
           sizeof(littleEndianmBIH.important_colors));
    mImageBufferCurr += sizeof(littleEndianmBIH.important_colors);
  }
}

// Sets a pixel in the image buffer that doesn't have alpha data
static inline void 
  SetPixel24(PRUint8*& imageBufferCurr, PRUint8 aRed, PRUint8 aGreen, 
  PRUint8 aBlue)
{
  *imageBufferCurr = aBlue;
  *(imageBufferCurr + 1) = aGreen;
  *(imageBufferCurr + 2) = aRed;
}

// Sets a pixel in the image buffer with alpha data
static inline void 
SetPixel32(PRUint8*& imageBufferCurr, PRUint8 aRed, PRUint8 aGreen, 
           PRUint8 aBlue, PRUint8 aAlpha = 0xFF)
{
  *imageBufferCurr = aBlue;
  *(imageBufferCurr + 1) = aGreen;
  *(imageBufferCurr + 2) = aRed;
  *(imageBufferCurr + 3) = aAlpha;
}

// Encodes a row of image data which does not have alpha data
void 
nsBMPEncoder::EncodeImageDataRow24(const PRUint8* aData)
{
  for (PRInt32 x = 0; x < mBMPInfoHeader.width; x ++) {
    PRUint32 pos = x * BytesPerPixel(mBMPInfoHeader.bpp);
    SetPixel24(mImageBufferCurr, aData[pos], aData[pos + 1], aData[pos + 2]);
    mImageBufferCurr += BytesPerPixel(mBMPInfoHeader.bpp);
  }
  
  for (PRUint32 x = 0; x < PaddingBytes(mBMPInfoHeader.bpp, 
                                        mBMPInfoHeader.width); x ++) {
    *mImageBufferCurr++ = 0;
  }
}

// Encodes a row of image data which does have alpha data
void 
nsBMPEncoder::EncodeImageDataRow32(const PRUint8* aData)
{
  for (PRInt32 x = 0; x < mBMPInfoHeader.width; x ++) {
    PRUint32 pos = x * BytesPerPixel(mBMPInfoHeader.bpp);
    SetPixel32(mImageBufferCurr, aData[pos], aData[pos + 1], 
               aData[pos + 2], aData[pos + 3]);
    mImageBufferCurr += 4;
  }

  for (PRUint32 x = 0; x < PaddingBytes(mBMPInfoHeader.bpp, 
                                        mBMPInfoHeader.width); x ++) {
    *mImageBufferCurr++ = 0;
  }
}
