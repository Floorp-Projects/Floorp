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

#include "nsUnitConverters.h"

#include "nsCRT.h"

NS_IMPL_ISUPPORTS2(nsJPEGDecoder, nsIImageDecoder, nsIOutputStream)


void PR_CALLBACK init_source (j_decompress_ptr jd);
boolean PR_CALLBACK fill_input_buffer (j_decompress_ptr jd);
void PR_CALLBACK skip_input_data (j_decompress_ptr jd, long num_bytes);
void PR_CALLBACK term_source (j_decompress_ptr jd);
void PR_CALLBACK il_error_exit (j_common_ptr cinfo);

/* Normal JFIF markers can't have more bytes than this. */
#define MAX_JPEG_MARKER_LENGTH  (((PRUint32)1 << 16) - 1)


/* Possible states for JPEG source manager */
enum data_source_state {
    dss_consuming_backtrack_buffer = 0, /* Must be zero for init purposes */
    dss_consuming_netlib_buffer
};


/*
 *  Implementation of a JPEG src object that understands our state machine
 */
typedef struct {
  /* public fields; must be first in this struct! */
  struct jpeg_source_mgr pub;


  nsJPEGDecoder *decoder;


  int bytes_to_skip;            /* remaining bytes to skip */

  JOCTET *netlib_buffer;        /* next buffer for fill_input_buffer */
  PRUint32 netlib_buflen;

  enum data_source_state state;


  /*
   * Buffer of "remaining" characters left over after a call to 
   * fill_input_buffer(), when no additional data is available.
   */ 
  JOCTET *backtrack_buffer;
  size_t backtrack_buffer_size; /* Allocated size of backtrack_buffer     */
  size_t backtrack_buflen;      /* Offset of end of active backtrack data */
  size_t backtrack_num_unread_bytes; /* Length of active backtrack data   */
} decoder_source_mgr;


nsJPEGDecoder::nsJPEGDecoder()
{
  NS_INIT_ISUPPORTS();

  mState = JPEG_HEADER;

  mBuf = nsnull;
  mBufLen = 0;

  mCurDataLen = 0;
  mSamples = nsnull;
  mSamples3 = nsnull;

  mBytesToSkip = 0;
}

nsJPEGDecoder::~nsJPEGDecoder()
{

}


/** nsIImageDecoder methods **/

/* void init (in nsIImageRequest aRequest); */
NS_IMETHODIMP nsJPEGDecoder::Init(nsIImageRequest *aRequest)
{
  mRequest = aRequest;

  aRequest->GetImage(getter_AddRefs(mImage));


  /* Step 1: allocate and initialize JPEG decompression object */

  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&mInfo);

  decoder_source_mgr *src;

  if (mInfo.src == NULL) {
    src = PR_NEWZAP(decoder_source_mgr);
    if (!src) {
      return PR_FALSE;
    }
    mInfo.src = (struct jpeg_source_mgr *) src;
  }

  mInfo.err = jpeg_std_error(&mErr.pub);

  mErr.pub.error_exit = il_error_exit;

  /* Setup callback functions. */
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;
  src->pub.term_source = term_source;

  src->decoder = this;


#if 0
  /* We set up the normal JPEG error routines, then override error_exit. */
  mInfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 0;
  }

#endif


  return NS_OK;
}

/* readonly attribute nsIImageRequest request; */
NS_IMETHODIMP nsJPEGDecoder::GetRequest(nsIImageRequest * *aRequest)
{
  *aRequest = mRequest;
  NS_ADDREF(*aRequest);
  return NS_OK;
}






/** nsIOutputStream methods **/

/* void close (); */
NS_IMETHODIMP nsJPEGDecoder::Close()
{
  // XXX progressive? ;)
  OutputScanlines(mInfo.output_height);

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&mInfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */


  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&mInfo);




  printf("nsJPEGDecoder::Close()\n");


  PR_FREEIF(mBuf);


  return NS_OK;
}

/* void flush (); */
NS_IMETHODIMP nsJPEGDecoder::Flush()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long write (in string buf, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::Write(const char *buf, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count); */
NS_IMETHODIMP nsJPEGDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  // XXX above what is this?

  PRUint32 readLen;

  if (!mBuf) {
    mBuf = (char*)PR_Malloc(count);
    mBufLen = count;
  }

  if (mBuf && count > mBufLen)
  {
    mBuf = (char *)PR_Realloc(mBuf, count);
    mBufLen = count;
  }

  nsresult rv = inStr->Read(mBuf, count, &readLen);
  mCurDataLen = readLen;

  /* Step 2: specify data source (eg, a file) */

  // i think this magically happens
//  jpeg_stdio_src(&cinfo, infile);



   /* Register new buffer contents with data source manager. */

  decoder_source_mgr *src = NS_REINTERPRET_CAST(decoder_source_mgr *, mInfo.src);

  if (!mSamples && jpeg_read_header(&mInfo, TRUE) != JPEG_SUSPENDED) {


    /* FIXME -- Should reset dct_method and dither mode
     * for final pass of progressive JPEG
     */
    mInfo.dct_method = JDCT_FASTEST;
    mInfo.dither_mode = JDITHER_ORDERED;
    mInfo.do_fancy_upsampling = FALSE;
    mInfo.enable_2pass_quant = FALSE;
    mInfo.do_block_smoothing = TRUE;

    /*
     * Don't allocate a giant and superfluous memory buffer
     * when the image is a sequential JPEG.
     */
    mInfo.buffered_image = jpeg_has_multiple_scans(&mInfo);

    /* Used to set up image size so arrays can be allocated */
    jpeg_calc_output_dimensions(&mInfo);

    mImage->Init(mInfo.image_width, mInfo.image_height, nsIGFXFormat::RGB);


    /*
     * Make a one-row-high sample array that will go away
     * when done with image. Always make it big enough to
     * hold an RGB row.  Since this uses the IJG memory
     * manager, it must be allocated before the call to
     * jpeg_start_compress().
     */
    int row_stride;
    row_stride = mInfo.output_width * mInfo.output_components;
    mSamples = (*mInfo.mem->alloc_sarray)((j_common_ptr) &mInfo,
                                           JPOOL_IMAGE,
                                           row_stride, 1);

    /* Allocate RGB buffer for conversion from greyscale. */
    if (mInfo.output_components != 3) {
      row_stride = mInfo.output_width * 3;
      mSamples3 = (*mInfo.mem->alloc_sarray)((j_common_ptr) &mInfo,
                                              JPOOL_IMAGE,
                                              row_stride, 1);
    }

  } else { 
    return NS_OK;
  }


  /* Step 3: read file parameters with jpeg_read_header() */
//  (void) jpeg_read_header(&mInfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&mInfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  int status;
  do {
    status = jpeg_consume_input(&mInfo);
  } while (!((status == JPEG_SUSPENDED) ||
             (status == JPEG_REACHED_EOI)));

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
#if 0
  int row_stride = mInfo.output_width * mInfo.output_components; /* physical row width in output buffer */

  /* Make a one-row-high sample array that will go away when done with image */
  JSAMPARRAY buffer = (*mInfo.mem->alloc_sarray)       /* Output row buffer */
		((j_common_ptr) &mInfo, JPOOL_IMAGE, row_stride, 1);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (mInfo.output_scanline < mInfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void) jpeg_read_scanlines(&mInfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    mImage->SetBits(buffer[0], row_stride, row_stride*mInfo.output_scanline /* XXX ??? */);
  }
#endif

  return NS_OK;
}


int
nsJPEGDecoder::OutputScanlines(int num_scanlines)
{
  int input_exhausted;
  int pass;
  
#ifdef DEBUG
  PRUintn start_scanline = mInfo.output_scanline;
#endif

  if (mState == JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT)
      pass = -1;
  else
      pass = mCompletedPasses + 1;

  while ((mInfo.output_scanline < mInfo.output_height) && num_scanlines--) {
      JSAMPROW samples;
      
      /* Request one scanline.  Returns 0 or 1 scanlines. */
      int ns = jpeg_read_scanlines(&mInfo, mSamples, 1);
#if 0
      ILTRACE(15,("il:jpeg: scanline %d, ns = %d",
                  mInfo.output_scanline, ns));
#endif
      if (ns != 1) {
//          ILTRACE(5,("il:jpeg: suspending scanline"));
          input_exhausted = TRUE;
          goto done;
      }

      /* If grayscale image ... */
      if (mInfo.output_components == 1) {
          JSAMPLE j, *j1, *j1end, *j3;

          /* Convert from grayscale to RGB. */
          j1 = mSamples[0];
          j1end = j1 + mInfo.output_width;
          j3 = mSamples3[0];
          while (j1 < j1end) {
              j = *j1++;
              j3[0] = j;
              j3[1] = j;
              j3[2] = j;
              j3 += 3;
          }
          samples = mSamples3[0];
      } else {        /* 24-bit color image */
          samples = mSamples[0];
      }


      mImage->SetBits(samples, strlen((const char *)samples), strlen((const char *)samples)*mInfo.output_scanline /* XXX ??? */);
#if 0
      ic->imgdcb->ImgDCBHaveRow( 0, samples, 0, mInfo.output_width, mInfo.output_scanline-1,
                  1, ilErase, pass);
#endif
  }

  input_exhausted = FALSE;

done:
  
  return input_exhausted;
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
il_error_exit (j_common_ptr cinfo)
{
#if 0
    int error_code;
    il_error_mgr *err = (il_error_mgr *) cinfo->err;

#ifdef DEBUG
#if 0
  /*ptn fix later */
    if (il_debug >= 1) {
        char buffer[JMSG_LENGTH_MAX];

        /* Create the message */
        (*cinfo->err->format_message) (cinfo, buffer);

        ILTRACE(1,("%s\n", buffer));
    }
#endif
#endif

    /* Convert error to a browser error code */
    if (cinfo->err->msg_code == JERR_OUT_OF_MEMORY)
        error_code = MK_OUT_OF_MEMORY;
    else
        error_code = MK_IMAGE_LOSSAGE;
        
    /* Return control to the setjmp point. */
    longjmp(err->setjmp_buffer, error_code);

#endif
}



void PR_CALLBACK
init_source (j_decompress_ptr jd)
{
}

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
 * At any one time, the JPEG decompressor is either reading from the netlib
 * input buffer, which is volatile across top-level calls to the IJG library,
 * or the "backtrack" buffer.  The backtrack buffer contains the remaining
 * unconsumed data from the netlib buffer after parsing was suspended due
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
boolean PR_CALLBACK
fill_input_buffer (j_decompress_ptr jd)
{
  decoder_source_mgr *src = (decoder_source_mgr *)jd->src;

  return src->decoder->FillInput(jd);
}

PRBool nsJPEGDecoder::FillInput(j_decompress_ptr jd)
{
  // read the data from the input stram...

  decoder_source_mgr *src = (decoder_source_mgr *)jd->src;

  if (mCurDataLen == 0)
    return FALSE;

  unsigned char *newBuffer = (unsigned char *)mBuf;
  size_t newBufLen = mCurDataLen;

  if (mBytesToSkip < mCurDataLen) {
    newBuffer += mBytesToSkip;
    newBufLen -= mBytesToSkip;
    mBytesToSkip = 0;
  } else {
    mBytesToSkip -= newBufLen;
    return FALSE; // suspend
  }

  src->pub.next_input_byte = newBuffer;
  src->pub.bytes_in_buffer = newBufLen;

  return TRUE;

#if 0
    enum data_source_state src_state = src->state;
    PRUint32 bytesToSkip, new_backtrack_buflen, new_buflen, roundup_buflen;
    unsigned char *new_buffer;

#if 0
    ILTRACE(5,("il:jpeg: fill, state=%d, nib=0x%x, bib=%d", src_state,
               src->pub.next_input_byte, src->pub.bytes_in_buffer));
#endif
    
    switch (src_state) {

    /* Decompressor reached end of backtrack buffer. Return netlib buffer.*/
    case dss_consuming_backtrack_buffer:
        new_buffer = (unsigned char *)src->netlib_buffer;
        new_buflen = src->netlib_buflen;
        if ((new_buffer == NULL) || (new_buflen == 0))
            goto suspend;

        /*
         * Clear, so that repeated calls to fill_input_buffer() do not
         * deliver the same netlib buffer more than once.
         */
        src->netlib_buflen = 0;
        
        /* Discard data if asked by skip_input_data(). */
        bytesToSkip = src->bytes_to_skip;
        if (bytesToSkip) {
            if (bytesToSkip < new_buflen) {
                /* All done skipping bytes; Return what's left. */
                new_buffer += bytesToSkip;
                new_buflen -= bytesToSkip;
                src->bytes_to_skip = 0;
            } else {
                /* Still need to skip some more data in the future */
                src->bytes_to_skip -= (size_t)new_buflen;
                goto suspend;
            }
        }

        /* Save old backtrack buffer parameters, in case the decompressor
         * backtracks and we're forced to restore its contents.
         */
        src->backtrack_num_unread_bytes = src->pub.bytes_in_buffer;

        src->pub.next_input_byte = new_buffer;
        src->pub.bytes_in_buffer = (size_t)new_buflen;
        src->state = dss_consuming_netlib_buffer;
        return TRUE;

    /* Reached end of netlib buffer. Suspend */
    case dss_consuming_netlib_buffer:
        if (src->pub.next_input_byte != src->netlib_buffer) {
            /* Backtrack data has been permanently consumed. */
            src->backtrack_num_unread_bytes = 0;
            src->backtrack_buflen = 0;
        }

        /* Save remainder of netlib buffer in backtrack buffer */
        new_backtrack_buflen = src->pub.bytes_in_buffer + src->backtrack_buflen;
                
        /* Make sure backtrack buffer is big enough to hold new data. */
        if (src->backtrack_buffer_size < new_backtrack_buflen) {

            /* Round up to multiple of 16 bytes. */
            roundup_buflen = ((new_backtrack_buflen + 15) >> 4) << 4;
            if (src->backtrack_buffer_size) {
                src->backtrack_buffer =
                    (JOCTET *)PR_REALLOC(src->backtrack_buffer, roundup_buflen);
            } else {
                src->backtrack_buffer = (JOCTET*)PR_MALLOC(roundup_buflen);
            }

            /* Check for OOM */
            if (! src->backtrack_buffer) {
#if 0
                j_common_ptr cinfo = (j_common_ptr)(&src->js->jd);
                cinfo->err->msg_code = JERR_OUT_OF_MEMORY;
                il_error_exit(cinfo);
#endif
            }
                
            src->backtrack_buffer_size = (size_t)roundup_buflen;

            /* Check for malformed MARKER segment lengths. */
            if (new_backtrack_buflen > MAX_JPEG_MARKER_LENGTH) {
        //        il_error_exit((j_common_ptr)(&src->js->jd));
            }
        }

        /* Copy remainder of netlib buffer into backtrack buffer. */
        nsCRT::memmove(src->backtrack_buffer + src->backtrack_buflen,
                  src->pub.next_input_byte,
                  src->pub.bytes_in_buffer);

        /* Point to start of data to be rescanned. */
        src->pub.next_input_byte = src->backtrack_buffer +
            src->backtrack_buflen - src->backtrack_num_unread_bytes;
        src->pub.bytes_in_buffer += src->backtrack_num_unread_bytes;
        src->backtrack_buflen = (size_t)new_backtrack_buflen;
        
        src->state = dss_consuming_backtrack_buffer;
        goto suspend;
            
    default:
        PR_ASSERT(0);
        return FALSE;
    }

  suspend:
//    ILTRACE(5,("         Suspending, bib=%d", src->pub.bytes_in_buffer));
    return FALSE;
#endif
    return FALSE;
}

void PR_CALLBACK
skip_input_data (j_decompress_ptr jd, long num_bytes)
{
  decoder_source_mgr *src = (decoder_source_mgr *)jd->src;

#if 0
  ILTRACE(5, ("il:jpeg: skip_input_data js->buf=0x%x js->buflen=%d skip=%d",
              src->netlib_buffer, src->netlib_buflen,
              num_bytes));
#endif

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


/*
 * Terminate source --- called by jpeg_finish_decompress() after all
 * data has been read to clean up JPEG source manager.
 */
void PR_CALLBACK
term_source (j_decompress_ptr jd)
{
    /* No work necessary here */
}