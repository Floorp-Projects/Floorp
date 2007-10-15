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
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Federico Mena-Quintero <federico@novell.com>
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

#include "nsJPEGDecoder.h"

#include "imgIContainerObserver.h"

#include "nsIComponentManager.h"
#include "nsIInputStream.h"

#include "nspr.h"
#include "nsCRT.h"
#include "ImageLogging.h"
#include "nsIImage.h"
#include "nsIInterfaceRequestorUtils.h"
#include "gfxColor.h"

#include "jerror.h"

#include "gfxPlatform.h"

extern "C" {
#include "iccjpeg.h"
}

NS_IMPL_ISUPPORTS1(nsJPEGDecoder, imgIDecoder)

#if defined(PR_LOGGING)
PRLogModuleInfo *gJPEGlog = PR_NewLogModule("JPEGDecoder");
static PRLogModuleInfo *gJPEGDecoderAccountingLog = PR_NewLogModule("JPEGDecoderAccounting");
#else
#define gJPEGlog
#define gJPEGDecoderAccountingLog
#endif


METHODDEF(void) init_source (j_decompress_ptr jd);
METHODDEF(boolean) fill_input_buffer (j_decompress_ptr jd);
METHODDEF(void) skip_input_data (j_decompress_ptr jd, long num_bytes);
METHODDEF(void) term_source (j_decompress_ptr jd);
METHODDEF(void) my_error_exit (j_common_ptr cinfo);

/* Normal JFIF markers can't have more bytes than this. */
#define MAX_JPEG_MARKER_LENGTH  (((PRUint32)1 << 16) - 1)


nsJPEGDecoder::nsJPEGDecoder()
{
  mState = JPEG_HEADER;
  mReading = PR_TRUE;

  mBytesToSkip = 0;
  memset(&mInfo, 0, sizeof(jpeg_decompress_struct));
  memset(&mSourceMgr, 0, sizeof(mSourceMgr));
  mInfo.client_data = (void*)this;

  mBuffer = nsnull;
  mBufferLen = mBufferSize = 0;

  mBackBuffer = nsnull;
  mBackBufferLen = mBackBufferSize = mBackBufferUnreadLen = 0;

  mInProfile = nsnull;
  mTransform = nsnull;

  PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
         ("nsJPEGDecoder::nsJPEGDecoder: Creating JPEG decoder %p",
          this));
}

nsJPEGDecoder::~nsJPEGDecoder()
{
  PR_FREEIF(mBuffer);
  PR_FREEIF(mBackBuffer);
  if (mTransform)
    cmsDeleteTransform(mTransform);
  if (mInProfile)
    cmsCloseProfile(mInProfile);

  PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
         ("nsJPEGDecoder::~nsJPEGDecoder: Destroying JPEG decoder %p",
          this));
}


/** imgIDecoder methods **/

/* void init (in imgILoad aLoad); */
NS_IMETHODIMP nsJPEGDecoder::Init(imgILoad *aLoad)
{
  mImageLoad = aLoad;
  mObserver = do_QueryInterface(aLoad);

  /* We set up the normal JPEG error routines, then override error_exit. */
  mInfo.err = jpeg_std_error(&mErr.pub);
  /*   mInfo.err = jpeg_std_error(&mErr.pub); */
  mErr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(mErr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    return NS_ERROR_FAILURE;
  }

  /* Step 1: allocate and initialize JPEG decompression object */
  jpeg_create_decompress(&mInfo);
  /* Set the source manager */
  mInfo.src = &mSourceMgr;

  /* Step 2: specify data source (eg, a file) */

  /* Setup callback functions. */
  mSourceMgr.init_source = init_source;
  mSourceMgr.fill_input_buffer = fill_input_buffer;
  mSourceMgr.skip_input_data = skip_input_data;
  mSourceMgr.resync_to_restart = jpeg_resync_to_restart;
  mSourceMgr.term_source = term_source;

  /* Record app markers for ICC data */
  for (PRUint32 m = 0; m < 16; m++)
    jpeg_save_markers(&mInfo, JPEG_APP0 + m, 0xFFFF);



  /* Check if the request already has an image container.
   * this is the case when multipart/x-mixed-replace is being downloaded
   * if we already have one and it has the same width and height, reuse it.
   * This is also the case when an existing container is reloading itself from
   * us.
   *
   * If we have a mismatch in width/height for the container later on we will
   * generate an error.
   */
  mImageLoad->GetImage(getter_AddRefs(mImage));

  if (!mImage) {
      mImage = do_CreateInstance("@mozilla.org/image/container;1");
      if (!mImage)
        return NS_ERROR_OUT_OF_MEMORY;
      
      mImageLoad->SetImage(mImage);
      nsresult result = mImage->SetDiscardable("image/jpeg");
      if (NS_FAILED(result)) {
        mState = JPEG_ERROR;
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               (" (could not set image container to discardable)"));
        return result;
    }
  }

  return NS_OK;
}


/* void close (); */
NS_IMETHODIMP nsJPEGDecoder::Close()
{
  PR_LOG(gJPEGlog, PR_LOG_DEBUG,
         ("[this=%p] nsJPEGDecoder::Close\n", this));

  if (mState != JPEG_DONE && mState != JPEG_SINK_NON_JPEG_TRAILER)
    NS_WARNING("Never finished decoding the JPEG.");

  /* Step 8: Release JPEG decompression object */
  mInfo.src = nsnull;

  jpeg_destroy_decompress(&mInfo);

  return NS_OK;
}

/* void flush (); */
NS_IMETHODIMP nsJPEGDecoder::Flush()
{
  LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::Flush");

  PRUint32 ret;
  if (mState != JPEG_DONE && mState != JPEG_SINK_NON_JPEG_TRAILER && mState != JPEG_ERROR)
    return this->WriteFrom(nsnull, 0, &ret);

  return NS_OK;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  LOG_SCOPE_WITH_PARAM(gJPEGlog, "nsJPEGDecoder::WriteFrom", "count", count);

  PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
         ("nsJPEGDecoder::WriteFrom(decoder = %p) {\n"
          "        image container %s; %u bytes to be added",
          this,
          mImage ? "exists" : "does not exist",
          count));

  if (inStr) {
    if (!mBuffer) {
      mBuffer = (JOCTET *)PR_Malloc(count);
      if (!mBuffer) {
        mState = JPEG_ERROR;
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (out of memory allocating buffer)"));
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mBufferSize = count;
    } else if (count > mBufferSize) {
      JOCTET *buf = (JOCTET *)PR_Realloc(mBuffer, count);
      if (!buf) {
        mState = JPEG_ERROR;
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (out of memory resizing buffer)"));
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mBuffer = buf;
      mBufferSize = count;
    }

    nsresult rv = inStr->Read((char*)mBuffer, count, &mBufferLen);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsJPEGDecoder::WriteFrom -- inStr->Read failed");

    PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
           ("nsJPEGDecoder::WriteFrom(): decoder %p got %u bytes, read %u from the stream (buffer size %u)",
            this,
            count,
            mBufferLen,
            mBufferSize));
    
    *_retval = mBufferLen;

    nsresult result = mImage->AddRestoreData((char *) mBuffer, count);

    if (NS_FAILED(result)) {
      mState = JPEG_ERROR;
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (could not add restore data)"));
      return result;
    }

    PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
           ("        added %u bytes to restore data",
            count));
  }
  // else no input stream.. Flush() ?

  /* Return here if there is a fatal error. */
  nsresult error_code;
  if ((error_code = setjmp(mErr.setjmp_buffer)) != 0) {
    mState = JPEG_SINK_NON_JPEG_TRAILER;
    if (error_code == NS_ERROR_FAILURE) {
      /* Error due to corrupt stream - return NS_OK so that libpr0n
         doesn't throw away a partial image load */
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (setjmp returned NS_ERROR_FAILURE)"));
      return NS_OK;
    } else {
      /* Error due to reasons external to the stream (probably out of
         memory) - let libpr0n attempt to clean up, even though
         mozilla is seconds away from falling flat on its face. */
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (setjmp returned an error)"));
      return error_code;
    }
  }

  PR_LOG(gJPEGlog, PR_LOG_DEBUG,
         ("[this=%p] nsJPEGDecoder::WriteFrom -- processing JPEG data\n", this));

  switch (mState) {
  case JPEG_HEADER:
  {
    LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- entering JPEG_HEADER case");

    /* Step 3: read file parameters with jpeg_read_header() */
    if (jpeg_read_header(&mInfo, TRUE) == JPEG_SUSPENDED) {
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (JPEG_SUSPENDED)"));
      return NS_OK; /* I/O suspension */
    }

    JOCTET  *profile;
    PRUint32 profileLength;

    if (gfxPlatform::IsCMSEnabled() &&
        read_icc_profile(&mInfo, &profile, &profileLength) &&
        (mInProfile = cmsOpenProfileFromMem(profile, profileLength)) != NULL) {
      free(profile);

      PRUint32 profileSpace = cmsGetColorSpace(mInProfile);
      PRBool mismatch = PR_FALSE;

#ifdef DEBUG_tor
      fprintf(stderr, "JPEG profileSpace: 0x%08X\n", profileSpace);
#endif
      switch (mInfo.jpeg_color_space) {
      case JCS_GRAYSCALE:
        if (profileSpace == icSigRgbData)
          mInfo.out_color_space = JCS_RGB;
        else if (profileSpace != icSigGrayData)
          mismatch = PR_TRUE;
        break;
      case JCS_RGB:
        if (profileSpace != icSigRgbData)
          mismatch =  PR_TRUE;
        break;
      case JCS_YCbCr:
        if (profileSpace == icSigRgbData)
          mInfo.out_color_space = JCS_RGB;
        else if (profileSpace != icSigYCbCrData)
          mismatch = PR_TRUE;
        break;
      case JCS_CMYK:
      case JCS_YCCK:
        if (profileSpace == icSigCmykData)
          mInfo.out_color_space = JCS_CMYK;
        else
          mismatch = PR_TRUE;
        break;
      default:
        mState = JPEG_ERROR;
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (unknown colorpsace (1))"));
        return NS_ERROR_UNEXPECTED;
      }

      if (!mismatch) {
        PRUint32 space, channels;
        switch (mInfo.out_color_space) {
        case JCS_GRAYSCALE:
          space = PT_GRAY;
          channels = 1;
          break;
        case JCS_RGB:
          space = PT_RGB;
          channels = 3;
          break;
        case JCS_YCbCr:
          space = PT_YCbCr;
          channels = 3;
        case JCS_CMYK:
          space = PT_CMYK;
          channels = 4;
          break;
        default:
          mState = JPEG_ERROR;
          PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
                 ("} (unknown colorpsace (2))"));
          return NS_ERROR_UNEXPECTED;
        }

        PRUint32 type =
          COLORSPACE_SH(space) |
          CHANNELS_SH(channels) |
          BYTES_SH(1);

        /* Adobe Photoshop writes YCCK/CMYK files with inverted data */
        if (mInfo.out_color_space == JCS_CMYK)
          type |= FLAVOR_SH(mInfo.saw_Adobe_marker ? 1 : 0);

        if (gfxPlatform::GetCMSOutputProfile())
          mTransform = cmsCreateTransform(mInProfile,
                                          type,
                                          gfxPlatform::GetCMSOutputProfile(),
                                          TYPE_RGB_8,
                                          cmsTakeRenderingIntent(mInProfile),
                                          0);
      } else {
#ifdef DEBUG_tor
        fprintf(stderr, "ICM profile colorspace mismatch\n");
#endif
      }
    }

    if (!mTransform) {
      switch (mInfo.jpeg_color_space) {
      case JCS_GRAYSCALE:
      case JCS_RGB:
      case JCS_YCbCr:
        mInfo.out_color_space = JCS_RGB;
        break;
      default:
        mState = JPEG_ERROR;
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (unknown colorpsace (3))"));
        return NS_ERROR_UNEXPECTED;
        break;
      }
    }

    /*
     * Don't allocate a giant and superfluous memory buffer
     * when the image is a sequential JPEG.
     */
    mInfo.buffered_image = jpeg_has_multiple_scans(&mInfo);

    /* Used to set up image size so arrays can be allocated */
    jpeg_calc_output_dimensions(&mInfo);

    mObserver->OnStartDecode(nsnull);

    /* verify that the width and height of the image are the same as
     * the container we're about to put things in to.
     * XXX it might not matter maybe we should just resize the image.
     */
    PRInt32 width, height;
    mImage->GetWidth(&width);
    mImage->GetHeight(&height);
    if (width == 0 && height == 0) {
      mImage->Init(mInfo.image_width, mInfo.image_height, mObserver);
    } else if ((width != (PRInt32)mInfo.image_width) || (height != (PRInt32)mInfo.image_height)) {
      mState = JPEG_ERROR;
      return NS_ERROR_UNEXPECTED;
    }

    mImage->Init(mInfo.image_width, mInfo.image_height, mObserver);

    mObserver->OnStartContainer(nsnull, mImage);

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
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (could not create image frame)"));
        return NS_ERROR_OUT_OF_MEMORY;
      }

      gfx_format format = gfxIFormats::RGB;
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)
      format = gfxIFormats::BGR;
#endif

      if (NS_FAILED(mFrame->Init(0, 0, mInfo.image_width, mInfo.image_height, format, 24))) {
        mState = JPEG_ERROR;
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (could not initialize image frame)"));
        return NS_ERROR_OUT_OF_MEMORY;
      }

      mImage->AppendFrame(mFrame);

      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("        JPEGDecoderAccounting: nsJPEGDecoder::WriteFrom -- created image frame with %ux%u pixels",
              mInfo.image_width, mInfo.image_height));
    }

    mObserver->OnStartFrame(nsnull, mFrame);
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
    mInfo.do_fancy_upsampling = TRUE;
    mInfo.enable_2pass_quant = FALSE;
    mInfo.do_block_smoothing = TRUE;

    /* Step 5: Start decompressor */
    if (jpeg_start_decompress(&mInfo) == FALSE) {
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (I/O suspension after jpeg_start_decompress())"));
      return NS_OK; /* I/O suspension */
    }

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
      
      if (!OutputScanlines()) {
        PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
               ("} (I/O suspension after OutputScanlines() - SEQUENTIAL)"));
        return NS_OK; /* I/O suspension */
      }
      
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
      } while ((status != JPEG_SUSPENDED) &&
               (status != JPEG_REACHED_EOI));

      for (;;) {
        if (mInfo.output_scanline == 0) {
          int scan = mInfo.input_scan_number;

          /* if we haven't displayed anything yet (output_scan_number==0)
             and we have enough data for a complete scan, force output
             of the last full scan */
          if ((mInfo.output_scan_number == 0) &&
              (scan > 1) &&
              (status != JPEG_REACHED_EOI))
            scan--;

          if (!jpeg_start_output(&mInfo, scan)) {
            PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
                   ("} (I/O suspension after jpeg_start_output() - PROGRESSIVE)"));
            return NS_OK; /* I/O suspension */
          }
        }

        if (mInfo.output_scanline == 0xffffff)
          mInfo.output_scanline = 0;

        if (!OutputScanlines()) {
          if (mInfo.output_scanline == 0) {
            /* didn't manage to read any lines - flag so we don't call
               jpeg_start_output() multiple times for the same scan */
            mInfo.output_scanline = 0xffffff;
          }
          PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
                 ("} (I/O suspension after OutputScanlines() - PROGRESSIVE)"));
          return NS_OK; /* I/O suspension */
        }

        if (mInfo.output_scanline == mInfo.output_height)
        {
          if (!jpeg_finish_output(&mInfo)) {
            PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
                   ("} (I/O suspension after jpeg_finish_output() - PROGRESSIVE)"));
            return NS_OK; /* I/O suspension */
          }

          if (jpeg_input_complete(&mInfo) &&
              (mInfo.input_scan_number == mInfo.output_scan_number))
            break;

          mInfo.output_scanline = 0;
        }
      }

      mState = JPEG_DONE;
    }
  }

  case JPEG_DONE:
  {
    nsresult result;

    LOG_SCOPE(gJPEGlog, "nsJPEGDecoder::WriteFrom -- entering JPEG_DONE case");

    /* Step 7: Finish decompression */

    if (jpeg_finish_decompress(&mInfo) == FALSE) {
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (I/O suspension after jpeg_finish_decompress() - DONE)"));
      return NS_OK; /* I/O suspension */
    }

    mState = JPEG_SINK_NON_JPEG_TRAILER;

    result = mImage->RestoreDataDone();
    if (NS_FAILED (result)) {
      mState = JPEG_ERROR;
      PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
             ("} (could not mark image container with RestoreDataDone)"));
      return result;
    }

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

  PR_LOG(gJPEGDecoderAccountingLog, PR_LOG_DEBUG,
         ("} (end of function)"));
  return NS_OK;
}


PRBool
nsJPEGDecoder::OutputScanlines()
{
  const PRUint32 top = mInfo.output_scanline;
  PRBool rv = PR_TRUE;

  mFrame->LockImageData();
  
  // we're thebes. we can write stuff directly to the data
  PRUint8 *imageData;
  PRUint32 imageDataLength;
  mFrame->GetImageData(&imageData, &imageDataLength);

  while ((mInfo.output_scanline < mInfo.output_height)) {
      /* Request one scanline.  Returns 0 or 1 scanlines. */    
      PRUint32 *imageRow = ((PRUint32*)imageData) +
                           (mInfo.output_scanline * mInfo.output_width);

      /* Use the Cairo image buffer as scanline buffer */
      JSAMPROW sampleRow = (JSAMPROW)imageRow;
      if (!mTransform || (mInfo.out_color_space != JCS_GRAYSCALE &&
                          mInfo.out_color_space != JCS_CMYK)) {
        /* Put the pixels at end of row to enable in-place expansion */
        sampleRow += mInfo.output_width;
      }

      if (jpeg_read_scanlines(&mInfo, &sampleRow, 1) != 1) {
        rv = PR_FALSE; /* suspend */
        break;
      }

      if (mTransform) {
        JSAMPROW source = sampleRow;
        if (mInfo.out_color_space == JCS_GRAYSCALE) {
          /* Convert from the 1byte grey pixels at begin of row 
             to the 3byte RGB byte pixels at 'end' of row */
          sampleRow += mInfo.output_width;
        }
        cmsDoTransform(mTransform, source, sampleRow, mInfo.output_width);
        /* Move 3byte RGB data to end of row */
        if (mInfo.out_color_space == JCS_CMYK) {
          memmove(sampleRow + mInfo.output_width,
                  sampleRow,
                  3 * mInfo.output_width);
          sampleRow += mInfo.output_width;
        }
      } else if (gfxPlatform::IsCMSEnabled()) {
        /* No embedded ICC profile - treat as sRGB */
        cmsHTRANSFORM transform = gfxPlatform::GetCMSRGBTransform();
        if (transform) {
          cmsDoTransform(transform, sampleRow, sampleRow, mInfo.output_width);
        }
      }

      for (PRUint32 i=mInfo.output_width; i>0; --i) {
        *imageRow++ = GFX_PACKED_PIXEL(0xFF, sampleRow[0], sampleRow[1], sampleRow[2]);
        sampleRow += 3;
      }
  }

  if (top != mInfo.output_scanline) {
      nsIntRect r(0, top, mInfo.output_width, mInfo.output_scanline-top);
      nsCOMPtr<nsIImage> img(do_GetInterface(mFrame));
      img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
      mObserver->OnDataAvailable(nsnull, mFrame, &r);
  }
  
  mFrame->UnlockImageData();
  
  return rv;
}


/* Override the standard error method in the IJG JPEG decoder code. */
METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  decoder_error_mgr *err = (decoder_error_mgr *) cinfo->err;

  /* Convert error to a browser error code */
  nsresult error_code = err->pub.msg_code == JERR_OUT_OF_MEMORY
                      ? NS_ERROR_OUT_OF_MEMORY
                      : NS_ERROR_FAILURE;

#ifdef DEBUG
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*err->pub.format_message) (cinfo, buffer);

  fprintf(stderr, "JPEG decoding error:\n%s\n", buffer);
#endif

  /* Return control to the setjmp point. */
  longjmp(err->setjmp_buffer, error_code);
}

/******************************************************************************/
/*-----------------------------------------------------------------------------
 * This is the callback routine from the IJG JPEG library used to supply new
 * data to the decompressor when its input buffer is exhausted.  It juggles
 * multiple buffers in an attempt to avoid unnecessary copying of input data.
 *
 * (A simpler scheme is possible: It's much easier to use only a single
 * buffer; when fill_input_buffer() is called, move any unconsumed data
 * (beyond the current pointer/count) down to the beginning of this buffer and
 * then load new data into the remaining buffer space.  This approach requires
 * a little more data copying but is far easier to get right.)
 *
 * At any one time, the JPEG decompressor is either reading from the necko
 * input buffer, which is volatile across top-level calls to the IJG library,
 * or the "backtrack" buffer.  The backtrack buffer contains the remaining
 * unconsumed data from the necko buffer after parsing was suspended due
 * to insufficient data in some previous call to the IJG library.
 *
 * When suspending, the decompressor will back up to a convenient restart
 * point (typically the start of the current MCU). The variables
 * next_input_byte & bytes_in_buffer indicate where the restart point will be
 * if the current call returns FALSE.  Data beyond this point must be
 * rescanned after resumption, so it must be preserved in case the decompressor
 * decides to backtrack.
 *
 * Returns:
 *  TRUE if additional data is available, FALSE if no data present and
 *   the JPEG library should therefore suspend processing of input stream
 *---------------------------------------------------------------------------*/

/******************************************************************************/
/* data source manager method                                                 */
/******************************************************************************/


/******************************************************************************/
/* data source manager method 
        Initialize source.  This is called by jpeg_read_header() before any
        data is actually read.  May leave
        bytes_in_buffer set to 0 (in which case a fill_input_buffer() call
        will occur immediately).
*/
METHODDEF(void)
init_source (j_decompress_ptr jd)
{
}

/******************************************************************************/
/* data source manager method
        Skip num_bytes worth of data.  The buffer pointer and count should
        be advanced over num_bytes input bytes, refilling the buffer as
        needed.  This is used to skip over a potentially large amount of
        uninteresting data (such as an APPn marker).  In some applications
        it may be possible to optimize away the reading of the skipped data,
        but it's not clear that being smart is worth much trouble; large
        skips are uncommon.  bytes_in_buffer may be zero on return.
        A zero or negative skip count should be treated as a no-op.
*/
METHODDEF(void)
skip_input_data (j_decompress_ptr jd, long num_bytes)
{
  struct jpeg_source_mgr *src = jd->src;
  nsJPEGDecoder *decoder = (nsJPEGDecoder *)(jd->client_data);

  if (num_bytes > (long)src->bytes_in_buffer) {
    /*
     * Can't skip it all right now until we get more data from
     * network stream. Set things up so that fill_input_buffer
     * will skip remaining amount.
     */
    decoder->mBytesToSkip = (size_t)num_bytes - src->bytes_in_buffer;
    src->next_input_byte += src->bytes_in_buffer;
    src->bytes_in_buffer = 0;

  } else {
    /* Simple case. Just advance buffer pointer */

    src->bytes_in_buffer -= (size_t)num_bytes;
    src->next_input_byte += num_bytes;
  }
}


/******************************************************************************/
/* data source manager method
        This is called whenever bytes_in_buffer has reached zero and more
        data is wanted.  In typical applications, it should read fresh data
        into the buffer (ignoring the current state of next_input_byte and
        bytes_in_buffer), reset the pointer & count to the start of the
        buffer, and return TRUE indicating that the buffer has been reloaded.
        It is not necessary to fill the buffer entirely, only to obtain at
        least one more byte.  bytes_in_buffer MUST be set to a positive value
        if TRUE is returned.  A FALSE return should only be used when I/O
        suspension is desired.
*/
METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr jd)
{
  struct jpeg_source_mgr *src = jd->src;
  nsJPEGDecoder *decoder = (nsJPEGDecoder *)(jd->client_data);

  if (decoder->mReading) {
    unsigned char *new_buffer = (unsigned char *)decoder->mBuffer;
    PRUint32 new_buflen = decoder->mBufferLen;
  
    if (!new_buffer || new_buflen == 0)
      return PR_FALSE; /* suspend */

    decoder->mBufferLen = 0;

    if (decoder->mBytesToSkip) {
      if (decoder->mBytesToSkip < new_buflen) {
        /* All done skipping bytes; Return what's left. */
        new_buffer += decoder->mBytesToSkip;
        new_buflen -= decoder->mBytesToSkip;
        decoder->mBytesToSkip = 0;
      } else {
        /* Still need to skip some more data in the future */
        decoder->mBytesToSkip -= (size_t)new_buflen;
        return PR_FALSE; /* suspend */
      }
    }

      decoder->mBackBufferUnreadLen = src->bytes_in_buffer;

    src->next_input_byte = new_buffer;
    src->bytes_in_buffer = (size_t)new_buflen;
    decoder->mReading = PR_FALSE;

    return PR_TRUE;
  }

  if (src->next_input_byte != decoder->mBuffer) {
    /* Backtrack data has been permanently consumed. */
    decoder->mBackBufferUnreadLen = 0;
    decoder->mBackBufferLen = 0;
  }

  /* Save remainder of netlib buffer in backtrack buffer */
  const PRUint32 new_backtrack_buflen = src->bytes_in_buffer + decoder->mBackBufferLen;
 
  /* Make sure backtrack buffer is big enough to hold new data. */
  if (decoder->mBackBufferSize < new_backtrack_buflen) {


    /* Round up to multiple of 256 bytes. */
    const PRUint32 roundup_buflen = ((new_backtrack_buflen + 255) >> 8) << 8;

    if (decoder->mBackBuffer) {
      JOCTET *buf = (JOCTET *)PR_REALLOC(decoder->mBackBuffer, roundup_buflen);
      /* Check for OOM */
      if (!buf) {
        decoder->mInfo.err->msg_code = JERR_OUT_OF_MEMORY;
        my_error_exit((j_common_ptr)(&decoder->mInfo));
      }
      decoder->mBackBuffer = buf;
    } else {
      decoder->mBackBuffer = (JOCTET*)PR_MALLOC(roundup_buflen);
      /* Check for OOM */
      if (!decoder->mBackBuffer) {
        decoder->mInfo.err->msg_code = JERR_OUT_OF_MEMORY;
        my_error_exit((j_common_ptr)(&decoder->mInfo));
      }
    }
      
    decoder->mBackBufferSize = (size_t)roundup_buflen;

    /* Check for malformed MARKER segment lengths. */
    if (new_backtrack_buflen > MAX_JPEG_MARKER_LENGTH) {
      my_error_exit((j_common_ptr)(&decoder->mInfo));
    }
  }

  /* Copy remainder of netlib buffer into backtrack buffer. */
  memmove(decoder->mBackBuffer + decoder->mBackBufferLen,
          src->next_input_byte,
          src->bytes_in_buffer);

  /* Point to start of data to be rescanned. */
  src->next_input_byte = decoder->mBackBuffer + decoder->mBackBufferLen - decoder->mBackBufferUnreadLen;
  src->bytes_in_buffer += decoder->mBackBufferUnreadLen;
  decoder->mBackBufferLen = (size_t)new_backtrack_buflen;
  decoder->mReading = PR_TRUE;

  return PR_FALSE;
}

/******************************************************************************/
/* data source manager method */
/*
 * Terminate source --- called by jpeg_finish_decompress() after all
 * data has been read to clean up JPEG source manager. NOT called by 
 * jpeg_abort() or jpeg_destroy().
 */
METHODDEF(void)
term_source (j_decompress_ptr jd)
{
  nsJPEGDecoder *decoder = (nsJPEGDecoder *)(jd->client_data);

  if (decoder->mObserver) {
    decoder->mObserver->OnStopFrame(nsnull, decoder->mFrame);
    decoder->mObserver->OnStopContainer(nsnull, decoder->mImage);
    decoder->mObserver->OnStopDecode(nsnull, NS_OK, nsnull);
  }

  PRBool isMutable = PR_FALSE;
  if (decoder->mImageLoad) 
      decoder->mImageLoad->GetIsMultiPartChannel(&isMutable);
  decoder->mFrame->SetMutable(isMutable);
}
