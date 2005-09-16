/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is PNG Encoding code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "nsPNGEncoder.h"
#include "prmem.h"
#include "nsString.h"
#include "nsDependentSubstring.h"

// Bug 308126 - AIX defines jmpbuf in sys/context.h which conflicts with the
// definition of jmpbuf in the png.h header file.
#ifdef jmpbuf
#undef jmpbuf
#endif

NS_IMPL_ISUPPORTS2(nsPNGEncoder, imgIEncoder, nsIInputStream)

nsPNGEncoder::nsPNGEncoder() : mImageBuffer(nsnull), mImageBufferSize(0),
                                 mImageBufferUsed(0), mImageBufferReadPoint(0)
{
}

nsPNGEncoder::~nsPNGEncoder()
{
  if (mImageBuffer) {
    PR_Free(mImageBuffer);
    mImageBuffer = nsnull;
  }
}

// nsPNGEncoder::InitFromData
//
//    One output option is supported: "transparency=none" means that the
//    output PNG will not have an alpha channel, even if the input does.
//
//    Based partially on gfx/cairo/cairo/src/cairo-png.c
//    See also modules/libimg/png/libpng.txt

NS_IMETHODIMP nsPNGEncoder::InitFromData(const PRUint8* aData,
                                          PRUint32 aLength, // (unused, req'd by JS)
                                          PRUint32 aWidth,
                                          PRUint32 aHeight,
                                          PRUint32 aStride,
                                          PRUint32 aInputFormat,
                                          const nsAString& aOutputOptions)
{
  // validate input format
  if (aInputFormat != INPUT_FORMAT_RGB &&
      aInputFormat != INPUT_FORMAT_RGBA &&
      aInputFormat != INPUT_FORMAT_HOSTARGB)
    return NS_ERROR_INVALID_ARG;

  // Stride is the padded width of each row, so it better be longer (I'm afraid
  // people will not understand what stride means, so check it well)
  if ((aInputFormat == INPUT_FORMAT_RGB &&
       aStride < aWidth * 3) ||
      ((aInputFormat == INPUT_FORMAT_RGBA || aInputFormat == INPUT_FORMAT_HOSTARGB) &&
       aStride < aWidth * 4)) {
    NS_WARNING("Invalid stride for InitFromData");
    return NS_ERROR_INVALID_ARG;
  }

  // can't initialize more than once
  if (mImageBuffer != nsnull)
    return NS_ERROR_ALREADY_INITIALIZED;

  // options: we only have one option so this is easy
  PRBool useTransparency = PR_TRUE;
  if (aOutputOptions.Length() >= 17) {
    if (StringBeginsWith(aOutputOptions,  NS_LITERAL_STRING("transparency=none")))
      useTransparency = PR_FALSE;
  }

  // initialize
  png_struct* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                png_voidp_NULL,
                                                png_error_ptr_NULL,
                                                png_error_ptr_NULL);
  if (! png_ptr)
    return NS_ERROR_OUT_OF_MEMORY;
  png_info* info_ptr = png_create_info_struct(png_ptr);
  if (! info_ptr)
  {
    png_destroy_write_struct(&png_ptr, nsnull);
    return NS_ERROR_FAILURE;
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Set up to read the data into our image buffer, start out with an 8K
  // estimated size. Note: we don't have to worry about freeing this data
  // in this function. It will be freed on object destruction.
  mImageBufferSize = 8192;
  mImageBuffer = (PRUint8*)PR_Malloc(mImageBufferSize);
  if (!mImageBuffer) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mImageBufferUsed = 0;

  // set our callback for libpng to give us the data
  png_set_write_fn(png_ptr, this, WriteCallback, NULL);

  // include alpha?
  int colorType;
  if ((aInputFormat == INPUT_FORMAT_HOSTARGB ||
       aInputFormat == INPUT_FORMAT_RGBA)  &&  useTransparency)
    colorType = PNG_COLOR_TYPE_RGB_ALPHA;
  else
    colorType = PNG_COLOR_TYPE_RGB;

  png_set_IHDR(png_ptr, info_ptr, aWidth, aHeight, 8, colorType,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr, info_ptr);

  // write each row: if we add more input formats, we may want to
  // generalize the conversions
  if (aInputFormat == INPUT_FORMAT_HOSTARGB) {
    // PNG requires RGBA with post-multiplied alpha, so we need to convert
    PRUint8* row = new PRUint8[aWidth * 4];
    for (PRUint32 y = 0; y < aHeight; y ++) {
      ConvertHostARGBRow(&aData[y * aStride], row, aWidth, useTransparency);
      png_write_row(png_ptr, row);
    }
    delete[] row;

  } else if (aInputFormat == INPUT_FORMAT_RGBA && ! useTransparency) {
    // RBGA, but we need to strip the alpha
    PRUint8* row = new PRUint8[aWidth * 4];
    for (PRUint32 y = 0; y < aHeight; y ++) {
      StripAlpha(&aData[y * aStride], row, aWidth);
      png_write_row(png_ptr, row);
    }
    delete[] row;

  } else if (aInputFormat == INPUT_FORMAT_RGB ||
             aInputFormat == INPUT_FORMAT_RGBA) {
    // simple RBG(A), no conversion needed
    for (PRUint32 y = 0; y < aHeight; y ++) {
      png_write_row(png_ptr, (PRUint8*)&aData[y * aStride]);
    }

  } else {
    NS_NOTREACHED("Bad format type");
  }

  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  // if output callback can't get enough memory, it will free our buffer
  if (mImageBuffer)
    return NS_OK;
  else
    return NS_ERROR_OUT_OF_MEMORY;
}


/* void close (); */
NS_IMETHODIMP nsPNGEncoder::Close()
{
  if (mImageBuffer != nsnull) {
    PR_Free(mImageBuffer);
    mImageBuffer = nsnull;
    mImageBufferSize = 0;
    mImageBufferUsed = 0;
  }
  return NS_OK;
}

/* unsigned long available (); */
NS_IMETHODIMP nsPNGEncoder::Available(PRUint32 *_retval)
{
  *_retval = mImageBufferUsed - mImageBufferReadPoint;
  return NS_OK;
}

/* [noscript] unsigned long read (in charPtr aBuf, in unsigned long aCount); */
NS_IMETHODIMP nsPNGEncoder::Read(char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
  if (mImageBufferReadPoint >= mImageBufferUsed) {
    // done reading
    *_retval = 0;
    return NS_OK;
  }

  PRUint32 toRead = mImageBufferUsed - mImageBufferReadPoint;
  if (aCount < toRead)
    toRead = aCount;

  memcpy(aBuf, &mImageBuffer[mImageBufferReadPoint], toRead);

  mImageBufferReadPoint += toRead;
  *_retval = toRead;
  return NS_OK;
}

/* [noscript] unsigned long readSegments (in nsWriteSegmentFun aWriter, in voidPtr aClosure, in unsigned long aCount); */
NS_IMETHODIMP nsPNGEncoder::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, PRUint32 aCount, PRUint32 *_retval)
{
  if (mImageBufferUsed == 0)
    return NS_ERROR_FAILURE;

  PRUint32 writeCount = 0;
  aWriter(this, aClosure, (const char*)mImageBuffer, 0, mImageBufferUsed, &writeCount);

  if (writeCount > 0)
    return NS_OK;
  else
    return NS_ERROR_FAILURE; // some problem with callback
}

/* boolean isNonBlocking (); */
NS_IMETHODIMP nsPNGEncoder::IsNonBlocking(PRBool *_retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}


// nsPNGEncoder::ConvertHostARGBRow
//
//    Our colors are stored with premultiplied alphas, but PNGs use
//    post-multiplied alpha. This swaps to PNG-style alpha.
//
//    Copied from gfx/cairo/cairo/src/cairo-png.c

void
nsPNGEncoder::ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                                  PRUint32 aPixelWidth, PRBool aUseTransparency)
{
  PRUint32 pixelStride = aUseTransparency ? 4 : 3;
  for (PRUint32 x = 0; x < aPixelWidth; x ++) {
    const PRUint32& pixelIn = ((const PRUint32*)(aSrc))[x];
    PRUint8 *pixelOut = &aDest[x * pixelStride];

    PRUint8 alpha = (pixelIn & 0xff000000) >> 24;
    if (alpha == 0) {
      pixelOut[0] = pixelOut[1] = pixelOut[2] = pixelOut[3] = 0;
    } else {
      pixelOut[0] = (((pixelIn & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
      pixelOut[1] = (((pixelIn & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
      pixelOut[2] = (((pixelIn & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
      if (aUseTransparency)
        pixelOut[3] = alpha;
    }
  }
}


// nsPNGEncoder::StripAlpha
//
//    Input is RGBA, output is RGB

void
nsPNGEncoder::StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
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


// nsPNGEncoder::WriteCallback

void // static
nsPNGEncoder::WriteCallback(png_structp png, png_bytep data, png_size_t size)
{
  nsPNGEncoder* that = NS_STATIC_CAST(nsPNGEncoder*, png_get_io_ptr(png));
  if (! that->mImageBuffer)
    return;

  if (that->mImageBufferUsed + size > that->mImageBufferSize) {
    // expand buffer, just double each time
    that->mImageBufferSize *= 2;
    PRUint8* newBuf = (PRUint8*)PR_Realloc(that->mImageBuffer,
                                           that->mImageBufferSize);
    if (! newBuf) {
      // can't resize, just zero (this will keep us from writing more)
      PR_Free(that->mImageBuffer);
      that->mImageBufferSize = 0;
      that->mImageBufferUsed = 0;
      return;
    }
    that->mImageBuffer = newBuf;
  }
  memcpy(&that->mImageBuffer[that->mImageBufferUsed], data, size);
  that->mImageBufferUsed += size;
}
