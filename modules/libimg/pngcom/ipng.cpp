/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "png.h"
#include "nsIImgDecoder.h" // include if_struct.h Needs to be first

#include "ipng.h"
#include "pngdec.h"

#include "nsPNGDecoder.h"
#include "nsIImgDCallbk.h"
#include "ilISystemServices.h"

#define MINIMUM_DELAY_TIME 10

static void PR_CALLBACK info_callback(png_structp png_ptr, png_infop info);
static void PR_CALLBACK row_callback(png_structp png_ptr, png_bytep new_row,
                         png_uint_32 row_num, int pass);
static void PR_CALLBACK end_callback(png_structp png_ptr, png_infop info);
static void PR_CALLBACK il_png_error_handler(png_structp png_ptr, png_const_charp msg);

int il_debug;
PRLogModuleInfo *il_log_module = NULL;

#define CAN_SUPPORT_8_BIT_MASK

PRBool
il_png_init(il_container *ic)
{

    ipng_struct *ipng_p;
    NI_ColorSpace *src_color_space = ic->src_header->color_space;

    ic->image->header.width = ic->dest_width;
    ic->image->header.height = ic->dest_height;

    ipng_p = PR_NEWZAP(ipng_struct);
    if (!ipng_p) 
        return PR_FALSE;
    
        ic->ds = ipng_p;
        ipng_p->state = PNG_INIT;
        ipng_p->ic = ic;

        /* Initialize the container's source image header. */
        /* Always decode to 24 bit pixdepth */

        src_color_space->type = NI_TrueColor;
        src_color_space->pixmap_depth = 24;
        src_color_space->bit_alloc.index_depth = 0;

        png_structp png_ptr;
        png_infop info_ptr;

        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, ipng_p,
          il_png_error_handler, NULL);
        if (png_ptr == NULL) {
            ipng_p->pngs_p = NULL;
            ipng_p->info_p = NULL;
            ipng_p->state = PNG_ERROR;
            return -1;
        }

        info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == NULL) {
            png_destroy_read_struct(&png_ptr, NULL, NULL);
            ipng_p->pngs_p = NULL;
            ipng_p->info_p = NULL;
            ipng_p->state = PNG_ERROR;
            return -1;
        }

        ipng_p->pngs_p = png_ptr;
        ipng_p->info_p = info_ptr;

        /* use ic as libpng "progressive pointer" (retrieve in callbacks) */
        png_set_progressive_read_fn(png_ptr, (void *)ic, info_callback,
          row_callback, end_callback);

    return PR_TRUE;

}

/* Gather n characters from the input stream and then enter state s. */

int 
il_png_write(il_container *ic, const unsigned char *buf, int32 len)
{
    ipng_structp ipng_p;
  
   
    PR_ASSERT(ic != NULL);
    ipng_p = (ipng_structp)ic->ds;   

    if (ipng_p->state == PNG_ERROR)
        return -1;

    if (setjmp(ipng_p->jmpbuf)) {
        png_destroy_read_struct(&ipng_p->pngs_p, &ipng_p->info_p, NULL);
        ipng_p->state = PNG_ERROR;
        return -1;
    }

    png_process_data(ipng_p->pngs_p, ipng_p->info_p, (unsigned char *)buf, len);
    ipng_p->state = PNG_CONTINUE;
          
    return 0;
}


int
il_png_complete(il_container *ic)
{
    /* notify observers that the current frame has completed. */
    ic->imgdcb->ImgDCBHaveImageAll(); 
    return 0;
}

int
il_png_abort(il_container *ic)
{
    if (ic->ds) {
        ipng_structp ipng_p = (ipng_structp)ic->ds;

        PR_FREEIF(ipng_p->rgbrow);
        PR_FREEIF(ipng_p->alpharow);
        PR_FREEIF(ipng_p->interlacebuf);

        ipng_p->rgbrow = NULL;
        ipng_p->alpharow = NULL;
        ipng_p->interlacebuf = NULL;

        if((ipng_p->pngs_p)&&(ipng_p->info_p))
             png_destroy_read_struct(&ipng_p->pngs_p, &ipng_p->info_p, NULL);
        PR_FREEIF(ipng_p);
        ic->ds = NULL;
    }
    /*   il_abort( ic ); */
    return 0;
}


/*---------------------------------------------------------------------------
    Former contents of png_png.cpp, a.k.a. libpng's example.c (modified):
  ---------------------------------------------------------------------------*/

static void
info_callback(png_structp png_ptr, png_infop info_ptr)
{
/*  int number_passes;   NOT USED  */
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_type;
    int channels;
    double LUT_exponent, CRT_exponent = 2.2, display_exponent, aGamma;

    il_container *ic;
    ipng_structp ipng_p;
    NI_PixmapHeader *img_hdr;
    NI_PixmapHeader *src_hdr;

    png_bytep trans=NULL;
    int num_trans =0;

    /* always decode to 24-bit RGB or 32-bit RGBA  */
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
      &interlace_type, &compression_type, &filter_type);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    {
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
        png_set_expand(png_ptr);
    }
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);



    /* set up gamma correction for Mac, Unix and (Win32 and everything else)
     * using educated guesses for display-system exponents; do preferences
     * later */

#if defined(XP_MAC)
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
    if (interlace_type == PNG_INTERLACE_ADAM7)
        /* number_passes = */ png_set_interlace_handling(png_ptr);

    /* now all of those things we set above are used to update various struct
     * members and whatnot, after which we can get channels, rowbytes, etc. */
    png_read_update_info(png_ptr, info_ptr);
    channels = png_get_channels(png_ptr, info_ptr);
    PR_ASSERT(channels == 3 || channels == 4);

    /* set the ic values */

    ic = (il_container *)png_get_progressive_ptr(png_ptr);
    PR_ASSERT(ic != NULL);

    /*---------------------------------------------------------------*/
    /* copy PNG info into imagelib structs (formerly png_set_dims()) */
    /*---------------------------------------------------------------*/

    ipng_p = (ipng_structp)ic->ds;
    img_hdr = &ic->image->header;
    src_hdr = ic->src_header;

    ipng_p->width  = src_hdr->width = width;
    ipng_p->height = src_hdr->height = height;
    ipng_p->channels = channels;

    PR_ASSERT(ipng_p->rgbrow == NULL);
    PR_ASSERT(ipng_p->alpharow == NULL);

    ipng_p->rgbrow = (PRUint8 *)PR_MALLOC(channels*width);
 
    if (!ipng_p->rgbrow) {
        ILTRACE(0, ("il:png: MEM row"));
        ipng_p->state = PNG_ERROR;
        return;
    }

     if(interlace_type == PNG_INTERLACE_ADAM7) {
         ipng_p->interlacebuf = (PRUint8 *)PR_MALLOC(channels*width*height);
         if (!ipng_p->interlacebuf) {
             ILTRACE(0, ("il:png: MEM interlacebuf"));
             ipng_p->state = PNG_ERROR;
             return;
         }            
     }
     else
         ipng_p->interlacebuf = NULL;
 

    if (channels > 3) {
        ipng_p->alpharow = NULL;

#if defined(CAN_SUPPORT_8_BIT_MASK)
        if (color_type || PNG_COLOR_MASK_ALPHA) {
            /* check if alpha is coming from a tRNS chunk and is binary */
            if (num_trans) {
                ic->image->header.alpha_bits = 1;

                /* if it's not a indexed color image, tRNS means binary */
                if (color_type == PNG_COLOR_TYPE_PALETTE)
                    for (int i=0; i<num_trans; i++)
                        if ((trans[i] != 0) && (trans[i] != 255)) {
                            ic->image->header.alpha_bits = 8;
                            break;
                        }
             } else
                ic->image->header.alpha_bits = 8/*png_ptr->pixel_depth*/;   /* 8 */
        } else
#endif
                 ic->image->header.alpha_bits = 1;  
        ic->image->header.alpha_shift = 0;
        ic->image->header.is_interleaved_alpha = TRUE;
    }

    ic->imgdcb->ImgDCBImageSize();

    /* Note: all PNGs are decoded to RGB or RGBA and
       converted by imglib to appropriate pixel depth */

    ic->imgdcb->ImgDCBSetupColorspaceConverter();
    
    return;
}





static void
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
    il_container *ic = (il_container *)png_get_progressive_ptr(png_ptr);
    ipng_structp ipng_p = (ipng_structp)ic->ds;

    PR_ASSERT(ipng_p);

    int bw = ipng_p->channels * ipng_p->width;

    png_bytep line;
    if(ipng_p->interlacebuf)
    {
      line = ipng_p->interlacebuf+row_num*bw;
      png_progressive_combine_row(png_ptr, line, new_row);
    }
    else
      line = new_row;

    if (new_row) {
        /* first we copy the row data to a different buffer so that
         * il_emit_row() in scale.cpp doesn't mess up libpng's row buffer
         */
        memcpy(ipng_p->rgbrow, line, bw);
        ic->imgdcb->ImgDCBHaveRow(0, ipng_p->rgbrow, 0, ipng_p->width,
          row_num, 1, ilErase /* ilOverlay */, pass);
    }
}



static void
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
    il_container *ic = (il_container *)png_get_progressive_ptr(png_ptr);
        
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
