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
 * Copyright (C) 2002 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 */

#include "nsJPEGDecoder.h"

#include "imgIContainerObserver.h"

#include "nsIComponentManager.h"
#include "nsIInputStream.h"

#include "nspr.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsJPEGDecoder, imgIDecoder)


nsJPEGDecoder::nsJPEGDecoder()
{
  NS_INIT_ISUPPORTS();

  ZeroMemory(&mJPEG, sizeof(JPEG_CORE_PROPERTIES));
}

nsJPEGDecoder::~nsJPEGDecoder()
{
}


/** imgIDecoder methods **/

/* void init (in imgILoad aLoad); */
NS_IMETHODIMP nsJPEGDecoder::Init(imgILoad *aLoad)
{
  mImageLoad = aLoad;
  mObserver = do_QueryInterface(aLoad);

  /* Step 1: allocate and initialize JPEG decompression object */

  if (ijlInit(&mJPEG) != IJL_OK) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


/* void close (); */
NS_IMETHODIMP nsJPEGDecoder::Close()
{
  ijlFree(&mJPEG);

  return NS_OK;
}

/* void flush (); */
NS_IMETHODIMP nsJPEGDecoder::Flush()
{
  return NS_OK;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{

#if 0
  nsresult error_code = NS_ERROR_FAILURE;
  /* Return here if there is a fatal error. */
  if ((error_code = setjmp(mErr.setjmp_buffer)) != 0) {
    return error_code;
  }

  PR_LOG(gJPEGlog, PR_LOG_DEBUG,
         ("[this=%p] nsJPEGDecoder::WriteFrom -- processing JPEG data\n", this));

  switch (mState) {
  case JPEG_HEADER:
  {
    LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- entering JPEG_HEADER case");

    /* Step 3: read file parameters with jpeg_read_header() */
    if (jpeg_read_header(&mInfo, TRUE) == JPEG_SUSPENDED)
      return NS_OK; /* I/O suspension */

    /*
     * Don't allocate a giant and superfluous memory buffer
     * when the image is a sequential JPEG.
     */
    mInfo.buffered_image = jpeg_has_multiple_scans(&mInfo);

    /* Used to set up image size so arrays can be allocated */
    jpeg_calc_output_dimensions(&mInfo);

    mObserver->OnStartDecode(nsnull, nsnull);

    /* we only support jpegs with 1 or 3 components currently. */
    if (mInfo.output_components != 1 &&
        mInfo.output_components != 3) {
      mState = JPEG_ERROR;
      return NS_ERROR_UNEXPECTED;
    }

    /* Check if the request already has an image container.
       this is the case when multipart/x-mixed-replace is being downloaded
       if we already have one and it has the same width and height, reuse it.
     */
    mImageLoad->GetImage(getter_AddRefs(mImage));
    if (mImage) {
      PRInt32 width, height;
      mImage->GetWidth(&width);
      mImage->GetHeight(&height);
      if ((width != (PRInt32)mInfo.image_width) ||
          (height != (PRInt32)mInfo.image_height)) {
        mImage = nsnull;
      }
    }

    if (!mImage) {
      mImage = do_CreateInstance("@mozilla.org/image/container;1");
      if (!mImage) {
        mState = JPEG_ERROR;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mImageLoad->SetImage(mImage);
      mImage->Init(mInfo.image_width, mInfo.image_height, mObserver);
    }

    mObserver->OnStartContainer(nsnull, nsnull, mImage);

    mImage->GetFrameAt(0, getter_AddRefs(mFrame));

    PRBool createNewFrame = PR_TRUE;

    if (mFrame) {
      PRInt32 width, height;
      mFrame->GetWidth(&width);
      mFrame->GetHeight(&height);

      if ((width == (PRInt32)mInfo.image_width) &&
          (height == (PRInt32)mInfo.image_height)) {
        createNewFrame = PR_FALSE;
      } else {
        mImage->Clear();
      }
    }

    if (createNewFrame) {
      mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
      if (!mFrame) {
        mState = JPEG_ERROR;
        return NS_ERROR_OUT_OF_MEMORY;
      }

      gfx_format format = gfxIFormats::RGB;
#if defined(XP_PC) || defined(XP_BEOS)
      format = gfxIFormats::BGR;
#endif

      mFrame->Init(0, 0, mInfo.image_width, mInfo.image_height, format);
      mImage->AppendFrame(mFrame);
    }      

    mObserver->OnStartFrame(nsnull, nsnull, mFrame);

    /*
     * Make a one-row-high sample array that will go away
     * when done with image. Always make it big enough to
     * hold an RGB row.  Since this uses the IJG memory
     * manager, it must be allocated before the call to
     * jpeg_start_compress().
     */
    int row_stride;

    if (mInfo.output_components == 1)
      row_stride = mInfo.output_width;
    else
      row_stride = mInfo.output_width * 4; // use 4 instead of mInfo.output_components
                                           // so we don't have to fuss with byte alignment.
                                           // Mac wants 4 anyways.

    mSamples = (*mInfo.mem->alloc_sarray)((j_common_ptr) &mInfo,
                                           JPOOL_IMAGE,
                                           row_stride, 1);

#if defined(XP_PC) || defined(XP_BEOS) || defined(XP_MAC) || defined(XP_MACOSX) || defined(MOZ_WIDGET_PHOTON)
    // allocate buffer to do byte flipping if needed
    if (mInfo.output_components == 3) {
      mRGBPadRow = (PRUint8*) PR_MALLOC(row_stride);
      mRGBPadRowLength = row_stride;
      memset(mRGBPadRow, 0, mRGBPadRowLength);
    }
#endif

    /* Allocate RGB buffer for conversion from greyscale. */
    if (mInfo.output_components != 3) {
      row_stride = mInfo.output_width * 4;
      mSamples3 = (*mInfo.mem->alloc_sarray)((j_common_ptr) &mInfo,
                                              JPOOL_IMAGE,
                                              row_stride, 1);
    }

    mState = JPEG_START_DECOMPRESS;
  }
  case JPEG_START_DECOMPRESS:
  {
    LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- entering JPEG_START_DECOMPRESS case");
    /* Step 4: set parameters for decompression */

    /* FIXME -- Should reset dct_method and dither mode
     * for final pass of progressive JPEG
     */
    mInfo.dct_method =  JDCT_ISLOW;
    mInfo.dither_mode = JDITHER_FS;
    mInfo.do_fancy_upsampling = FALSE;
    mInfo.enable_2pass_quant = FALSE;
    mInfo.do_block_smoothing = TRUE;

    /* Step 5: Start decompressor */
    if (jpeg_start_decompress(&mInfo) == FALSE)
      return NS_OK; /* I/O suspension */

    /* If this is a progressive JPEG ... */
    if (mInfo.buffered_image) {
      mState = JPEG_DECOMPRESS_PROGRESSIVE;
    } else {
      mState = JPEG_DECOMPRESS_SEQUENTIAL;
    }

  }

  case JPEG_DECOMPRESS_SEQUENTIAL:
  {
    if (mState == JPEG_DECOMPRESS_SEQUENTIAL)
    {
      LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- JPEG_DECOMPRESS_SEQUENTIAL case");

      if (OutputScanlines(-1) == PR_FALSE)
        return NS_OK; /* I/O suspension */

      /* If we've completed image output ... */
      NS_ASSERTION(mInfo.output_scanline == mInfo.output_height, "We didn't process all of the data!");
      mState = JPEG_DONE;    
    }
  }


  case JPEG_DECOMPRESS_PROGRESSIVE:
  {
    if (mState == JPEG_DECOMPRESS_PROGRESSIVE)
    {
      LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- JPEG_DECOMPRESS_PROGRESSIVE case");

      int status;
      do {
        status = jpeg_consume_input(&mInfo);
      } while (!((status == JPEG_SUSPENDED) ||
                 (status == JPEG_REACHED_EOI)));

      switch (status) {
        case JPEG_REACHED_EOI:
          // End of image
          mState = JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT;
          break;
        case JPEG_SUSPENDED:
          PR_LOG(gJPEGlog, PR_LOG_DEBUG,
                 ("[this=%p] nsJPEGDecoder::WriteFrom -- suspending\n", this));

          return NS_OK; /* I/O suspension */
        default:
        {
#ifdef DEBUG
            printf("got someo other state!?\n");
#endif
        }
      }
    }
  }

  case JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT:
  {
    if (mState == JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT)
    {
      LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- entering JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT case");

      // XXX progressive? ;)
      // not really progressive according to the state machine... -saari
      jpeg_start_output(&mInfo, mInfo.input_scan_number);
      if (OutputScanlines(-1) == PR_FALSE)
        return NS_OK; /* I/O suspension */
      jpeg_finish_output(&mInfo);
      mState = JPEG_DONE;
    }
  }

  case JPEG_DONE:
  {
    LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- entering JPEG_DONE case");

    /* Step 7: Finish decompression */

    if (jpeg_finish_decompress(&mInfo) == FALSE)
      return NS_OK; /* I/O suspension */

    mState = JPEG_SINK_NON_JPEG_TRAILER;

    /* we're done dude */
    break;
  }
  case JPEG_SINK_NON_JPEG_TRAILER:
    PR_LOG(gJPEGlog, PR_LOG_DEBUG,
           ("[this=%p] nsJPEGDecoder::WriteFrom -- entering JPEG_SINK_NON_JPEG_TRAILER case\n", this));

    break;

  case JPEG_ERROR:
    PR_LOG(gJPEGlog, PR_LOG_DEBUG,
           ("[this=%p] nsJPEGDecoder::WriteFrom -- entering JPEG_ERROR case\n", this));

    break;
  }

#endif

  return NS_OK;
}

