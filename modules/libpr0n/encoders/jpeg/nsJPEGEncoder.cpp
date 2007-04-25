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
 * The Original Code is JPEG Encoding code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
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

#include "nsJPEGEncoder.h"
#include "prmem.h"
#include "prprf.h"
#include "nsString.h"
#include "nsStreamUtils.h"

#include <setjmp.h>
#include "jerror.h"

// Input streams that do not implement nsIAsyncInputStream should be threadsafe
// so that they may be used with nsIInputStreamPump and nsIInputStreamChannel,
// which read such a stream on a background thread.
NS_IMPL_THREADSAFE_ISUPPORTS2(nsJPEGEncoder, imgIEncoder, nsIInputStream)

// used to pass error info through the JPEG library
struct encoder_error_mgr {
  jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

nsJPEGEncoder::nsJPEGEncoder() : mImageBuffer(nsnull), mImageBufferSize(0),
                                 mImageBufferUsed(0), mImageBufferReadPoint(0)
{
}

nsJPEGEncoder::~nsJPEGEncoder()
{
  if (mImageBuffer) {
    PR_Free(mImageBuffer);
    mImageBuffer = nsnull;
  }
}


// nsJPEGEncoder::InitFromData
//
//    One output option is supported: "quality=X" where X is an integer in the
//    range 0-100. Higher values for X give better quality.
//
//    Transparency is always discarded.

NS_IMETHODIMP nsJPEGEncoder::InitFromData(const PRUint8* aData,
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
  int quality = 50;
  if (aOutputOptions.Length() > 0) {
    // have options string
    const nsString qualityPrefix(NS_LITERAL_STRING("quality="));
    if (aOutputOptions.Length() > qualityPrefix.Length()  &&
        StringBeginsWith(aOutputOptions, qualityPrefix)) {
      // have quality string
      nsCString value = NS_ConvertUTF16toUTF8(Substring(aOutputOptions,
                                                        qualityPrefix.Length()));
      int newquality = -1;
      if (PR_sscanf(PromiseFlatCString(value).get(), "%d", &newquality) == 1) {
        if (newquality >= 0 && newquality <= 100) {
          quality = newquality;
        } else {
          NS_WARNING("Quality value out of range, should be 0-100, using default");
        }
      } else {
        NS_WARNING("Quality value invalid, should be integer 0-100, using default");
      }
    }
  }

  jpeg_compress_struct cinfo;

  // We set up the normal JPEG error routines, then override error_exit.
  // This must be done before the call to create_compress
  encoder_error_mgr errmgr;
  cinfo.err = jpeg_std_error(&errmgr.pub);
  errmgr.pub.error_exit = errorExit;
  // Establish the setjmp return context for my_error_exit to use.
  if (setjmp(errmgr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error.
    // We need to clean up the JPEG object, close the input file, and return.
    return NS_ERROR_FAILURE;
  }

  jpeg_create_compress(&cinfo);
  cinfo.image_width = aWidth;
  cinfo.image_height = aHeight;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  cinfo.data_precision = 8;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, 1); // quality here is 0-100

  // set up the destination manager
  jpeg_destination_mgr destmgr;
  destmgr.init_destination = initDestination;
  destmgr.empty_output_buffer = emptyOutputBuffer;
  destmgr.term_destination = termDestination;
  cinfo.dest = &destmgr;
  cinfo.client_data = this;

  jpeg_start_compress(&cinfo, 1);

  // feed it the rows
  if (aInputFormat == INPUT_FORMAT_RGB) {
    while (cinfo.next_scanline < cinfo.image_height) {
      const PRUint8* row = &aData[cinfo.next_scanline * aStride];
      jpeg_write_scanlines(&cinfo, NS_CONST_CAST(PRUint8**, &row), 1);
    }
  } else if (aInputFormat == INPUT_FORMAT_RGBA) {
    PRUint8* row = new PRUint8[aWidth * 3];
    while (cinfo.next_scanline < cinfo.image_height) {
      StripAlpha(&aData[cinfo.next_scanline * aStride], row, aWidth);
      jpeg_write_scanlines(&cinfo, &row, 1);
    }
    delete[] row;
  } else if (aInputFormat == INPUT_FORMAT_HOSTARGB) {
    PRUint8* row = new PRUint8[aWidth * 3];
    while (cinfo.next_scanline < cinfo.image_height) {
      ConvertHostARGBRow(&aData[cinfo.next_scanline * aStride], row, aWidth);
      jpeg_write_scanlines(&cinfo, &row, 1);
    }
    delete[] row;
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  // if output callback can't get enough memory, it will free our buffer
  if (!mImageBuffer)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMETHODIMP nsJPEGEncoder::StartImageEncode(PRUint32 aWidth,
                                              PRUint32 aHeight,
                                              PRUint32 aInputFormat,
                                              const nsAString& aOutputOptions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsJPEGEncoder::AddImageFrame(const PRUint8* aData,
                                           PRUint32 aLength,
                                           PRUint32 aWidth,
                                           PRUint32 aHeight,
                                           PRUint32 aStride,
                                           PRUint32 aFrameFormat,
                                           const nsAString& aFrameOptions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsJPEGEncoder::EndImageEncode()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/* void close (); */
NS_IMETHODIMP nsJPEGEncoder::Close()
{
  if (mImageBuffer != nsnull) {
    PR_Free(mImageBuffer);
    mImageBuffer = nsnull;
    mImageBufferSize = 0;
    mImageBufferUsed = 0;
    mImageBufferReadPoint = 0;
  }
  return NS_OK;
}

/* unsigned long available (); */
NS_IMETHODIMP nsJPEGEncoder::Available(PRUint32 *_retval)
{
  if (!mImageBuffer)
    return NS_BASE_STREAM_CLOSED;

  *_retval = mImageBufferUsed - mImageBufferReadPoint;
  return NS_OK;
}

/* [noscript] unsigned long read (in charPtr aBuf, in unsigned long aCount); */
NS_IMETHODIMP nsJPEGEncoder::Read(char * aBuf, PRUint32 aCount,
                                  PRUint32 *_retval)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, _retval);
}

/* [noscript] unsigned long readSegments (in nsWriteSegmentFun aWriter, in voidPtr aClosure, in unsigned long aCount); */
NS_IMETHODIMP nsJPEGEncoder::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, PRUint32 aCount, PRUint32 *_retval)
{
  PRUint32 maxCount = mImageBufferUsed - mImageBufferReadPoint;
  if (maxCount == 0) {
    *_retval = 0;
    return NS_OK;
  }

  if (aCount > maxCount)
    aCount = maxCount;
  nsresult rv = aWriter(this, aClosure,
                        NS_REINTERPRET_CAST(const char*, mImageBuffer),
                        0, aCount, _retval);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*_retval <= aCount, "bad write count");
    mImageBufferReadPoint += *_retval;
  }

  // errors returned from the writer end here!
  return NS_OK;
}

/* boolean isNonBlocking (); */
NS_IMETHODIMP nsJPEGEncoder::IsNonBlocking(PRBool *_retval)
{
  *_retval = PR_FALSE;  // We don't implement nsIAsyncInputStream
  return NS_OK;
}


// nsJPEGEncoder::ConvertHostARGBRow
//
//    Our colors are stored with premultiplied alphas, but we need
//    an output with no alpha in machine-independent byte order.
//
//    See gfx/cairo/cairo/src/cairo-png.c

void
nsJPEGEncoder::ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                                 PRUint32 aPixelWidth)
{
  for (PRUint32 x = 0; x < aPixelWidth; x ++) {
    const PRUint32& pixelIn = ((const PRUint32*)(aSrc))[x];
    PRUint8 *pixelOut = &aDest[x * 3];

    PRUint8 alpha = (pixelIn & 0xff000000) >> 24;
    if (alpha == 0) {
      pixelOut[0] = pixelOut[1] = pixelOut[2] = 0;
    } else {
      pixelOut[0] = (((pixelIn & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
      pixelOut[1] = (((pixelIn & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
      pixelOut[2] = (((pixelIn & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
    }
  }
}


// nsJPEGEncoder::StripAlpha
//
//    Input is RGBA, output is RGB

void
nsJPEGEncoder::StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
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


// nsJPEGEncoder::initDestination
//
//    Initialize destination. This is called by jpeg_start_compress() before
//    any data is actually written. It must initialize next_output_byte and
//    free_in_buffer. free_in_buffer must be initialized to a positive value.

void // static
nsJPEGEncoder::initDestination(jpeg_compress_struct* cinfo)
{
  nsJPEGEncoder* that = NS_STATIC_CAST(nsJPEGEncoder*, cinfo->client_data);
  NS_ASSERTION(! that->mImageBuffer, "Image buffer already initialized");

  that->mImageBufferSize = 8192;
  that->mImageBuffer = (PRUint8*)PR_Malloc(that->mImageBufferSize);
  that->mImageBufferUsed = 0;

  cinfo->dest->next_output_byte = that->mImageBuffer;
  cinfo->dest->free_in_buffer = that->mImageBufferSize;
}


// nsJPEGEncoder::emptyOutputBuffer
//
//    This is called whenever the buffer has filled (free_in_buffer reaches
//    zero).  In typical applications, it should write out the *entire* buffer
//    (use the saved start address and buffer length; ignore the current state
//    of next_output_byte and free_in_buffer).  Then reset the pointer & count
//    to the start of the buffer, and return TRUE indicating that the buffer
//    has been dumped.  free_in_buffer must be set to a positive value when
//    TRUE is returned.  A FALSE return should only be used when I/O suspension
//    is desired (this operating mode is discussed in the next section).

boolean // static
nsJPEGEncoder::emptyOutputBuffer(jpeg_compress_struct* cinfo)
{
  nsJPEGEncoder* that = NS_STATIC_CAST(nsJPEGEncoder*, cinfo->client_data);
  NS_ASSERTION(that->mImageBuffer, "No buffer to empty!");

  that->mImageBufferUsed = that->mImageBufferSize;

  // expand buffer, just double size each time
  that->mImageBufferSize *= 2;

  PRUint8* newBuf = (PRUint8*)PR_Realloc(that->mImageBuffer,
                                         that->mImageBufferSize);
  if (! newBuf) {
    // can't resize, just zero (this will keep us from writing more)
    PR_Free(that->mImageBuffer);
    that->mImageBuffer = nsnull;
    that->mImageBufferSize = 0;
    that->mImageBufferUsed = 0;

    // this seems to be the only way to do errors through the JPEG library
    longjmp(((encoder_error_mgr*)(cinfo->err))->setjmp_buffer,
            NS_ERROR_OUT_OF_MEMORY);
  }
  that->mImageBuffer = newBuf;

  cinfo->dest->next_output_byte = &that->mImageBuffer[that->mImageBufferUsed];
  cinfo->dest->free_in_buffer = that->mImageBufferSize - that->mImageBufferUsed;
  return 1;
}


// nsJPEGEncoder::termDestination
//
//    Terminate destination --- called by jpeg_finish_compress() after all data
//    has been written.  In most applications, this must flush any data
//    remaining in the buffer.  Use either next_output_byte or free_in_buffer
//    to determine how much data is in the buffer.

void // static
nsJPEGEncoder::termDestination(jpeg_compress_struct* cinfo)
{
  nsJPEGEncoder* that = NS_STATIC_CAST(nsJPEGEncoder*, cinfo->client_data);
  if (! that->mImageBuffer)
    return;
  that->mImageBufferUsed = cinfo->dest->next_output_byte - that->mImageBuffer;
  NS_ASSERTION(that->mImageBufferUsed < that->mImageBufferSize,
               "JPEG library busted, got a bad image buffer size");
}


// nsJPEGEncoder::errorExit
//
//    Override the standard error method in the IJG JPEG decoder code. This
//    was mostly copied from nsJPEGDecoder.cpp

void // static
nsJPEGEncoder::errorExit(jpeg_common_struct* cinfo)
{
  nsresult error_code;
  encoder_error_mgr *err = (encoder_error_mgr *) cinfo->err;

  // Convert error to a browser error code
  switch (cinfo->err->msg_code) {
    case JERR_OUT_OF_MEMORY:
      error_code = NS_ERROR_OUT_OF_MEMORY;
    default:
      error_code = NS_ERROR_FAILURE;
  }

  // Return control to the setjmp point.
  longjmp(err->setjmp_buffer, error_code);
}
