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

#include "png.h"
#include "nsIImgDecoder.h" // include if_struct.h Needs to be first

#include "ipng.h"
#include "dllcompat.h"
#include "pngdec.h"

#include "nsPNGDecoder.h"
#include "nsIImgDCallbk.h"
#include "ilISystemServices.h"

#define MINIMUM_DELAY_TIME 10


static void png_set_dims(il_container *, png_structp);
static void info_callback(png_structp png_ptr, png_infop info);
static void row_callback(png_structp png_ptr, png_bytep new_row,
                         png_uint_32 row_num, int pass);
static void end_callback(png_structp png_ptr, png_infop info);
static void il_png_error_handler(png_structp png_ptr, png_const_charp msg);



int
il_png_init(il_container *ic)
{

	ipng_struct *ipng_p;
    NI_ColorSpace *src_color_space = ic->src_header->color_space;

	ipng_p = PR_NEWZAP(ipng_struct);
	if (ipng_p) 
	{
		ic->ds = ipng_p;
		ipng_p->state = PNG_INIT;
		ipng_p->ic = ic;

        /* Initialize the container's source image header. */
	    /* Always decode to 24 bit pixdepth */

        src_color_space->type = NI_TrueColor;
        src_color_space->pixmap_depth = 24;
        src_color_space->bit_alloc.index_depth = 0;
        return 0;
	}
    return -1;

}

/* Gather n characters from the input stream and then enter state s. */

int 
il_png_write(il_container *ic, const unsigned char *buf, int32 len)
{
    ipng_structp ipng_p;
    png_structp png_ptr;
    png_infop info_ptr;
  
   
    PR_ASSERT(ic != NULL);
	ipng_p = (ipng_structp)ic->ds;   

    if (ipng_p->state == PNG_ERROR)
        return -1;

    if(ipng_p->state == PNG_INIT ){
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, ipng_p,
          il_png_error_handler, NULL);
    	ipng_p->pngs_p = png_ptr;
        /* Allocate/initialize the memory for image information.  REQUIRED. */
	    info_ptr = png_create_info_struct(png_ptr);
        ipng_p->info_p = info_ptr;
	    if (info_ptr == NULL){
		    png_destroy_read_struct(&png_ptr, NULL, NULL);
            ipng_p->state = PNG_ERROR;
		    return -1;
	    }
        png_set_progressive_read_fn(png_ptr, (void *)buf,
        info_callback, row_callback, end_callback); 
    }
	else{
        png_ptr = ipng_p->pngs_p;
        info_ptr = ipng_p->info_p;
    }
    /* note addition of ic to png structure.... */
    png_ptr->io_ptr = ic;

    if (setjmp(ipng_p->jmpbuf)) {
        png_destroy_read_struct(&ipng_p->pngs_p, &ipng_p->info_p, NULL);
        ipng_p->state = PNG_ERROR;
        return -1;
    }

    png_process_data( png_ptr, info_ptr, (unsigned char *)buf, len );
    ipng_p->state = PNG_CONTINUE;
          
    return 0;
}

static void
png_set_dims( il_container *ic, png_structp png_ptr)
{    
    int status;

    NI_PixmapHeader *img_hdr = &ic->image->header;                  
    NI_PixmapHeader *src_hdr = ic->src_header;

    src_hdr->width = img_hdr->width = png_ptr->width;
    src_hdr->height = img_hdr->height = png_ptr->height;
#if 1
    if((png_ptr->num_trans)||(png_ptr->color_type  & PNG_COLOR_MASK_ALPHA))
    {
      ic->image->header.alpha_bits = 1;
      ic->image->header.alpha_shift = 0;
      ic->image->header.is_interleaved_alpha = TRUE;
    }
#else 
    if(png_ptr->num_trans){
      ic->image->header.alpha_bits = 1;
      ic->image->header.alpha_shift = 0;
      ic->image->header.is_interleaved_alpha = TRUE;
    }
    if(png_ptr->color_type  & PNG_COLOR_MASK_ALPHA){
      ic->image->header.alpha_bits = 8;
      ic->image->header.alpha_shift = 0;
      ic->image->header.is_interleaved_alpha = TRUE;
    }

#endif
    status = ic->imgdcb->ImgDCBImageSize();

    /*Note: all png's are decoded to RGB or RGBa and
    converted by imglib to appropriate pixdepth*/

    ic->imgdcb->ImgDCBSetupColorspaceConverter();
    
    return;
}


void
png_delay_time_callback(void *closure)
{
    ipng_struct *ipng_p = (ipng_struct *)closure;

    PR_ASSERT(ipng_p->state == PNG_DELAY);

    if (ipng_p->ic->state == IC_ABORT_PENDING)
        return;                                        
    
    ipng_p->delay_time = 0;         /* Reset for next image */
    return;
}


#define WE_DONT_HAVE_SUBSEQUENT_IMAGES

int
il_png_complete(il_container *ic)
{
#ifndef WE_DONT_HAVE_SUBSEQUENT_IMAGES
	ipng_structp ipng_p;

	ipng_p = (ipng_structp)ic->ds;

	il_png_abort(ic);
#endif
   
	/* notify observers that the current frame has completed. */
                 
    ic->imgdcb->ImgDCBHaveImageFrame();

#ifndef WE_DONT_HAVE_SUBSEQUENT_IMAGES
    /* An image can specify a delay time before which to display
       subsequent images.  Block until the appointed time. */
	if(ipng_p->delay_time < MINIMUM_DELAY_TIME )
		ipng_p->delay_time = MINIMUM_DELAY_TIME ;
	if (ipng_p->delay_time){
			ipng_p->delay_timeout =
			ic->imgdcb->ImgDCBSetTimeout(png_delay_time_callback, ipng_p, ipng_p->delay_time);

			/* Essentially, tell the decoder state machine to wait
			forever.  The delay_time callback routine will wake up the
			state machine and force it to decode the next image. */
			ipng_p->state = PNG_DELAY;
     } else {
		    ipng_p->state = PNG_INIT;
     }
#endif
 
	return 0;
}

int
il_png_abort(il_container *ic)
{
    if (ic->ds) {
        ipng_structp ipng_p = (ipng_structp)ic->ds;

#ifdef WE_DONT_HAVE_SUBSEQUENT_IMAGES
        PR_FREEIF(ipng_p);
        ic->ds = NULL;
#endif
    }
    /*   il_abort( ic ); */
	return 0;
}



/*---------------------------------------------------------------------------
    Former contents of png_png.cpp, a.k.a. libpng's example.c (modified):
  ---------------------------------------------------------------------------*/

static void
info_callback(png_structp png_ptr, png_infop info)
{
/* do any setup here, including setting any of the transformations
 * mentioned in the Reading PNG files section.  For now, you _must_
 * call either png_start_read_image() or png_read_update_info()
 * after all the transformations are set (even if you don't set
 * any).  You may start getting rows before png_process_data()
 * returns, so this is your last chance to prepare for that.
 */
    int number_passes;
    double screen_gamma;

    /*always decode to 24 bit*/
    if(png_ptr->color_type == PNG_COLOR_TYPE_PALETTE && png_ptr->bit_depth <= 8)
       png_set_expand(png_ptr);

    if(png_ptr->color_type == PNG_COLOR_TYPE_GRAY && png_ptr->bit_depth <= 8){
       png_set_gray_to_rgb(png_ptr);
       png_set_expand(png_ptr);
    }

    if(png_get_valid(png_ptr, info, PNG_INFO_tRNS))    
       png_set_expand(png_ptr);


    /* implement scr gamma for mac & unix. (do preferences later.) */
#ifdef XP_MAC
    screen_gamma = 1.7;  /*Mac : 1.7 */
#else
    screen_gamma = 2.2;  /*good for PC.*/
#endif
/*
    if (png_get_gAMA(png_ptr, info, (double *)&png_ptr->gamma))
      png_set_gamma(png_ptr, screen_gamma, png_ptr->gamma);
    else
      png_set_gamma(png_ptr, screen_gamma, 0.45);
*/
    if(png_ptr->interlaced == PNG_INTERLACE_ADAM7)
        number_passes = png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info);

    /* Set the ic values */
    png_set_dims((il_container *)png_ptr->io_ptr, png_ptr);

}



static void
row_callback(png_structp png_ptr, png_bytep new_row,
             png_uint_32 row_num, int pass)
{
/* this function is called for every row in the image.  If the
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
 */
    il_container *ic = (il_container *)png_ptr->io_ptr;  

	if(new_row){

  		ic->imgdcb->ImgDCBHaveRow( 0,  new_row, 0, png_ptr->width, 
        row_num, 1, ilErase /* ilOverlay */, png_ptr->pass );
    
		/*	il_flush_image_data(png_ptr->io_ptr); */
	}


/* where old_row is what was displayed for previous rows.  Note
 * that the first pass (pass == 0 really) will completely cover
 * the old row, so the rows do not have to be initialized.  After
 * the first pass (and only for interlaced images), you will have
 * to pass the current row, and the function will combine the
 * old row and the new row.
 */
}



static void
end_callback(png_structp png_ptr, png_infop info)
{
/* this function is called when the whole image has been read,
 * including any chunks after the image (up to and including
 * the IEND).  You will usually have the same info chunk as you
 * had in the header, although some data may have been added
 * to the comments and time fields.
 *
 * Most people won't do much here, perhaps setting a flag that
 * marks the image as finished.
 */
      il_container *ic = (il_container *)png_ptr->io_ptr;
        
      ic->imgdcb->ImgDCBFlushImage();
}



static void
il_png_error_handler(png_structp png_ptr, png_const_charp msg)
{
    ipng_structp ipng_p;

    /* This function, aside from the extra step of retrieving the "error
     * pointer" (below) and the fact that it exists within the application
     * rather than within libpng, is essentially identical to libpng's
     * default error handler.  The second point is critical:  since both
     * setjmp() and longjmp() are called from the same code, they are
     * guaranteed to have compatible notions of how big a jmp_buf is,
     * regardless of whether _BSD_SOURCE or anything else has (or has not)
     * been defined.  Adapted from readpng2_error_handler() in "PNG: The
     * Definitive Guide" (O'Reilly, 1999). */

    fprintf(stderr, "nspng libpng error: %s\n", msg);
    fflush(stderr);

    ipng_p = (ipng_structp)png_get_error_ptr(png_ptr);
    if (ipng_p == NULL) {            /* we are completely hosed now */
        fprintf(stderr, "nspng severe error:  jmpbuf not recoverable.\n");
        fflush(stderr);
        PR_ASSERT(ipng_p != NULL);   /* instead of exit(99); */
    }

    longjmp(ipng_p->jmpbuf, 1);
}
