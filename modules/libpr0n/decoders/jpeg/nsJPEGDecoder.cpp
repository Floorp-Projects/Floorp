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

#include "nsJPEGDecoder.h"

#include "nsIInputStream.h"

#include "nspr.h"

#include "nsCRT.h"

#include "nsIComponentManager.h"

#include "imgIContainerObserver.h"


#include "ImageLogging.h"


NS_IMPL_ISUPPORTS2(nsJPEGDecoder, imgIDecoder, nsIOutputStream)


#if defined(PR_LOGGING)
PRLogModuleInfo *gJPEGlog = PR_NewLogModule("JPEGDecoder");
#else
#define gJPEGlog
#endif



static void PR_CALLBACK init_source (j_decompress_ptr jd);
static boolean PR_CALLBACK fill_input_buffer (j_decompress_ptr jd);
static void PR_CALLBACK skip_input_data (j_decompress_ptr jd, long num_bytes);
static void PR_CALLBACK term_source (j_decompress_ptr jd);
void PR_CALLBACK my_error_exit (j_common_ptr cinfo);

/* Normal JFIF markers can't have more bytes than this. */
#define MAX_JPEG_MARKER_LENGTH  (((PRUint32)1 << 16) - 1)


/* Possible states for JPEG source manager */
enum data_source_state {
    READING_BACK = 0, /* Must be zero for init purposes */
    READING_NEW
};

/*
 *  Implementation of a JPEG src object that understands our state machine
 */
typedef struct {
  /* public fields; must be first in this struct! */
  struct jpeg_source_mgr pub;

  nsJPEGDecoder *decoder;

} decoder_source_mgr;


nsJPEGDecoder::nsJPEGDecoder()
{
  NS_INIT_ISUPPORTS();

  mState = JPEG_HEADER;
  mFillState = READING_BACK;

  mSamples = nsnull;
  mSamples3 = nsnull;
  mRGBPadRow = nsnull;
  mRGBPadRowLength = 0;

  mBytesToSkip = 0;
  
  memset(&mInfo, 0, sizeof(jpeg_decompress_struct));

  mCompletedPasses = 0;

  mBuffer = nsnull;
  mBufferLen = mBufferSize = 0;

  mBackBuffer = nsnull;
  mBackBufferLen = mBackBufferSize = mBackBufferUnreadLen = 0;

}

nsJPEGDecoder::~nsJPEGDecoder()
{
  if (mBuffer)
    PR_Free(mBuffer);
  if (mBackBuffer)
    PR_Free(mBackBuffer);
  if (mRGBPadRow)
    PR_Free(mRGBPadRow);
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
  
  decoder_source_mgr *src;
  if (mInfo.src == NULL) {
    src = PR_NEWZAP(decoder_source_mgr);
    if (!src) {
      mState = JPEG_ERROR;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mInfo.src = NS_REINTERPRET_CAST(struct jpeg_source_mgr *, src);
  }

  /* Step 2: specify data source (eg, a file) */

  /* Setup callback functions. */
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;
  src->pub.term_source = term_source;

  src->decoder = this;

  return NS_OK;
}




/** nsIOutputStream methods **/

/* void close (); */
NS_IMETHODIMP nsJPEGDecoder::Close()
{
  PR_LOG(gJPEGlog, PR_LOG_DEBUG,
         ("[this=%p] nsJPEGDecoder::Close\n", this));

  if (mState != JPEG_DONE && mState != JPEG_SINK_NON_JPEG_TRAILER)
    NS_WARNING("Never finished decoding the JPEG.");

  /* Step 8: Release JPEG decompression object */
  decoder_source_mgr *src = NS_REINTERPRET_CAST(decoder_source_mgr *, mInfo.src);
  PR_FREEIF(src);
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

/* unsigned long write (in string buf, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::Write(const char *buf, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  LOG_SCOPE_WITH_PARAM(gJPEGlog, "nsJPEGDecoder::WriteFrom", "count", count);

  if (inStr) {
    if (!mBuffer) {
      mBuffer = (JOCTET *)PR_Malloc(count);
      mBufferSize = count;
    } else if (count > mBufferSize) {
      mBuffer = (JOCTET *)PR_Realloc(mBuffer, count);
      mBufferSize = count;
    }

    nsresult rv = inStr->Read((char*)mBuffer, count, &mBufferLen);
    *_retval = mBufferLen;

    //nsresult rv = mOutStream->WriteFrom(inStr, count, _retval);

    NS_ASSERTION(NS_SUCCEEDED(rv), "nsJPEGDecoder::WriteFrom -- mOutStream->WriteFrom failed");
  }
  // else no input stream.. Flush() ?


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
#if defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
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

#if defined(XP_PC) || defined(XP_BEOS) || defined(XP_MAC) || defined(MOZ_WIDGET_PHOTON)
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
    mInfo.dct_method = JDCT_FASTEST;
    mInfo.dither_mode = JDITHER_ORDERED;
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

  return NS_OK;
}


int
nsJPEGDecoder::OutputScanlines(int num_scanlines)
{
  PRUint32 top = mInfo.output_scanline;
  PRBool rv = PR_TRUE;

  while ((mInfo.output_scanline < mInfo.output_height) && num_scanlines--) {
      JSAMPROW samples;
      
      /* Request one scanline.  Returns 0 or 1 scanlines. */
      int ns = jpeg_read_scanlines(&mInfo, mSamples, 1);
      
      if (ns != 1) {
        rv = PR_FALSE; /* suspend */
        break;
      }

      /* If grayscale image ... */
      if (mInfo.output_components == 1) {
        JSAMPLE j;
        JSAMPLE *j1 = mSamples[0];
        const JSAMPLE *j1end = j1 + mInfo.output_width;
        JSAMPLE *j3 = mSamples3[0];

        /* Convert from grayscale to RGB. */
        while (j1 < j1end) {
#ifdef XP_MAC
          j = *j1++;
          j3[0] = 0;
          j3[1] = j;
          j3[2] = j;
          j3[3] = j;
          j3 += 4;
#else
          j = *j1++;
          j3[0] = j;
          j3[1] = j;
          j3[2] = j;
          j3 += 3;
#endif
        }
        samples = mSamples3[0];
      } else {
        /* 24-bit color image */
#if defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
        memset(mRGBPadRow, 0, mInfo.output_width * 4);
        PRUint8 *ptrOutputBuf = mRGBPadRow;

        JSAMPLE *j1 = mSamples[0];
        for (PRUint32 i=0;i<mInfo.output_width;++i) {
          ptrOutputBuf[2] = *j1++;
          ptrOutputBuf[1] = *j1++;
          ptrOutputBuf[0] = *j1++;
          ptrOutputBuf += 3;
        }

        samples = mRGBPadRow;
#elif defined(XP_MAC)
        memset(mRGBPadRow, 0, mInfo.output_width * 4);
        PRUint8 *ptrOutputBuf = mRGBPadRow;

        JSAMPLE *j1 = mSamples[0];
        for (PRUint32 i=0;i<mInfo.output_width;++i) {
          ptrOutputBuf[0] = 0;
          ptrOutputBuf[1] = *j1++;
          ptrOutputBuf[2] = *j1++;
          ptrOutputBuf[3] = *j1++;
          ptrOutputBuf += 4;
        }

        samples = mRGBPadRow;
#else
        samples = mSamples[0];
#endif
      }

      PRUint32 bpr;
      mFrame->GetImageBytesPerRow(&bpr);
      mFrame->SetImageData(
        samples,             // data
        bpr,                 // length
        (mInfo.output_scanline-1) * bpr); // offset
  }

  if (top != mInfo.output_scanline) {
      nsRect r(0, top, mInfo.output_width, mInfo.output_scanline-top);
      mObserver->OnDataAvailable(nsnull, nsnull, mFrame, &r);
  }

  return rv;
}


/* [noscript] unsigned long writeSegments (in nsReadSegmentFun reader, in voidPtr closure, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean nonBlocking; */
NS_IMETHODIMP nsJPEGDecoder::GetNonBlocking(PRBool *aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsJPEGDecoder::SetNonBlocking(PRBool aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIOutputStreamObserver observer; */
NS_IMETHODIMP nsJPEGDecoder::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsJPEGDecoder::SetObserver(nsIOutputStreamObserver * aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}





/* Override the standard error method in the IJG JPEG decoder code. */
void PR_CALLBACK
my_error_exit (j_common_ptr cinfo)
{
  nsresult error_code = NS_ERROR_FAILURE;
  decoder_error_mgr *err = (decoder_error_mgr *) cinfo->err;

#if 0
#ifdef DEBUG
/*ptn fix later */
  if (il_debug >= 1) {
      char buffer[JMSG_LENGTH_MAX];

      /* Create the message */
      (*cinfo->err->format_message) (cinfo, buffer);

      ILTRACE(1,("%s\n", buffer));
  }
#endif


  /* Convert error to a browser error code */
  if (cinfo->err->msg_code == JERR_OUT_OF_MEMORY)
      error_code = MK_OUT_OF_MEMORY;
  else
      error_code = MK_IMAGE_LOSSAGE;
#endif

  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message) (cinfo, buffer);

#ifdef DEBUG
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
/* data source manager method 
/******************************************************************************/


/******************************************************************************/
/* data source manager method 
	Initialize source.  This is called by jpeg_read_header() before any
	data is actually read.  May leave
	bytes_in_buffer set to 0 (in which case a fill_input_buffer() call
	will occur immediately).
*/
void PR_CALLBACK
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
void PR_CALLBACK
skip_input_data (j_decompress_ptr jd, long num_bytes)
{
  decoder_source_mgr *src = (decoder_source_mgr *)jd->src;

  if (num_bytes > (long)src->pub.bytes_in_buffer) {
    /*
     * Can't skip it all right now until we get more data from
     * network stream. Set things up so that fill_input_buffer
     * will skip remaining amount.
     */
    src->decoder->mBytesToSkip = (size_t)num_bytes - src->pub.bytes_in_buffer;
    src->pub.next_input_byte += src->pub.bytes_in_buffer;
    src->pub.bytes_in_buffer = 0;

  } else {
    /* Simple case. Just advance buffer pointer */

    src->pub.bytes_in_buffer -= (size_t)num_bytes;
    src->pub.next_input_byte += num_bytes;
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
boolean PR_CALLBACK
fill_input_buffer (j_decompress_ptr jd)
{
  decoder_source_mgr *src = (decoder_source_mgr *)jd->src;

  unsigned char *new_buffer = (unsigned char *)src->decoder->mBuffer;
  PRUint32 new_buflen = src->decoder->mBufferLen;
  PRUint32 bytesToSkip = src->decoder->mBytesToSkip;

  switch(src->decoder->mFillState) {
  case READING_BACK:
    {
      if (!new_buffer || new_buflen == 0)
        return PR_FALSE; /* suspend */

      src->decoder->mBufferLen = 0;

      if (bytesToSkip != 0) {
        if (bytesToSkip < new_buflen) {
          /* All done skipping bytes; Return what's left. */
          new_buffer += bytesToSkip;
          new_buflen -= bytesToSkip;
          src->decoder->mBytesToSkip = 0;
        } else {
          /* Still need to skip some more data in the future */
          src->decoder->mBytesToSkip -= (size_t)new_buflen;
          return PR_FALSE; /* suspend */
        }
      }

      src->decoder->mBackBufferUnreadLen = src->pub.bytes_in_buffer;

      src->pub.next_input_byte = new_buffer;
      src->pub.bytes_in_buffer = (size_t)new_buflen;
      src->decoder->mFillState = READING_NEW;

      return PR_TRUE;
    }
    break;

  case READING_NEW:
    {
      if (src->pub.next_input_byte != src->decoder->mBuffer) {
        /* Backtrack data has been permanently consumed. */
        src->decoder->mBackBufferUnreadLen = 0;
        src->decoder->mBackBufferLen = 0;
      }

      /* Save remainder of netlib buffer in backtrack buffer */
      PRUint32 new_backtrack_buflen = src->pub.bytes_in_buffer + src->decoder->mBackBufferLen;


      /* Make sure backtrack buffer is big enough to hold new data. */
      if (src->decoder->mBackBufferSize < new_backtrack_buflen) {

        /* Round up to multiple of 16 bytes. */
        PRUint32 roundup_buflen = ((new_backtrack_buflen + 15) >> 4) << 4;
        if (src->decoder->mBackBufferSize) {
            src->decoder->mBackBuffer =
          (JOCTET *)PR_REALLOC(src->decoder->mBackBuffer, roundup_buflen);
        } else {
          src->decoder->mBackBuffer = (JOCTET*)PR_MALLOC(roundup_buflen);
        }

        /* Check for OOM */
        if (!src->decoder->mBackBuffer) {
#if 0
          j_common_ptr cinfo = (j_common_ptr)(&src->js->jd);
          cinfo->err->msg_code = JERR_OUT_OF_MEMORY;
          my_error_exit(cinfo);
#endif
        }
          
        src->decoder->mBackBufferSize = (size_t)roundup_buflen;

        /* Check for malformed MARKER segment lengths. */
        if (new_backtrack_buflen > MAX_JPEG_MARKER_LENGTH) {
          my_error_exit((j_common_ptr)(&src->decoder->mInfo));
        }
      }


      /* Copy remainder of netlib buffer into backtrack buffer. */
      nsCRT::memmove(src->decoder->mBackBuffer + src->decoder->mBackBufferLen,
                     src->pub.next_input_byte,
                     src->pub.bytes_in_buffer);


      /* Point to start of data to be rescanned. */
      src->pub.next_input_byte = src->decoder->mBackBuffer + src->decoder->mBackBufferLen - src->decoder->mBackBufferUnreadLen;
      src->pub.bytes_in_buffer += src->decoder->mBackBufferUnreadLen;
      src->decoder->mBackBufferLen = (size_t)new_backtrack_buflen;
    
      src->decoder->mFillState = READING_BACK;

      return PR_FALSE;
    }
    break;
  }

  return PR_FALSE;
}

/******************************************************************************/
/* data source manager method */
/*
 * Terminate source --- called by jpeg_finish_decompress() after all
 * data has been read to clean up JPEG source manager. NOT called by 
 * jpeg_abort() or jpeg_destroy().
 */
void PR_CALLBACK
term_source (j_decompress_ptr jd)
{
  decoder_source_mgr *src = (decoder_source_mgr *)jd->src;

  if (src->decoder->mObserver) {
    src->decoder->mObserver->OnStopFrame(nsnull, nsnull, src->decoder->mFrame);
    src->decoder->mObserver->OnStopContainer(nsnull, nsnull, src->decoder->mImage);
    src->decoder->mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
  }

    /* No work necessary here */
}
