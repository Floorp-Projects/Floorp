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

#include "nsPNGDecoder.h"

#include "nsMemory.h"
#include "nsRect.h"

#include "nsIComponentManager.h"
#include "nsIInputStream.h"

#include "imgIContainerObserver.h"

#include "nspr.h"
#include "png.h"

PR_STATIC_CALLBACK(void) info_callback(png_structp png_ptr, png_infop info_ptr);
PR_STATIC_CALLBACK(void) row_callback(png_structp png_ptr, png_bytep new_row,
                                      png_uint_32 row_num, int pass);
PR_STATIC_CALLBACK(void) end_callback(png_structp png_ptr, png_infop info_ptr);

NS_IMPL_ISUPPORTS1(nsPNGDecoder, imgIDecoder)

nsPNGDecoder::nsPNGDecoder()
{
  NS_INIT_ISUPPORTS();

  mPNG = nsnull;
  mInfo = nsnull;
  colorLine = 0;
  alphaLine = 0;
  interlacebuf = 0;
}

nsPNGDecoder::~nsPNGDecoder()
{
  if (colorLine)
    nsMemory::Free(colorLine);
  if (alphaLine)
    nsMemory::Free(alphaLine);
  if (interlacebuf)
    nsMemory::Free(interlacebuf);
}


/** imgIDecoder methods **/

/* void init (in imgILoad aLoad); */
NS_IMETHODIMP nsPNGDecoder::Init(imgILoad *aLoad)
{
  mImageLoad = aLoad;
  mObserver = do_QueryInterface(aLoad);  // we're holding 2 strong refs to the request.

  /* do png init stuff */

  /* Initialize the container's source image header. */
  /* Always decode to 24 bit pixdepth */

  mPNG = png_create_read_struct(PNG_LIBPNG_VER_STRING, 
                                NULL, NULL, 
                                NULL);
  if (!mPNG) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mInfo = png_create_info_struct(mPNG);
  if (!mInfo) {
    png_destroy_read_struct(&mPNG, NULL, NULL);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  /* use this as libpng "progressive pointer" (retrieve in callbacks) */
  png_set_progressive_read_fn(mPNG, NS_STATIC_CAST(png_voidp, this),
                              info_callback, row_callback, end_callback);

  return NS_OK;
}

/* void close (); */
NS_IMETHODIMP nsPNGDecoder::Close()
{
  if (mPNG)
    png_destroy_read_struct(&mPNG, mInfo ? &mInfo : NULL, NULL);

  return NS_OK;
}

/* void flush (); */
NS_IMETHODIMP nsPNGDecoder::Flush()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


static NS_METHOD ReadDataOut(nsIInputStream* in,
                             void* closure,
                             const char* fromRawSegment,
                             PRUint32 toOffset,
                             PRUint32 count,
                             PRUint32 *writeCount)
{
  nsPNGDecoder *decoder = NS_STATIC_CAST(nsPNGDecoder*, closure);

  // we need to do the setjmp here otherwise bad things will happen
  if (setjmp(decoder->mPNG->jmpbuf)) {
    png_destroy_read_struct(&decoder->mPNG, &decoder->mInfo, NULL);
    *writeCount = 0;
    return NS_ERROR_FAILURE;
  }

  return decoder->ProcessData((unsigned char*)fromRawSegment, count, writeCount);
}

nsresult nsPNGDecoder::ProcessData(unsigned char *data, PRUint32 count, PRUint32 *readCount)
{
  png_process_data(mPNG, mInfo, data, count);
  *readCount = count; // we always consume all the data
  return NS_OK;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsPNGDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  NS_ASSERTION(inStr, "Got a null input stream!");
  return inStr->ReadSegments(ReadDataOut, this, count, _retval);
}


void
info_callback(png_structp png_ptr, png_infop info_ptr)
{
/*  int number_passes;   NOT USED  */
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type, compression_type, filter_type;
  int channels;
  double LUT_exponent, CRT_exponent = 2.2, display_exponent, aGamma;

  png_bytep trans=NULL;
  int num_trans =0;

  /* always decode to 24-bit RGB or 32-bit RGBA  */
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_expand(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
    png_set_expand(png_ptr);
  }

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      png_set_gray_to_rgb(png_ptr);


#if defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
  // windows likes BGR
  png_set_bgr(png_ptr);
#endif

  /* set up gamma correction for Mac, Unix and (Win32 and everything else)
   * using educated guesses for display-system exponents; do preferences
   * later */

#if defined(XP_MAC) || defined(XP_MACOSX)
  LUT_exponent = 1.8 / 2.61;
#elif defined(XP_UNIX)
# if defined(__sgi)
  LUT_exponent = 1.0 / 1.7;   /* typical default for SGI console */
# elif defined(NeXT)
  LUT_exponent = 1.0 / 2.2;   /* typical default for NeXT cube */
# else
  LUT_exponent = 1.0;         /* default for most other Unix workstations */
# endif
#else
  LUT_exponent = 1.0;         /* virtually all PCs and most other systems */
#endif

  /* (alternatively, could check for SCREEN_GAMMA environment variable) */
  display_exponent = LUT_exponent * CRT_exponent;

  if (png_get_gAMA(png_ptr, info_ptr, &aGamma))
      png_set_gamma(png_ptr, display_exponent, aGamma);
  else
      png_set_gamma(png_ptr, display_exponent, 0.45455);

  /* let libpng expand interlaced images */
  if (interlace_type == PNG_INTERLACE_ADAM7) {
    /* number_passes = */
    png_set_interlace_handling(png_ptr);
  }

  /* now all of those things we set above are used to update various struct
   * members and whatnot, after which we can get channels, rowbytes, etc. */
  png_read_update_info(png_ptr, info_ptr);
  channels = png_get_channels(png_ptr, info_ptr);
  PR_ASSERT(channels == 3 || channels == 4);

  /*---------------------------------------------------------------*/
  /* copy PNG info into imagelib structs (formerly png_set_dims()) */
  /*---------------------------------------------------------------*/

  PRInt32 alpha_bits = 1;

  if (channels > 3) {
    /* check if alpha is coming from a tRNS chunk and is binary */
    if (num_trans) {
      /* if it's not a indexed color image, tRNS means binary */
      if (color_type == PNG_COLOR_TYPE_PALETTE) {
        for (int i=0; i<num_trans; i++) {
          if ((trans[i] != 0) && (trans[i] != 255)) {
            alpha_bits = 8;
            break;
          }
        }
      }
    } else {
      alpha_bits = 8;
    }
  }

  nsPNGDecoder *decoder = NS_STATIC_CAST(nsPNGDecoder*, png_get_progressive_ptr(png_ptr));

  if (decoder->mObserver)
    decoder->mObserver->OnStartDecode(nsnull, nsnull);

  decoder->mImage = do_CreateInstance("@mozilla.org/image/container;1");
  if (!decoder->mImage)
    longjmp(decoder->mPNG->jmpbuf, 5); // NS_ERROR_OUT_OF_MEMORY

  decoder->mImageLoad->SetImage(decoder->mImage);

  // since the png is only 1 frame, initalize the container to the width and height of the frame
  decoder->mImage->Init(width, height, decoder->mObserver);

  if (decoder->mObserver)
    decoder->mObserver->OnStartContainer(nsnull, nsnull, decoder->mImage);

  decoder->mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  if (!decoder->mFrame)
    longjmp(decoder->mPNG->jmpbuf, 5); // NS_ERROR_OUT_OF_MEMORY

  gfx_format format;

  if (channels == 3) {
    format = gfxIFormats::RGB;
  } else if (channels > 3) {
    if (alpha_bits == 8) {
      decoder->mImage->GetPreferredAlphaChannelFormat(&format);
    } else if (alpha_bits == 1) {
      format = gfxIFormats::RGB_A1;
    }
  }

#if defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
  // XXX this works...
  format += 1; // RGB to BGR
#endif

  // then initalize the frame and append it to the container
  decoder->mFrame->Init(0, 0, width, height, format);

  decoder->mImage->AppendFrame(decoder->mFrame);

  if (decoder->mObserver)
    decoder->mObserver->OnStartFrame(nsnull, nsnull, decoder->mFrame);

  PRUint32 bpr, abpr;
  decoder->mFrame->GetImageBytesPerRow(&bpr);
  decoder->mFrame->GetAlphaBytesPerRow(&abpr);
  decoder->colorLine = (PRUint8 *)nsMemory::Alloc(bpr);
  if (channels > 3)
    decoder->alphaLine = (PRUint8 *)nsMemory::Alloc(abpr);

  if (interlace_type == PNG_INTERLACE_ADAM7) {
    decoder->interlacebuf = (PRUint8 *)nsMemory::Alloc(channels*width*height);
    decoder->ibpr = channels*width;
    if (!decoder->interlacebuf) {
      longjmp(decoder->mPNG->jmpbuf, 5); // NS_ERROR_OUT_OF_MEMORY
    }            
  }

  return;
}





void
row_callback(png_structp png_ptr, png_bytep new_row,
             png_uint_32 row_num, int pass)
{
  /* libpng comments:
   *
   * this function is called for every row in the image.  If the
   * image is interlacing, and you turned on the interlace handler,
   * this function will be called for every row in every pass.
   * Some of these rows will not be changed from the previous pass.
   * When the row is not changed, the new_row variable will be NULL.
   * The rows and passes are called in order, so you don't really
   * need the row_num and pass, but I'm supplying them because it
   * may make your life easier.
   *
   * For the non-NULL rows of interlaced images, you must call
   * png_progressive_combine_row() passing in the row and the
   * old row.  You can call this function for NULL rows (it will
   * just return) and for non-interlaced images (it just does the
   * memcpy for you) if it will make the code easier.  Thus, you
   * can just do this for all cases:
   *
   *    png_progressive_combine_row(png_ptr, old_row, new_row);
   *
   * where old_row is what was displayed for previous rows.  Note
   * that the first pass (pass == 0 really) will completely cover
   * the old row, so the rows do not have to be initialized.  After
   * the first pass (and only for interlaced images), you will have
   * to pass the current row, and the function will combine the
   * old row and the new row.
   */
  nsPNGDecoder *decoder = NS_STATIC_CAST(nsPNGDecoder*, png_get_progressive_ptr(png_ptr));

  PRUint32 bpr, abpr;
  decoder->mFrame->GetImageBytesPerRow(&bpr);
  decoder->mFrame->GetAlphaBytesPerRow(&abpr);

  png_bytep line;
  if (decoder->interlacebuf) {
    line = decoder->interlacebuf+(row_num*decoder->ibpr);
    png_progressive_combine_row(png_ptr, line, new_row);
  }
  else
    line = new_row;

  if (new_row) {
    nscoord width;
    decoder->mFrame->GetWidth(&width);
    PRUint32 iwidth = width;

    gfx_format format;
    decoder->mFrame->GetFormat(&format);
    PRUint8 *aptr, *cptr;

    // The mac specific ifdefs in the code below are there to make sure we
    // always fill in 4 byte pixels right now, which is what the mac always
    // allocates for its pixel buffers in true color mode. This will change
    // when we start storing images with color palettes when they don't need
    // true color support (GIFs).
    switch (format) {
    case gfxIFormats::RGB:
    case gfxIFormats::BGR:
#if defined(XP_MAC) || defined(XP_MACOSX)
        cptr = decoder->colorLine;
        for (PRUint32 x=0; x<iwidth; x++) {
          *cptr++ = 0;
          *cptr++ = *line++;
          *cptr++ = *line++;
          *cptr++ = *line++;
        }
        decoder->mFrame->SetImageData(decoder->colorLine, bpr, row_num*bpr);
#else
        decoder->mFrame->SetImageData((PRUint8*)line, bpr, row_num*bpr);
#endif
      break;
    case gfxIFormats::RGB_A1:
    case gfxIFormats::BGR_A1:
      {
        cptr = decoder->colorLine;
        aptr = decoder->alphaLine;
        memset(aptr, 0, abpr);
        for (PRUint32 x=0; x<iwidth; x++) {
#if defined(XP_MAC) || defined(XP_MACOSX)
          *cptr++ = 0;
#endif
          if (line[3]) {
            *cptr++ = *line++;
            *cptr++ = *line++;
            *cptr++ = *line++;
            aptr[x>>3] |= 1<<(7-x&0x7);
            line++;
          } else {
#if defined(XP_WIN) || defined(XP_OS2)
            *cptr++ = 0;
            *cptr++ = 0;
            *cptr++ = 0;
#else
            cptr += 3;
#endif
            line += 4;
          }
        }
        decoder->mFrame->SetAlphaData(decoder->alphaLine, abpr, row_num*abpr);
        decoder->mFrame->SetImageData(decoder->colorLine, bpr, row_num*bpr);
      }
      break;
    case gfxIFormats::RGB_A8:
    case gfxIFormats::BGR_A8:
      {
        cptr = decoder->colorLine;
        aptr = decoder->alphaLine;
        for (PRUint32 x=0; x<iwidth; x++) {
#if defined(XP_MAC) || defined(XP_MACOSX)
          *cptr++ = 0;
#endif
          *cptr++ = *line++;
          *cptr++ = *line++;
          *cptr++ = *line++;
          *aptr++ = *line++;
        }
        decoder->mFrame->SetAlphaData(decoder->alphaLine, abpr, row_num*abpr);
        decoder->mFrame->SetImageData(decoder->colorLine, bpr, row_num*bpr);
      }
      break;
    case gfxIFormats::RGBA:
    case gfxIFormats::BGRA:
#if defined(XP_MAC) || defined(XP_MACOSX)
      {
        cptr = decoder->colorLine;
        aptr = decoder->alphaLine;
        for (PRUint32 x=0; x<iwidth; x++) {
          *cptr++ = 0;
          *cptr++ = *line++;
          *cptr++ = *line++;
          *cptr++ = *line++;
          *aptr++ = *line++;
        }
        decoder->mFrame->SetAlphaData(decoder->alphaLine, abpr, row_num*abpr);
        decoder->mFrame->SetImageData(decoder->colorLine, bpr, row_num*bpr);
      }
#else
      decoder->mFrame->SetImageData(line, bpr, row_num*bpr);
#endif
      break;
    }

    nsRect r(0, row_num, width, 1);
    decoder->mObserver->OnDataAvailable(nsnull, nsnull, decoder->mFrame, &r);
  }
}



void
end_callback(png_structp png_ptr, png_infop info_ptr)
{
  /* libpng comments:
   *
   * this function is called when the whole image has been read,
   * including any chunks after the image (up to and including
   * the IEND).  You will usually have the same info chunk as you
   * had in the header, although some data may have been added
   * to the comments and time fields.
   *
   * Most people won't do much here, perhaps setting a flag that
   * marks the image as finished.
   */

  nsPNGDecoder *decoder = NS_STATIC_CAST(nsPNGDecoder*, png_get_progressive_ptr(png_ptr));

  if (decoder->mObserver) {
    decoder->mObserver->OnStopFrame(nsnull, nsnull, decoder->mFrame);
    decoder->mObserver->OnStopContainer(nsnull, nsnull, decoder->mImage);
    decoder->mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
  }

  // We are never going to change the data of this frame again.  Let the OS
  // do what it wants with this image.
  decoder->mFrame->SetMutable(PR_FALSE);
}

