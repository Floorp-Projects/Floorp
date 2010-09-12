/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *   Bobby Holley <bobbyholley@gmail.com>
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
/*
The Graphics Interchange Format(c) is the copyright property of CompuServe
Incorporated. Only CompuServe Incorporated is authorized to define, redefine,
enhance, alter, modify or change in any way the definition of the format.

CompuServe Incorporated hereby grants a limited, non-exclusive, royalty-free
license for the use of the Graphics Interchange Format(sm) in computer
software; computer software utilizing GIF(sm) must acknowledge ownership of the
Graphics Interchange Format and its Service Mark by CompuServe Incorporated, in
User and Technical Documentation. Computer software utilizing GIF, which is
distributed or may be distributed without User or Technical Documentation must
display to the screen or printer a message acknowledging ownership of the
Graphics Interchange Format and the Service Mark by CompuServe Incorporated; in
this case, the acknowledgement may be displayed in an opening screen or leading
banner, or a closing screen or trailing banner. A message such as the following
may be used:

    "The Graphics Interchange Format(c) is the Copyright property of
    CompuServe Incorporated. GIF(sm) is a Service Mark property of
    CompuServe Incorporated."

For further information, please contact :

    CompuServe Incorporated
    Graphics Technology Department
    5000 Arlington Center Boulevard
    Columbus, Ohio  43220
    U. S. A.

CompuServe Incorporated maintains a mailing list with all those individuals and
organizations who wish to receive copies of this document when it is corrected
or revised. This service is offered free of charge; please provide us with your
mailing address.
*/

#include <stddef.h>
#include "prmem.h"

#include "nsGIFDecoder2.h"
#include "nsIInputStream.h"
#include "imgIContainerObserver.h"
#include "RasterImage.h"

#include "gfxColor.h"
#include "gfxPlatform.h"
#include "qcms.h"

namespace mozilla {
namespace imagelib {

/*
 * GETN(n, s) requests at least 'n' bytes available from 'q', at start of state 's'
 *
 * Note, the hold will never need to be bigger than 256 bytes to gather up in the hold,
 * as each GIF block (except colormaps) can never be bigger than 256 bytes.
 * Colormaps are directly copied in the resp. global_colormap or the local_colormap of the PAL image frame
 * So a fixed buffer in gif_struct is good enough.
 * This buffer is only needed to copy left-over data from one GifWrite call to the next
 */
#define GETN(n,s)                      \
  PR_BEGIN_MACRO                       \
    mGIFStruct.bytes_to_consume = (n); \
    mGIFStruct.state = (s);            \
  PR_END_MACRO

/* Get a 16-bit value stored in little-endian format */
#define GETINT16(p)   ((p)[1]<<8|(p)[0])
//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation

nsGIFDecoder2::nsGIFDecoder2()
  : mCurrentRow(-1)
  , mLastFlushedRow(-1)
  , mImageData(nsnull)
  , mOldColor(0)
  , mCurrentFrame(-1)
  , mCurrentPass(0)
  , mLastFlushedPass(0)
  , mGIFOpen(PR_FALSE)
  , mSawTransparency(PR_FALSE)
  , mEnded(PR_FALSE)
{
  // Clear out the structure, excluding the arrays
  memset(&mGIFStruct, 0, sizeof(mGIFStruct));
}

nsGIFDecoder2::~nsGIFDecoder2()
{
  PR_FREEIF(mGIFStruct.local_colormap);
}

void
nsGIFDecoder2::InitInternal()
{
  // Fire OnStartDecode at init time to support bug 512435
  if (!IsSizeDecode() && mObserver)
    mObserver->OnStartDecode(nsnull);

  // Start with the version (GIF89a|GIF87a)
  mGIFStruct.state = gif_type;
  mGIFStruct.bytes_to_consume = 6;
}

void
nsGIFDecoder2::FinishInternal()
{
  // Send notifications if appropriate
  if (!IsSizeDecode() && !IsError()) {
    if (mCurrentFrame == mGIFStruct.images_decoded)
      EndImageFrame();
    EndGIF(/* aSuccess = */ PR_TRUE);
  }
}

// Push any new rows according to mCurrentPass/mLastFlushedPass and
// mCurrentRow/mLastFlushedRow.  Note: caller is responsible for
// updating mlastFlushed{Row,Pass}.
void
nsGIFDecoder2::FlushImageData(PRUint32 fromRow, PRUint32 rows)
{
  nsIntRect r(mGIFStruct.x_offset, mGIFStruct.y_offset + fromRow, mGIFStruct.width, rows);
  PostInvalidation(r);
}

void
nsGIFDecoder2::FlushImageData()
{
  switch (mCurrentPass - mLastFlushedPass) {
    case 0:  // same pass
      if (mCurrentRow - mLastFlushedRow)
        FlushImageData(mLastFlushedRow + 1, mCurrentRow - mLastFlushedRow);
      break;
  
    case 1:  // one pass on - need to handle bottom & top rects
      FlushImageData(0, mCurrentRow + 1);
      FlushImageData(mLastFlushedRow + 1, mGIFStruct.height - (mLastFlushedRow + 1));
      break;

    default:   // more than one pass on - push the whole frame
      FlushImageData(0, mGIFStruct.height);
  }
}

void
nsGIFDecoder2::WriteInternal(const char *aBuffer, PRUint32 aCount)
{
  // Don't forgive previously flagged errors
  if (IsError())
    return;

  // Push the data to the GIF decoder
  nsresult rv = GifWrite((const unsigned char *)aBuffer, aCount);

  // Flushing is only needed for first frame
  if (NS_SUCCEEDED(rv) && !mGIFStruct.images_decoded) {
    FlushImageData();
    mLastFlushedRow = mCurrentRow;
    mLastFlushedPass = mCurrentPass;
  }

  // We do some fine-grained error control here. If we have at least one frame
  // of an animated gif, we still want to display it (mostly for legacy reasons).
  // libpr0n code is strict, so we have to lie and tell it we were successful. So
  // if we have something to salvage, we send off final decode notifications, and
  // pretend that we're decoded. Otherwise, we set a data error.
  if (NS_FAILED(rv)) {

    // Determine if we want to salvage the situation.
    // If we're salvaging, send off notifications.
    // Note that we need to make sure that we have 2 frames, since that tells us
    // that the first frame is complete (the second could be in any state).
    if (mImage && mImage->GetNumFrames() > 1) {
      EndGIF(/* aSuccess = */ PR_TRUE);
    }

    // Otherwise, set an error
    else
      PostDataError();
  }
}

//******************************************************************************
// GIF decoder callback methods. Part of public API for GIF2
//******************************************************************************

//******************************************************************************
void nsGIFDecoder2::BeginGIF()
{
  if (mGIFOpen)
    return;

  mGIFOpen = PR_TRUE;

  PostSize(mGIFStruct.screen_width, mGIFStruct.screen_height);

  // If we're doing a size decode, we have what we came for
  if (IsSizeDecode())
    return;
}

//******************************************************************************
void nsGIFDecoder2::EndGIF(PRBool aSuccess)
{
  if (mEnded)
    return;

  if (aSuccess)
    mImage->DecodingComplete();

  if (mObserver) {
    mObserver->OnStopContainer(nsnull, mImage);
    mObserver->OnStopDecode(nsnull, aSuccess ? NS_OK : NS_ERROR_FAILURE,
                            nsnull);
  }

  mImage->SetLoopCount(mGIFStruct.loop_count);

  mGIFOpen = PR_FALSE;
  mEnded = PR_TRUE;
}

//******************************************************************************
nsresult nsGIFDecoder2::BeginImageFrame(gfx_depth aDepth)
{
  PRUint32 imageDataLength;
  nsresult rv;
  gfxASurface::gfxImageFormat format;
  if (mGIFStruct.is_transparent)
    format = gfxASurface::ImageFormatARGB32;
  else
    format = gfxASurface::ImageFormatRGB24;

  // Use correct format, RGB for first frame, PAL for following frames
  // and include transparency to allow for optimization of opaque images
  if (mGIFStruct.images_decoded) {
    // Image data is stored with original depth and palette
    rv = mImage->AppendPalettedFrame(mGIFStruct.x_offset, mGIFStruct.y_offset,
                                     mGIFStruct.width, mGIFStruct.height,
                                     format, aDepth, &mImageData, &imageDataLength,
                                     &mColormap, &mColormapSize);
  } else {
    // Regardless of depth of input, image is decoded into 24bit RGB
    rv = mImage->AppendFrame(mGIFStruct.x_offset, mGIFStruct.y_offset,
                             mGIFStruct.width, mGIFStruct.height,
                             format, &mImageData, &imageDataLength);
  }

  if (NS_FAILED(rv))
    return rv;

  mImage->SetFrameDisposalMethod(mGIFStruct.images_decoded,
                                 mGIFStruct.disposal_method);

  // Tell the superclass we're starting a frame
  PostFrameStart();

  if (!mGIFStruct.images_decoded) {
    // Send a onetime invalidation for the first frame if it has a y-axis offset. 
    // Otherwise, the area may never be refreshed and the placeholder will remain
    // on the screen. (Bug 37589)
    if (mGIFStruct.y_offset > 0) {
      PRInt32 imgWidth;
      mImage->GetWidth(&imgWidth);
      nsIntRect r(0, 0, imgWidth, mGIFStruct.y_offset);
      PostInvalidation(r);
    }
  }

  mCurrentFrame = mGIFStruct.images_decoded;
  return NS_OK;
}


//******************************************************************************
void nsGIFDecoder2::EndImageFrame()
{
  // First flush all pending image data 
  if (!mGIFStruct.images_decoded) {
    // Only need to flush first frame
    FlushImageData();

    // If the first frame is smaller in height than the entire image, send an
    // invalidation for the area it does not have data for.
    // This will clear the remaining bits of the placeholder. (Bug 37589)
    const PRUint32 realFrameHeight = mGIFStruct.height + mGIFStruct.y_offset;
    if (realFrameHeight < mGIFStruct.screen_height) {
      nsIntRect r(0, realFrameHeight,
                  mGIFStruct.screen_width,
                  mGIFStruct.screen_height - realFrameHeight);
      PostInvalidation(r);
    }
    // This transparency check is only valid for first frame
    if (mGIFStruct.is_transparent && !mSawTransparency) {
      mImage->SetFrameHasNoAlpha(mGIFStruct.images_decoded);
    }
  }
  mCurrentRow = mLastFlushedRow = -1;
  mCurrentPass = mLastFlushedPass = 0;

  // Only add frame if we have any rows at all
  if (mGIFStruct.rows_remaining != mGIFStruct.height) {
    if (mGIFStruct.rows_remaining && mGIFStruct.images_decoded) {
      // Clear the remaining rows (only needed for the animation frames)
      PRUint8 *rowp = mImageData + ((mGIFStruct.height - mGIFStruct.rows_remaining) * mGIFStruct.width);
      memset(rowp, 0, mGIFStruct.rows_remaining * mGIFStruct.width);
    }

    // We actually have the timeout information before we get the lzw encoded 
    // image data, at least according to the spec, but we delay in setting the 
    // timeout for the image until here to help ensure that we have the whole 
    // image frame decoded before we go off and try to display another frame.
    mImage->SetFrameTimeout(mGIFStruct.images_decoded, mGIFStruct.delay_time);
    mImage->EndFrameDecode(mGIFStruct.images_decoded);
  }

  // Unconditionally increment images_decoded, because we unconditionally
  // append frames in BeginImageFrame(). This ensures that images_decoded
  // always refers to the frame in mImage we're currently decoding,
  // even if some of them weren't decoded properly and thus are blank.
  mGIFStruct.images_decoded++;

  // Tell the superclass we finished a frame
  PostFrameStop();

  // Reset the transparent pixel
  if (mOldColor) {
    mColormap[mGIFStruct.tpixel] = mOldColor;
    mOldColor = 0;
  }

  mCurrentFrame = -1;
}


//******************************************************************************
// Send the data to the display front-end.
PRUint32 nsGIFDecoder2::OutputRow()
{
  int drow_start, drow_end;
  drow_start = drow_end = mGIFStruct.irow;

  /* Protect against too much image data */
  if ((PRUintn)drow_start >= mGIFStruct.height) {
    NS_WARNING("GIF2.cpp::OutputRow - too much image data");
    return 0;
  }

  if (!mGIFStruct.images_decoded) {
    /*
     * Haeberli-inspired hack for interlaced GIFs: Replicate lines while
     * displaying to diminish the "venetian-blind" effect as the image is
     * loaded. Adjust pixel vertical positions to avoid the appearance of the
     * image crawling up the screen as successive passes are drawn.
     */
    if (mGIFStruct.progressive_display && mGIFStruct.interlaced && (mGIFStruct.ipass < 4)) {
      /* ipass = 1,2,3 results in resp. row_dup = 7,3,1 and row_shift = 3,1,0 */
      const PRUint32 row_dup = 15 >> mGIFStruct.ipass;
      const PRUint32 row_shift = row_dup >> 1;
  
      drow_start -= row_shift;
      drow_end = drow_start + row_dup;
  
      /* Extend if bottom edge isn't covered because of the shift upward. */
      if (((mGIFStruct.height - 1) - drow_end) <= row_shift)
        drow_end = mGIFStruct.height - 1;
  
      /* Clamp first and last rows to upper and lower edge of image. */
      if (drow_start < 0)
        drow_start = 0;
      if ((PRUintn)drow_end >= mGIFStruct.height)
        drow_end = mGIFStruct.height - 1;
    }

    // Row to process
    const PRUint32 bpr = sizeof(PRUint32) * mGIFStruct.width; 
    PRUint8 *rowp = mImageData + (mGIFStruct.irow * bpr);

    // Convert color indices to Cairo pixels
    PRUint8 *from = rowp + mGIFStruct.width;
    PRUint32 *to = ((PRUint32*)rowp) + mGIFStruct.width;
    PRUint32 *cmap = mColormap;
    if (mColorMask == 0xFF) {
      for (PRUint32 c = mGIFStruct.width; c > 0; c--) {
        *--to = cmap[*--from];
      }
    } else {
      // Make sure that pixels within range of colormap.
      PRUint8 mask = mColorMask;
      for (PRUint32 c = mGIFStruct.width; c > 0; c--) {
        *--to = cmap[(*--from) & mask];
      }
    }
  
    // check for alpha (only for first frame)
    if (mGIFStruct.is_transparent && !mSawTransparency) {
      const PRUint32 *rgb = (PRUint32*)rowp;
      for (PRUint32 i = mGIFStruct.width; i > 0; i--) {
        if (*rgb++ == 0) {
          mSawTransparency = PR_TRUE;
          break;
        }
      }
    }

    // Duplicate rows
    if (drow_end > drow_start) {
      // irow is the current row filled
      for (int r = drow_start; r <= drow_end; r++) {
        if (r != int(mGIFStruct.irow)) {
          memcpy(mImageData + (r * bpr), rowp, bpr);
        }
      }
    }
  }

  mCurrentRow = drow_end;
  mCurrentPass = mGIFStruct.ipass;
  if (mGIFStruct.ipass == 1)
    mLastFlushedPass = mGIFStruct.ipass;   // interlaced starts at 1

  if (!mGIFStruct.interlaced) {
    mGIFStruct.irow++;
  } else {
    static const PRUint8 kjump[5] = { 1, 8, 8, 4, 2 };
    do {
      // Row increments resp. per 8,8,4,2 rows
      mGIFStruct.irow += kjump[mGIFStruct.ipass];
      if (mGIFStruct.irow >= mGIFStruct.height) {
        // Next pass starts resp. at row 4,2,1,0
        mGIFStruct.irow = 8 >> mGIFStruct.ipass;
        mGIFStruct.ipass++;
      }
    } while (mGIFStruct.irow >= mGIFStruct.height);
  }

  return --mGIFStruct.rows_remaining;
}

//******************************************************************************
/* Perform Lempel-Ziv-Welch decoding */
PRBool
nsGIFDecoder2::DoLzw(const PRUint8 *q)
{
  if (!mGIFStruct.rows_remaining)
    return PR_TRUE;

  /* Copy all the decoder state variables into locals so the compiler
   * won't worry about them being aliased.  The locals will be homed
   * back into the GIF decoder structure when we exit.
   */
  int avail       = mGIFStruct.avail;
  int bits        = mGIFStruct.bits;
  int codesize    = mGIFStruct.codesize;
  int codemask    = mGIFStruct.codemask;
  int count       = mGIFStruct.count;
  int oldcode     = mGIFStruct.oldcode;
  const int clear_code = ClearCode();
  PRUint8 firstchar = mGIFStruct.firstchar;
  PRInt32 datum     = mGIFStruct.datum;
  PRUint16 *prefix  = mGIFStruct.prefix;
  PRUint8 *stackp   = mGIFStruct.stackp;
  PRUint8 *suffix   = mGIFStruct.suffix;
  PRUint8 *stack    = mGIFStruct.stack;
  PRUint8 *rowp     = mGIFStruct.rowp;

  PRUint32 bpr = mGIFStruct.width;
  if (!mGIFStruct.images_decoded) 
    bpr *= sizeof(PRUint32);
  PRUint8 *rowend   = mImageData + (bpr * mGIFStruct.irow) + mGIFStruct.width;

#define OUTPUT_ROW()                                        \
  PR_BEGIN_MACRO                                            \
    if (!OutputRow())                                       \
      goto END;                                             \
    rowp = mImageData + mGIFStruct.irow * bpr;              \
    rowend = rowp + mGIFStruct.width;                       \
  PR_END_MACRO

  for (const PRUint8* ch = q; count-- > 0; ch++)
  {
    /* Feed the next byte into the decoder's 32-bit input buffer. */
    datum += ((int32) *ch) << bits;
    bits += 8;

    /* Check for underflow of decoder's 32-bit input buffer. */
    while (bits >= codesize)
    {
      /* Get the leading variable-length symbol from the data stream */
      int code = datum & codemask;
      datum >>= codesize;
      bits -= codesize;

      /* Reset the dictionary to its original state, if requested */
      if (code == clear_code) {
        codesize = mGIFStruct.datasize + 1;
        codemask = (1 << codesize) - 1;
        avail = clear_code + 2;
        oldcode = -1;
        continue;
      }

      /* Check for explicit end-of-stream code */
      if (code == (clear_code + 1)) {
        /* end-of-stream should only appear after all image data */
        return (mGIFStruct.rows_remaining == 0);
      }

      if (oldcode == -1) {
        if (code >= MAX_BITS)
          return PR_FALSE;
        *rowp++ = suffix[code];
        if (rowp == rowend)
          OUTPUT_ROW();

        firstchar = oldcode = code;
        continue;
      }

      int incode = code;
      if (code >= avail) {
        *stackp++ = firstchar;
        code = oldcode;

        if (stackp >= stack + MAX_BITS)
          return PR_FALSE;
      }

      while (code >= clear_code)
      {
        if ((code >= MAX_BITS) || (code == prefix[code]))
          return PR_FALSE;

        *stackp++ = suffix[code];
        code = prefix[code];

        if (stackp == stack + MAX_BITS)
          return PR_FALSE;
      }

      *stackp++ = firstchar = suffix[code];

      /* Define a new codeword in the dictionary. */
      if (avail < 4096) {
        prefix[avail] = oldcode;
        suffix[avail] = firstchar;
        avail++;

        /* If we've used up all the codewords of a given length
         * increase the length of codewords by one bit, but don't
         * exceed the specified maximum codeword size of 12 bits.
         */
        if (((avail & codemask) == 0) && (avail < 4096)) {
          codesize++;
          codemask += avail;
        }
      }
      oldcode = incode;

      /* Copy the decoded data out to the scanline buffer. */
      do {
        *rowp++ = *--stackp;
        if (rowp == rowend)
          OUTPUT_ROW();
      } while (stackp > stack);
    }
  }

  END:

  /* Home the local copies of the GIF decoder state variables */
  mGIFStruct.avail = avail;
  mGIFStruct.bits = bits;
  mGIFStruct.codesize = codesize;
  mGIFStruct.codemask = codemask;
  mGIFStruct.count = count;
  mGIFStruct.oldcode = oldcode;
  mGIFStruct.firstchar = firstchar;
  mGIFStruct.datum = datum;
  mGIFStruct.stackp = stackp;
  mGIFStruct.rowp = rowp;

  return PR_TRUE;
}

/** 
 * Expand the colormap from RGB to Packed ARGB as needed by Cairo.
 * And apply any LCMS transformation.
 */
static void ConvertColormap(PRUint32 *aColormap, PRUint32 aColors)
{
  // Apply CMS transformation if enabled and available
  if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
    qcms_transform *transform = gfxPlatform::GetCMSRGBTransform();
    if (transform)
      qcms_transform_data(transform, aColormap, aColormap, aColors);
  }
  // Convert from the GIF's RGB format to the Cairo format.
  // Work from end to begin, because of the in-place expansion
  PRUint8 *from = ((PRUint8 *)aColormap) + 3 * aColors;
  PRUint32 *to = aColormap + aColors;

  // Convert color entries to Cairo format

  // set up for loops below
  if (!aColors) return;
  PRUint32 c = aColors;

  // copy as bytes until source pointer is 32-bit-aligned
  // NB: can't use 32-bit reads, they might read off the end of the buffer
  for (; (NS_PTR_TO_UINT32(from) & 0x3) && c; --c) {
    from -= 3;
    *--to = GFX_PACKED_PIXEL(0xFF, from[0], from[1], from[2]);
  }

  // bulk copy of pixels.
  while (c >= 4) {
    from -= 12;
    to   -=  4;
    c    -=  4;
    GFX_BLOCK_RGB_TO_FRGB(from,to);
  }

  // copy remaining pixel(s)
  // NB: can't use 32-bit reads, they might read off the end of the buffer
  while (c--) {
    from -= 3;
    *--to = GFX_PACKED_PIXEL(0xFF, from[0], from[1], from[2]);
  }
}

/******************************************************************************/
/*
 * process data arriving from the stream for the gif decoder
 */

nsresult nsGIFDecoder2::GifWrite(const PRUint8 *buf, PRUint32 len)
{
  if (!buf || !len)
    return NS_ERROR_FAILURE;

  const PRUint8 *q = buf;

  // Add what we have sofar to the block
  // If previous call to me left something in the hold first complete current block
  // Or if we are filling the colormaps, first complete the colormap
  PRUint8* p = (mGIFStruct.state == gif_global_colormap) ? (PRUint8*)mGIFStruct.global_colormap :
               (mGIFStruct.state == gif_image_colormap) ? (PRUint8*)mColormap :
               (mGIFStruct.bytes_in_hold) ? mGIFStruct.hold : nsnull;
  if (p) {
    // Add what we have sofar to the block
    PRUint32 l = PR_MIN(len, mGIFStruct.bytes_to_consume);
    memcpy(p+mGIFStruct.bytes_in_hold, buf, l);

    if (l < mGIFStruct.bytes_to_consume) {
      // Not enough in 'buf' to complete current block, get more
      mGIFStruct.bytes_in_hold += l;
      mGIFStruct.bytes_to_consume -= l;
      return NS_OK;
    }
    // Reset hold buffer count
    mGIFStruct.bytes_in_hold = 0;
    // Point 'q' to complete block in hold (or in colormap)
    q = p;
  }

  // Invariant:
  //    'q' is start of current to be processed block (hold, colormap or buf)
  //    'bytes_to_consume' is number of bytes to consume from 'buf'
  //    'buf' points to the bytes to be consumed from the input buffer
  //    'len' is number of bytes left in input buffer from position 'buf'.
  //    At entrance of the for loop will 'buf' will be moved 'bytes_to_consume'
  //    to point to next buffer, 'len' is adjusted accordingly.
  //    So that next round in for loop, q gets pointed to the next buffer.

  for (;len >= mGIFStruct.bytes_to_consume; q=buf) {
    // Eat the current block from the buffer, q keeps pointed at current block
    buf += mGIFStruct.bytes_to_consume;
    len -= mGIFStruct.bytes_to_consume;

    switch (mGIFStruct.state)
    {
    case gif_lzw:
      if (!DoLzw(q)) {
        mGIFStruct.state = gif_error;
        break;
      }
      GETN(1, gif_sub_block);
      break;

    case gif_lzw_start:
    {
      // Make sure the transparent pixel is transparent in the colormap
      if (mGIFStruct.is_transparent) {
        // Save old value so we can restore it later
        if (mColormap == mGIFStruct.global_colormap)
            mOldColor = mColormap[mGIFStruct.tpixel];
        mColormap[mGIFStruct.tpixel] = 0;
      }

      /* Initialize LZW parser/decoder */
      mGIFStruct.datasize = *q;
      const int clear_code = ClearCode();
      if (mGIFStruct.datasize > MAX_LZW_BITS ||
          clear_code >= MAX_BITS) {
        mGIFStruct.state = gif_error;
        break;
      }

      mGIFStruct.avail = clear_code + 2;
      mGIFStruct.oldcode = -1;
      mGIFStruct.codesize = mGIFStruct.datasize + 1;
      mGIFStruct.codemask = (1 << mGIFStruct.codesize) - 1;
      mGIFStruct.datum = mGIFStruct.bits = 0;

      /* init the tables */
      for (int i = 0; i < clear_code; i++)
        mGIFStruct.suffix[i] = i;

      mGIFStruct.stackp = mGIFStruct.stack;

      GETN(1, gif_sub_block);
    }
    break;

    /* All GIF files begin with "GIF87a" or "GIF89a" */
    case gif_type:
      if (!strncmp((char*)q, "GIF89a", 6)) {
        mGIFStruct.version = 89;
      } else if (!strncmp((char*)q, "GIF87a", 6)) {
        mGIFStruct.version = 87;
      } else {
        mGIFStruct.state = gif_error;
        break;
      }
      GETN(7, gif_global_header);
      break;

    case gif_global_header:
      /* This is the height and width of the "screen" or
       * frame into which images are rendered.  The
       * individual images can be smaller than the
       * screen size and located with an origin anywhere
       * within the screen.
       */

      mGIFStruct.screen_width = GETINT16(q);
      mGIFStruct.screen_height = GETINT16(q + 2);
      mGIFStruct.global_colormap_depth = (q[4]&0x07) + 1;

      // screen_bgcolor is not used
      //mGIFStruct.screen_bgcolor = q[5];
      // q[6] = Pixel Aspect Ratio
      //   Not used
      //   float aspect = (float)((q[6] + 15) / 64.0);

      if (q[4] & 0x80) { /* global map */
        // Get the global colormap
        const PRUint32 size = (3 << mGIFStruct.global_colormap_depth);
        if (len < size) {
          // Use 'hold' pattern to get the global colormap
          GETN(size, gif_global_colormap);
          break;
        }
        // Copy everything, go to colormap state to do CMS correction
        memcpy(mGIFStruct.global_colormap, buf, size);
        buf += size;
        len -= size;
        GETN(0, gif_global_colormap);
        break;
      }

      GETN(1, gif_image_start);
      break;

    case gif_global_colormap:
      // Everything is already copied into global_colormap
      // Convert into Cairo colors including CMS transformation
      ConvertColormap(mGIFStruct.global_colormap, 1<<mGIFStruct.global_colormap_depth);
      GETN(1, gif_image_start);
      break;

    case gif_image_start:
      switch (*q) {
        case GIF_TRAILER:
          mGIFStruct.state = gif_done;
          break;

        case GIF_EXTENSION_INTRODUCER:
          GETN(2, gif_extension);
          break;

        case GIF_IMAGE_SEPARATOR:
          GETN(9, gif_image_header);
          break;

        default:
          /* If we get anything other than GIF_IMAGE_SEPARATOR, 
           * GIF_EXTENSION_INTRODUCER, or GIF_TRAILER, there is extraneous data
           * between blocks. The GIF87a spec tells us to keep reading
           * until we find an image separator, but GIF89a says such
           * a file is corrupt. We follow GIF89a and bail out. */
          if (mGIFStruct.images_decoded > 0) {
            /* The file is corrupt, but one or more images have
             * been decoded correctly. In this case, we proceed
             * as if the file were correctly terminated and set
             * the state to gif_done, so the GIF will display.
             */
            mGIFStruct.state = gif_done;
          } else {
            /* No images decoded, there is nothing to display. */
            mGIFStruct.state = gif_error;
          }
      }
      break;

    case gif_extension:
      mGIFStruct.bytes_to_consume = q[1];
      if (mGIFStruct.bytes_to_consume) {
        switch (*q) {
        case GIF_GRAPHIC_CONTROL_LABEL:
          mGIFStruct.state = gif_control_extension;
          break;
  
        case GIF_APPLICATION_EXTENSION_LABEL:
          mGIFStruct.state = gif_application_extension;
          break;
  
        case GIF_COMMENT_LABEL:
          mGIFStruct.state = gif_consume_comment;
          break;
  
        default:
          mGIFStruct.state = gif_skip_block;
        }
      } else {
        GETN(1, gif_image_start);
      }
      break;

    case gif_consume_block:
      if (!*q)
        GETN(1, gif_image_start);
      else
        GETN(*q, gif_skip_block);
      break;

    case gif_skip_block:
      GETN(1, gif_consume_block);
      break;

    case gif_control_extension:
      mGIFStruct.is_transparent = *q & 0x1;
      mGIFStruct.tpixel = q[3];
      mGIFStruct.disposal_method = ((*q) >> 2) & 0x7;
      // Some specs say 3rd bit (value 4), other specs say value 3
      // Let's choose 3 (the more popular)
      if (mGIFStruct.disposal_method == 4)
        mGIFStruct.disposal_method = 3;
      mGIFStruct.delay_time = GETINT16(q + 1) * 10;
      GETN(1, gif_consume_block);
      break;

    case gif_comment_extension:
      if (*q)
        GETN(*q, gif_consume_comment);
      else
        GETN(1, gif_image_start);
      break;

    case gif_consume_comment:
      GETN(1, gif_comment_extension);
      break;

    case gif_application_extension:
      /* Check for netscape application extension */
      if (!strncmp((char*)q, "NETSCAPE2.0", 11) ||
        !strncmp((char*)q, "ANIMEXTS1.0", 11))
        GETN(1, gif_netscape_extension_block);
      else
        GETN(1, gif_consume_block);
      break;

    /* Netscape-specific GIF extension: animation looping */
    case gif_netscape_extension_block:
      if (*q)
        GETN(*q, gif_consume_netscape_extension);
      else
        GETN(1, gif_image_start);
      break;

    /* Parse netscape-specific application extensions */
    case gif_consume_netscape_extension:
      switch (q[0] & 7) {
        case 1:
          /* Loop entire animation specified # of times.  Only read the
             loop count during the first iteration. */
          mGIFStruct.loop_count = GETINT16(q + 1);
  
          /* Zero loop count is infinite animation loop request */
          if (mGIFStruct.loop_count == 0)
            mGIFStruct.loop_count = -1;
  
          GETN(1, gif_netscape_extension_block);
          break;
        
        case 2:
          /* Wait for specified # of bytes to enter buffer */
          // Don't do this, this extension doesn't exist (isn't used at all) 
          // and doesn't do anything, as our streaming/buffering takes care of it all...
          // See: http://semmix.pl/color/exgraf/eeg24.htm
          GETN(1, gif_netscape_extension_block);
          break;
  
        default:
          // 0,3-7 are yet to be defined netscape extension codes
          mGIFStruct.state = gif_error;
      }
      break;

    case gif_image_header:
    {
      /* Get image offsets, with respect to the screen origin */
      mGIFStruct.x_offset = GETINT16(q);
      mGIFStruct.y_offset = GETINT16(q + 2);

      /* Get image width and height. */
      mGIFStruct.width  = GETINT16(q + 4);
      mGIFStruct.height = GETINT16(q + 6);

      if (!mGIFStruct.images_decoded) {
        /* Work around broken GIF files where the logical screen
         * size has weird width or height.  We assume that GIF87a
         * files don't contain animations.
         */
        if ((mGIFStruct.screen_height < mGIFStruct.height) ||
            (mGIFStruct.screen_width < mGIFStruct.width) ||
            (mGIFStruct.version == 87)) {
          mGIFStruct.screen_height = mGIFStruct.height;
          mGIFStruct.screen_width = mGIFStruct.width;
          mGIFStruct.x_offset = 0;
          mGIFStruct.y_offset = 0;
        }    
        // Create the image container with the right size.
        BeginGIF();

        // If we were doing a size decode, we're done
        if (IsSizeDecode())
          return NS_OK;
      }

      /* Work around more broken GIF files that have zero image
         width or height */
      if (!mGIFStruct.height || !mGIFStruct.width) {
        mGIFStruct.height = mGIFStruct.screen_height;
        mGIFStruct.width = mGIFStruct.screen_width;
        if (!mGIFStruct.height || !mGIFStruct.width) {
          mGIFStruct.state = gif_error;
          break;
        }
      }

      /* Depth of colors is determined by colormap */
      /* (q[8] & 0x80) indicates local colormap */
      /* bits per pixel is (q[8]&0x07 + 1) when local colormap is set */
      PRUint32 depth = mGIFStruct.global_colormap_depth;
      if (q[8] & 0x80)
        depth = (q[8]&0x07) + 1;
      PRUint32 realDepth = depth;
      while (mGIFStruct.tpixel >= (1 << realDepth) && (realDepth < 8)) {
        realDepth++;
      } 
      // Mask to limit the color values within the colormap
      mColorMask = 0xFF >> (8 - realDepth);
      nsresult rv = BeginImageFrame(realDepth);
      if (NS_FAILED(rv) || !mImageData) {
        mGIFStruct.state = gif_error;
        break;
      }

      if (q[8] & 0x40) {
        mGIFStruct.interlaced = PR_TRUE;
        mGIFStruct.ipass = 1;
      } else {
        mGIFStruct.interlaced = PR_FALSE;
        mGIFStruct.ipass = 0;
      }

      /* Only apply the Haeberli display hack on the first frame */
      mGIFStruct.progressive_display = (mGIFStruct.images_decoded == 0);

      /* Clear state from last image */
      mGIFStruct.irow = 0;
      mGIFStruct.rows_remaining = mGIFStruct.height;
      mGIFStruct.rowp = mImageData;

      /* bits per pixel is q[8]&0x07 */

      if (q[8] & 0x80) /* has a local colormap? */
      {
        mGIFStruct.local_colormap_size = 1 << depth;
        if (!mGIFStruct.images_decoded) {
          // First frame has local colormap, allocate space for it
          // as the image frame doesn't have its own palette
          mColormapSize = sizeof(PRUint32) << realDepth;
          if (!mGIFStruct.local_colormap) {
            mGIFStruct.local_colormap = (PRUint32*)PR_MALLOC(mColormapSize);
            if (!mGIFStruct.local_colormap) {
              mGIFStruct.state = gif_oom;
              break;
            }
          }
          mColormap = mGIFStruct.local_colormap;
        }
        const PRUint32 size = 3 << depth;
        if (mColormapSize > size) {
          // Clear the notfilled part of the colormap
          memset(((PRUint8*)mColormap) + size, 0, mColormapSize - size);
        }
        if (len < size) {
          // Use 'hold' pattern to get the image colormap
          GETN(size, gif_image_colormap);
          break;
        }
        // Copy everything, go to colormap state to do CMS correction
        memcpy(mColormap, buf, size);
        buf += size;
        len -= size;
        GETN(0, gif_image_colormap);
        break;
      } else {
        /* Switch back to the global palette */
        if (mGIFStruct.images_decoded) {
          // Copy global colormap into the palette of current frame
          memcpy(mColormap, mGIFStruct.global_colormap, mColormapSize);
        } else {
          mColormap = mGIFStruct.global_colormap;
        }
      }
      GETN(1, gif_lzw_start);
    }
    break;

    case gif_image_colormap:
      // Everything is already copied into local_colormap
      // Convert into Cairo colors including CMS transformation
      ConvertColormap(mColormap, mGIFStruct.local_colormap_size);
      GETN(1, gif_lzw_start);
      break;

    case gif_sub_block:
      mGIFStruct.count = *q;
      if (mGIFStruct.count) {
        /* Still working on the same image: Process next LZW data block */
        /* Make sure there are still rows left. If the GIF data */
        /* is corrupt, we may not get an explicit terminator.   */
        if (!mGIFStruct.rows_remaining) {
#ifdef DONT_TOLERATE_BROKEN_GIFS
          mGIFStruct.state = gif_error;
          break;
#else
          /* This is an illegal GIF, but we remain tolerant. */
          GETN(1, gif_sub_block);
#endif
          if (mGIFStruct.count == GIF_TRAILER) {
            /* Found a terminator anyway, so consider the image done */
            GETN(1, gif_done);
            break;
          }
        }
        GETN(mGIFStruct.count, gif_lzw);
      } else {
        /* See if there are any more images in this sequence. */
        EndImageFrame();
        GETN(1, gif_image_start);
      }
      break;

    case gif_done:
      EndGIF(/* aSuccess = */ PR_TRUE);
      return NS_OK;
      break;

    case gif_error:
      EndGIF(/* aSuccess = */ PR_FALSE);
      return NS_ERROR_FAILURE;
      break;

    // Handle out of memory errors
    case gif_oom:
      return NS_ERROR_OUT_OF_MEMORY;

    // We shouldn't ever get here.
    default:
      break;
    }
  }

  // if an error state is set but no data remains, code flow reaches here
  if (mGIFStruct.state == gif_error) {
      EndGIF(/* aSuccess = */ PR_FALSE);
      return NS_ERROR_FAILURE;
  }
  
  // Copy the leftover into mGIFStruct.hold
  mGIFStruct.bytes_in_hold = len;
  if (len) {
    // Add what we have sofar to the block
    PRUint8* p = (mGIFStruct.state == gif_global_colormap) ? (PRUint8*)mGIFStruct.global_colormap :
                 (mGIFStruct.state == gif_image_colormap) ? (PRUint8*)mColormap :
                 mGIFStruct.hold;
    memcpy(p, buf, len);
    mGIFStruct.bytes_to_consume -= len;
  }

  return NS_OK;
}

} // namespace imagelib
} // namespace mozilla
