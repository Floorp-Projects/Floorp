/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 *	  jpeg.c --- Glue code to Independent JPEG Group decoder library
 *    $Id: jpeg.cpp,v 1.2 1999/04/19 21:28:48 pnunn%netscape.com Exp $
 */



#include "if_struct.h"

#include "dllcompat.h"
#include "nsIImgDecoder.h"
#include "nsImgDecCID.h"
#include "nsJPGDecoder.h"
#include "nsJPGCallback.h"

#include "jpeg.h"
#include "merrors.h"
#include "il.h"

#include <ctype.h>		/* to declare isprint() */
#ifndef XP_MAC
#	 include <sys/types.h>
#endif

PR_BEGIN_EXTERN_C
#include "jpeglib.h"
#include "jerror.h"

extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C

#ifdef XP_OS2_HACK
/* IBM-MAS: We removed setjmp.h from XP_core.h, now we need it here. */
/*          We need to see if we can fix hwthreads/XP_CORE correctly.. */
#include <setjmp.h>
#endif

#ifdef PROFILE
#	 pragma profile on
#endif

/* Normal JFIF markers can't have more bytes than this. */
#define MAX_JPEG_MARKER_LENGTH	(((uint32)1 << 16) - 1)

/*
 * States that the jpeg decoder might be in
 */
typedef enum {
	JPEG_HEADER,                          /* Reading JFIF headers */
    JPEG_START_DECOMPRESS,
	JPEG_DECOMPRESS_PROGRESSIVE,          /* Output progressive pixels */
	JPEG_DECOMPRESS_SEQUENTIAL,           /* Output sequential pixels */
    JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT,
	JPEG_DONE,
    JPEG_SINK_NON_JPEG_TRAILER           /* Some image files have a */
                                         /* non-JPEG trailer */
} jstate;
	
typedef struct {
	struct jpeg_error_mgr pub;	/* "public" fields for IJG library*/
	jmp_buf setjmp_buffer;		/* For handling catastropic errors */
} il_error_mgr;

/*
 * Structure used to manage the JPEG decoder stream.
 */
typedef struct jpeg_struct {
	jstate state;				/* Decoder FSM state */
    int pass_num;
    int completed_output_passes;

    int rows_per_chunk;
    void *timeout;
    
	/* One scanline's worth of post-processed sample data */
	JSAMPARRAY samples;
	JSAMPARRAY samples3;
	/* IJG JPEG library decompressor state */
	struct jpeg_decompress_struct jd;
	il_error_mgr jerr;
	il_container *ic;
} jpeg_struct;




/* Possible states for JPEG source manager */
enum data_source_state {
	dss_consuming_backtrack_buffer = 0, /* Must be zero for init purposes */
	dss_consuming_netlib_buffer
};

/*
 *	Implementation of a JPEG src object that understands our state machine
 */
typedef struct {
	/* public fields; must be first in this struct! */
	struct jpeg_source_mgr pub;

	jpeg_struct *js;              /* pointer to netlib stream object */

	int bytes_to_skip;            /* remaining bytes to skip */

	enum data_source_state state;

	JOCTET *netlib_buffer;        /* next buffer for fill_input_buffer */
	uint32 netlib_buflen;

	/*
	 * Buffer of "remaining" characters left over after a call to 
	 * fill_input_buffer(), when no additional data is available.
	 */ 
	JOCTET *backtrack_buffer;
	size_t backtrack_buffer_size; /* Allocated size of backtrack_buffer		*/
	size_t backtrack_buflen;	  /* Offset of end of active backtrack data */
	size_t backtrack_num_unread_bytes; /* Length of active backtrack data	*/
} il_source_mgr;


/* Override the standard error method in the IJG JPEG decoder code. */
void PR_CALLBACK
il_error_exit (j_common_ptr cinfo)
{
    int error_code;
	il_error_mgr *err = (il_error_mgr *) cinfo->err;

#ifdef DEBUG
#if 0
  //ptn fix later
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
}


void PR_CALLBACK
init_source (j_decompress_ptr jd)
{
}

/*-----------------------------------------------------------------------------
 * This is the callback routine from the IJG JPEG library used to supply new
 * data to the decompressor when its input buffer is exhausted.	 It juggles
 * multiple buffers in an attempt to avoid unnecessary copying of input data.
 *
 * (A simpler scheme is possible: It's much easier to use only a single
 * buffer; when fill_input_buffer() is called, move any unconsumed data
 * (beyond the current pointer/count) down to the beginning of this buffer and
 * then load new data into the remaining buffer space.	This approach requires
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
 *	TRUE if additional data is available, FALSE if no data present and
 *	 the JPEG library should therefore suspend processing of input stream
 *---------------------------------------------------------------------------*/
boolean PR_CALLBACK
fill_input_buffer (j_decompress_ptr jd)
{
	il_source_mgr *src = (il_source_mgr *)jd->src;
	enum data_source_state data_source_state = src->state;
    uint32 bytesToSkip, new_backtrack_buflen, new_buflen, roundup_buflen;
	unsigned char *new_buffer;

	ILTRACE(5,("il:jpeg: fill, state=%d, nib=0x%x, bib=%d", data_source_state,
               src->pub.next_input_byte, src->pub.bytes_in_buffer));

	switch (data_source_state) {

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
                j_common_ptr cinfo = (j_common_ptr)(&src->js->jd);
                cinfo->err->msg_code = JERR_OUT_OF_MEMORY;
				il_error_exit(cinfo);
            }
                
			src->backtrack_buffer_size = (size_t)roundup_buflen;

			/* Check for malformed MARKER segment lengths. */
			if (new_backtrack_buflen > MAX_JPEG_MARKER_LENGTH)
				il_error_exit((j_common_ptr)(&src->js->jd));
		}

		/* Copy remainder of netlib buffer into backtrack buffer. */
		XP_BCOPY (src->pub.next_input_byte,
				  src->backtrack_buffer + src->backtrack_buflen,
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
    ILTRACE(5,("         Suspending, bib=%d", src->pub.bytes_in_buffer));
    return FALSE;
}

void PR_CALLBACK
skip_input_data (j_decompress_ptr jd, long num_bytes)
{
	il_source_mgr *src = (il_source_mgr *)jd->src;

#ifdef DEBUG
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
		src->bytes_to_skip = (size_t)num_bytes - src->pub.bytes_in_buffer;
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


/*-----------------------------------------------------------------------------
 * Setup a JPEG source object for streaming data in a demand-driven
 * fashion into the IJG JPEG decompression library.	 A JPEG source
 * object consists of a set of callback functions which the
 * decompressor library calls when it needs more data or to seek ahead
 * in the input stream.
 *
 * Returns:
 *	TRUE if setup succeeds, FALSE otherwise.
 *---------------------------------------------------------------------------*/
static int
setup_jpeg_src (j_decompress_ptr jd, jpeg_struct *js)
{
	il_source_mgr *src;

	if (jd->src == NULL) {
		src = PR_NEWZAP(il_source_mgr);
		if (!src) {
			ILTRACE(1,("il:jpeg: src manager memory lossage"));
			return FALSE;
		}
		jd->src = (struct jpeg_source_mgr *) src;
	}

	src = (il_source_mgr *)jd->src;
	src->js = js;

	/* Setup callback functions. */
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;

	return TRUE;
}



/*
 * Macros for fetching data from the data source module.
 *
 * At all times, cinfo->src->next_input_byte and ->bytes_in_buffer reflect
 * the current restart point; we update them only when we have reached a
 * suitable place to restart if a suspension occurs.
 */

/* Declare and initialize local copies of input pointer/count */
#define INPUT_VARS(cinfo)  \
	struct jpeg_source_mgr * datasrc = (cinfo)->src;  \
	const JOCTET * next_input_byte = datasrc->next_input_byte;  \
	size_t bytes_in_buffer = datasrc->bytes_in_buffer

/* Unload the local copies --- do this only at a restart boundary */
#define INPUT_SYNC(cinfo)  \
	( datasrc->next_input_byte = next_input_byte,  \
	  datasrc->bytes_in_buffer = bytes_in_buffer )

/* Reload the local copies --- seldom used except in MAKE_BYTE_AVAIL */
#define INPUT_RELOAD(cinfo)  \
	( next_input_byte = datasrc->next_input_byte,  \
	  bytes_in_buffer = datasrc->bytes_in_buffer )

/* Internal macro for INPUT_BYTE and INPUT_2BYTES: make a byte available.
 * Note we do *not* do INPUT_SYNC before calling fill_input_buffer,
 * but we must reload the local copies after a successful fill.
 */
#define MAKE_BYTE_AVAIL(cinfo,action)  \
	if (bytes_in_buffer == 0) {  \
	  if (! (*datasrc->fill_input_buffer) (cinfo))  \
	    { action; }  \
	  INPUT_RELOAD(cinfo);  \
	}  \
	bytes_in_buffer--

/* Read a byte into variable V.
 * If must suspend, take the specified action (typically "return FALSE").
 */
#define INPUT_BYTE(cinfo,V,action)  \
	MAKESTMT( MAKE_BYTE_AVAIL(cinfo,action); \
		  V = GETJOCTET(*next_input_byte++); )

/* As above, but read two bytes interpreted as an unsigned 16-bit integer.
 * V should be declared unsigned int or perhaps INT32.
 */
#define INPUT_2BYTES(cinfo,V,action)  \
	MAKESTMT( MAKE_BYTE_AVAIL(cinfo,action); \
		  V = ((unsigned int) GETJOCTET(*next_input_byte++)) << 8; \
		  MAKE_BYTE_AVAIL(cinfo,action); \
		  V += GETJOCTET(*next_input_byte++); )


/* Process the COM marker segment, which contains human-readable comments. */
boolean PR_CALLBACK
il_jpeg_COM_handler (j_decompress_ptr cinfo)
{
    uint length;
    char *comment;
    unsigned int ch;

    il_source_mgr *src = (il_source_mgr*) cinfo->src;
    il_container *ic = src->js->ic;
    static unsigned int lastch = 0;

    INPUT_VARS(cinfo);

    /* Get 16-bit comment length word. */
    INPUT_2BYTES(cinfo, length, return FALSE);
    length -= 2;			/* discount the length word itself */
  
    PR_FREEIF(ic->comment);
    comment = ic->comment = (char *)PR_MALLOC(length + 1);
    ic->comment_length = length;
    
    if (!ic->comment) {
        skip_input_data(cinfo, length + 2);
        return TRUE;
    }

    /* Emit the character in a readable form.
     * Newlines in CR, CR/LF, or LF form will be printed as one newline.
     */
    while (length-- > 0) {
        INPUT_BYTE(cinfo, ch, return FALSE);

        if (ch == '\r') {
            *comment++ = '\n';
        } else if (ch == '\n') {
            if (lastch != '\r')
                *comment++ = '\n';
        } else if (isprint(ch) || !ch) {
            *comment++ = ch;
        }
        lastch = ch;
    }

    *comment = 0;

    INPUT_SYNC(cinfo);

    return TRUE;
}

int il_jpeg_init(il_container *ic)
{
	jpeg_struct *js;
	j_decompress_ptr jd;
    NI_ColorSpace *src_color_space = ic->src_header->color_space;
    NI_RGBBits *rgb = &src_color_space->bit_alloc.rgb;

	js = PR_NEWZAP(jpeg_struct);
	if (!js) {
		ILTRACE(1,("il:jpeg: jpeg_struct memory lossage"));
		return FALSE;
	}

	/* Init jpeg_struct */
	ic->ds = js;
	js->state = JPEG_HEADER;
	js->samples = NULL;
	js->samples3 = NULL;
	js->ic = ic;

	jd = &js->jd;

	/* Install our error handler because the default is to exit(). */
	jd->err = jpeg_std_error(&js->jerr.pub);
	js->jerr.pub.error_exit = il_error_exit;

	/* Control returns here if an error occurs before setup completes. */
	if(setjmp(js->jerr.setjmp_buffer)) {
        /* Free up all the data structures */
        il_jpeg_abort(ic);
		return FALSE;
	}

#ifdef DEBUG
#if 0
	if (il_debug > 20) {
		jd->err->trace_level = 99;
	}
#endif
#endif
	jpeg_create_decompress(jd);

	/* Setup jpeg data source object */
	if (!setup_jpeg_src(jd, js)) {
	  ILTRACE(1,("il:jpeg: jpeg source memory lossage"));
          /* Free up all the data structures */
          il_jpeg_abort(ic);
	  return FALSE;
	}

    /* Insert custom COM comment marker processor. */
    jpeg_set_marker_processor(jd, JPEG_COM, il_jpeg_COM_handler);


    /* Initialize the container's source image header. */
    src_color_space->type = NI_TrueColor;
    src_color_space->pixmap_depth = 24;
    rgb->red_bits = 8;
    rgb->red_shift = 16;
    rgb->green_bits = 8;
    rgb->green_shift = 8;
    rgb->blue_bits = 8;
    rgb->blue_shift = 0;

	return TRUE;
}


/*-----------------------------------------------------------------------------
 * Calling this routine sends scanlines to the front-end for display.
 * Scanlines will be emitted until the entire scan has been displayed or
 * until insufficient input data is available to continue output.
 *
 * The maximum number of scanlines to be output is controlled by the
 * `num_scanlines' parameter.  Set num_scanlines to -1 to output scanlines
 * until input data is exhausted.
 *
 * Returns:
 *	TRUE if output was discontinued due to lack of input data
 *  FALSE, otherwise
 *---------------------------------------------------------------------------*/
int
output_jpeg_scanlines(il_container *ic, int num_scanlines)
{
	jpeg_struct *js = (jpeg_struct *)ic->ds;
	j_decompress_ptr jd = &js->jd;
    int input_exhausted;
    int pass;
    
#ifdef DEBUG
    uint start_scanline = jd->output_scanline;
#endif

    if (js->state == JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT)
        pass = IL_FINAL_PASS;
    else
        pass = js->completed_output_passes + 1;

    while ((jd->output_scanline < jd->output_height) && num_scanlines--) {
        JSAMPROW samples;
        
        /* Request one scanline.  Returns 0 or 1 scanlines. */
        int ns = jpeg_read_scanlines(jd, js->samples, 1);
        ILTRACE(15,("il:jpeg: scanline %d, ns = %d",
                    jd->output_scanline, ns));
        if (ns != 1) {
            ILTRACE(5,("il:jpeg: suspending scanline"));
            input_exhausted = TRUE;
            goto done;
        }

        /* If grayscale image ... */
        if (jd->output_components == 1) {
            JSAMPLE j, *j1, *j1end, *j3;

            /* Convert from grayscale to RGB. */
            j1 = js->samples[0];
            j1end = j1 + jd->output_width;
            j3 = js->samples3[0];
            while (j1 < j1end) {
                j = *j1++;
                j3[0] = j;
                j3[1] = j;
                j3[2] = j;
                j3 += 3;
            }
            samples = js->samples3[0];
        } else {		/* 24-bit color image */
            samples = js->samples[0];
        }

        ic->imgdcb->ImgDCBHaveRow( 0, samples, 0, jd->output_width, jd->output_scanline-1,
                    1, ilErase, pass);
    }

    input_exhausted = FALSE;

  done:

#ifdef DEBUG
    if (start_scanline != jd->output_scanline)
        ILTRACE(4, ("il: jpeg: Input pass=%2d, next input scanline=%3d,"
                    " emitted %3d - %3d\n",
                    jd->input_scan_number, jd->input_iMCU_row * 16,
                    start_scanline, jd->output_scanline - 1));

#endif
    
    return input_exhausted;
}

#define JPEG_OUTPUT_CHUNK_SIZE     50000

/* Timeout durations, in milliseconds */

/* Delay between displaying chunks of pixels for the first scan. */
#define JPEG_TIMEOUT_INITIAL_DELAY   100

/* Delay between displaying chunks of pixels for subsequent scans */
#define JPEG_TIMEOUT_DELAY           200

static void
jpeg_timeout_callback(void *closure)
{
    uint32 delay;
	jpeg_struct *js = (jpeg_struct *)closure;
	j_decompress_ptr jd = &js->jd;

    if (jd->input_scan_number == 1)
        delay = JPEG_TIMEOUT_INITIAL_DELAY;
    else
        delay = JPEG_TIMEOUT_DELAY;

    /*
     * Perform incremental display of progressive scans,
     * except don't display unless enough time has elapsed
     * since the previous scan was displayed.
     */

    if (js->pass_num != js->completed_output_passes + 1) {
        if (! jpeg_start_output(jd, jd->input_scan_number)) {
            ILTRACE(1, ("il: jpeg: jpeg_start_output returned"
                        "FALSE!\n"));
            goto done;
        }
        js->pass_num = js->completed_output_passes + 1;
    }
    
    js->timeout = NULL;

    /* If there's no more data to process for this scan,
       wait until jpeg_write() wakes us up by scheduling a
       new timeout */
    if (output_jpeg_scanlines(js->ic, js->rows_per_chunk))
        return;
    
    /* If we're at the end of this progressive scan ... */
    if (jd->output_scanline == jd->output_height) {
        if (jpeg_finish_output(jd))
            js->completed_output_passes++;
    }

  done:
    js->timeout = js->ic->imgdcb->ImgDCBSetTimeout(jpeg_timeout_callback, js, delay);
}

/*
 * Force the display of scans, even if no data has entered the netlib
 * buffer since jpeg_timeout_callback() was run.  This will flush
 * pixels to the screen during a long pause in a bursty data source.
 */
#if 0
static void
jpeg_idle_callback(void *closure)
{
    il_container *ic = (il_container *)closure;
    il_jpeg_write(ic, NULL, 0);
}
#endif

int
il_jpeg_write(il_container *ic, const unsigned char *buf, int32 len)
{
	int row_stride, status;
	int input_exhausted;
  int error_code;

	jpeg_struct *js = (jpeg_struct *)ic->ds;


  
     /* If this js == NULL, chances are the netlib 
       continued to send data after the image stream was closed. */
    if (!js) {
#ifdef DEBUG
        ILTRACE(1,("Netlib sent data after the image stream was closed\n"));
#endif
        return MK_IMAGE_LOSSAGE;
    }

	j_decompress_ptr jd = &js->jd;
    il_source_mgr *src = (il_source_mgr*) js->jd.src;
    NI_PixmapHeader *img_header = &ic->image->header;
#ifndef M12N                    /* XXXM12N Get rid of this */
    NI_PixmapHeader *src_header = ic->src_header;
    NI_ColorMap *cmap = &src_header->color_space->cmap;
#endif /* M12N */

	/* Return here if there is a fatal error. */
	if ((error_code = setjmp(js->jerr.setjmp_buffer)) != 0) {
        /* Free up all the data structures */
        il_jpeg_abort(ic);
		return error_code;
	}

	/* Register new buffer contents with data source manager. */
	src->netlib_buffer = (JOCTET*)buf;
	src->netlib_buflen = (uint32)len;

    input_exhausted = 0;
	while (! input_exhausted) {
		ILTRACE(5,("il:jpeg: write, state=%d, buf=0x%x, len=%d",
				   js->state, buf, len));

		switch (js->state) {
		case JPEG_HEADER:
			if (jpeg_read_header(jd, TRUE) != JPEG_SUSPENDED) {
#ifndef M12N                    /* XXXM12N Get rid of this */
				cmap->map = 0;
				cmap->num_colors = ic->cs->default_map_size;
#endif /* M12N */
                ic->src_header->width = jd->image_width;
                ic->src_header->height = jd->image_height;
				if ((status = ic->imgdcb->ImgDCBImageSize()) != 0) {
					ILTRACE(1,("il:jpeg: MEM il_size"));
					return status;
				}

                ic->imgdcb->ImgDCBSetupColorspaceConverter(); /* XXXM12N Should check
                                                       return code. */

                js->rows_per_chunk =
                    JPEG_OUTPUT_CHUNK_SIZE / img_header->widthBytes;

				/* FIXME -- Should reset dct_method and dither mode
				 * for final pass of progressive JPEG
				 */
				jd->dct_method = JDCT_FASTEST;
				jd->dither_mode = JDITHER_ORDERED;
				jd->do_fancy_upsampling = FALSE;
				jd->enable_2pass_quant = FALSE;
				jd->do_block_smoothing = TRUE;
				
				/*
				 * Don't allocate a giant and superfluous memory buffer
				 * when the image is a sequential JPEG.
				 */
				jd->buffered_image = jpeg_has_multiple_scans(jd);

				/* Used to set up image size so arrays can be allocated */
				jpeg_calc_output_dimensions(jd);

				/*
				 * Make a one-row-high sample array that will go away
				 * when done with image. Always make it big enough to
				 * hold an RGB row.	 Since this uses the IJG memory
				 * manager, it must be allocated before the call to
				 * jpeg_start_compress().
				 */
				row_stride = jd->output_width * jd->output_components;
				js->samples = (*jd->mem->alloc_sarray)((j_common_ptr) jd,
													   JPOOL_IMAGE,
													   row_stride, 1);

				/* Allocate RGB buffer for conversion from greyscale. */
				if (jd->output_components != 3) {
					row_stride = jd->output_width * 3;
					js->samples3 = (*jd->mem->alloc_sarray)((j_common_ptr) jd,
															JPOOL_IMAGE,
															row_stride, 1);
				}
                js->state = JPEG_START_DECOMPRESS;

			} else {
				ILTRACE(5,("il:jpeg: suspending header"));
				input_exhausted = TRUE;
			}
			break;

        case JPEG_START_DECOMPRESS:
            if (jpeg_start_decompress(jd)) {

                /* If this is a progressive JPEG ... */
                if (jd->buffered_image) {
                    js->state = JPEG_DECOMPRESS_PROGRESSIVE;
                } else {
                    js->state = JPEG_DECOMPRESS_SEQUENTIAL;
                }
            }
                   
            break;

        case JPEG_DECOMPRESS_SEQUENTIAL:
            input_exhausted = output_jpeg_scanlines(ic, -1);

			/* If we've completed image output ... */
			if (jd->output_scanline == jd->output_height)
                js->state = JPEG_DONE;

            break;
            
		case JPEG_DECOMPRESS_PROGRESSIVE:

            /*
             * Set a timeout to trigger display of the next progressive scan.
             * Any scans which arrive in the intervening time will be displayed
             * instead.  Thus, the decoder adapts to the data arrival rate.
             */
            if (js->timeout == NULL) {
                uint32 delay;

                /*
                 * First time around, display the scan a little
                 * quicker than in subsequent scans.
                 */
                if (jd->input_scan_number == 1)
                    delay = JPEG_TIMEOUT_INITIAL_DELAY;
                else {
                    delay = JPEG_TIMEOUT_DELAY;
                }
                
                js->timeout = ic->imgdcb->ImgDCBSetTimeout(jpeg_timeout_callback, js, delay);
            
            }

            /* Eat all the available input data in the netlib buffer. */
            do {
                status = jpeg_consume_input(jd);
            } while (!((status == JPEG_SUSPENDED) ||
                       (status == JPEG_REACHED_EOI)));

            /* If we've parsed the whole input, do final display immediately. */
            if (status == JPEG_REACHED_EOI) {
                js->state = JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT;
                break;
            }
            
            input_exhausted = TRUE;
            
			break;

		case JPEG_FINAL_PROGRESSIVE_SCAN_OUTPUT:

            if ((jd->input_scan_number == jd->output_scan_number) &&
                (js->pass_num == js->completed_output_passes + 1)) {
                output_jpeg_scanlines(ic, -1);
                jpeg_finish_output(jd);
            } else {

                /* Abort the last output scan.
                 * We need to redraw the whole image.
                 */
                if (js->pass_num == js->completed_output_passes + 1)
                    jpeg_finish_output(jd);

                jpeg_start_output(jd, jd->input_scan_number);
                output_jpeg_scanlines(ic, -1);
                jpeg_finish_output(jd);
            }

            js->state = JPEG_DONE;

            /* Fall through ... */

        case JPEG_DONE:
			status = jpeg_finish_decompress(jd);
            
            /* Clear any pending timeouts */
            if (js->timeout) {
                ic->imgdcb->ImgDCBClearTimeout(js->timeout);
                js->timeout = NULL;
            }

			input_exhausted = TRUE;
            js->state = JPEG_SINK_NON_JPEG_TRAILER;  /* Be prepared for     */
                                                     /* non-JPEG data after */
                                                     /* EOI marker.         */
            break;

        case JPEG_SINK_NON_JPEG_TRAILER: /* Ignore non-JPEG trailer, if any */
            input_exhausted = TRUE;
            break;

		default:
			PR_ASSERT(0);
			break;
		}
	}

	return 0;
}

void
il_jpeg_abort(il_container *ic)
{
    jpeg_struct *js = (jpeg_struct *)ic->ds;
    
	if (js) {
		il_source_mgr *src = (il_source_mgr*) js->jd.src;

		/*
		 * Free up the memory that we allocated ourselves (not memory we
		 * allocated using the IJG memory manager - it will be freed in
		 * a moment.)
		 */
		if (src) {
			if (src->backtrack_buffer) {
				PR_FREEIF(src->backtrack_buffer);
				src->backtrack_buffer = NULL;
			}
			PR_FREEIF(src);
			js->jd.src = NULL;
		}

        /* Clear any pending timeouts */
        if (js->timeout) {
            ic->imgdcb->ImgDCBClearTimeout(js->timeout);
            js->timeout = NULL;
        }
        
		/*
		 * Free all the ancillary memory used during JPEG decoding by the
		 * IJG JPEG library. This has the side effect of freeing up js->samples
         * and js->samples3 which were allocated using the IJG memory manager.
		 */
        jpeg_destroy_decompress(&js->jd);
		js->samples = NULL;
		js->samples3 = NULL;

		/* Finally, free up our private decoder structure. */
		PR_FREEIF(js);
		ic->ds = NULL;
	}
}

void
il_jpeg_complete(il_container *ic)
{
    il_jpeg_abort(ic);
    ic->imgdcb->ImgDCBHaveImageAll();
    ic->imgdcb->ImgDCBHaveImageFrame();
}

#ifdef PROFILE
#	 pragma profile off
#endif

