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

/* png_png.c - modified from example.c code */

#include "png.h"
#include "nsIImgDecoder.h" // include if_struct.h Needs to be first

#include "ipng.h"
#include "nsPNGDecoder.h"
#include "nsIImgDCallbk.h"


extern void png_set_dims(il_container *, png_structp);



void info_callback(png_structp png_ptr, png_infop info)
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

void row_callback( png_structp png_ptr,  png_bytep new_row,
   png_uint_32 row_num, int pass )
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

void end_callback(png_structp png_ptr, png_infop info)
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



