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

/* -*- Mode: C; tab-width: 4 -*-
   scale.c --- Controls rendering of scan lines to screen, including scaling
               and transparency
*/


#include "if.h"                 /* Image library internal declarations */
#include "il.h"                 /* Image library external API */
#include "il_strm.h"            /* For image types. */

#include "nsVoidArray.h"        
#include "nsITimer.h"
#include "nsITimerCallback.h"



/* Approximate size of pixel data chunks sent to the FE for display */
#ifdef XP_OS2
#define OUTPUT_CHUNK_SIZE        30000
#else
#define OUTPUT_CHUNK_SIZE        15000
#endif

/* Delay from decode to display of first scanline, in milliseconds. */
#define ROW_OUTPUT_INITIAL_DELAY    50

/* Delays between subsequent sets of scanlines */
#define ROW_OUTPUT_DELAY           300

/* for png */
typedef struct _IL_IRGBGA {
     uint8 index;
    uint8 red, green, blue, gray, alpha;
} IL_IRGBGA;


static nsVoidArray *gTimeouts = NULL;

static void
il_timeout_callback(void *closure)
{
    int delay;
	il_container *ic = (il_container *)closure;
    NI_PixmapHeader *img_header = &ic->image->header;

    ic->row_output_timeout = NULL;
    if (ic->state == IC_ABORT_PENDING)
        return;

    /*
     * Don't schedule any more timeouts once we start decoding the
     * second image in a multipart image sequence.  Instead display
     * will take place when the entire image is decoded.
     */
    if (ic->multi &&
        ((uint32)img_header->height * img_header->width < 100000)) {
        return;
    }
    
    il_flush_image_data(ic);

    delay = (ic->pass > 1) ? 2 * ROW_OUTPUT_DELAY : ROW_OUTPUT_DELAY;

    ic->row_output_timeout = IL_SetTimeout(il_timeout_callback, ic, delay);
}

/*-----------------------------------------------------------------------------
 * Display a specified number of rows of pixels for the purpose of
 * progressive image display.  The data is accumulated without forwarding
 * it to the front-end for display until either the row-output timeout
 * fires or the image is fully decoded.
 *---------------------------------------------------------------------------*/
void
il_partial(
    il_container *ic,   /* The image container */
    int row,            /* Starting row; zero is top row of image */
    int row_count,      /* Number of rows to output, including starting row */
    int pass)           /* Zero, unless  interlaced GIF,
                           in which case ranges 1-4, or progressive JPEG,
                           in which case ranges from 1-n. */
{	
	NI_PixmapHeader *img_header = &ic->image->header;

    if (!ic->new_data_for_fe) {
        ic->update_start_row = row;
        ic->update_end_row = row + row_count - 1;
        ic->new_data_for_fe = TRUE;
    } else {
        if (row < ic->update_start_row)
            ic->update_start_row = row;
        
        if ((row + row_count - 1) > ic->update_end_row)
            ic->update_end_row = row + row_count - 1;
    }

    ic->pass = pass;

    if (ic->img_cx->progressive_display)
    {
#ifdef XP_WIN
        /* The last pass of an image is displayed with less delay. */
        if (!ic->multi && (pass == IL_FINAL_PASS))
#else
            /* The first and last pass of an image are displayed with less
               delay. */
        if (!ic->multi && ((pass <= 1) || (pass == IL_FINAL_PASS)))
#endif
            {
                int num_rows = ic->update_end_row - ic->update_start_row + 1;
                if (num_rows * img_header->widthBytes > OUTPUT_CHUNK_SIZE)
                    il_flush_image_data(ic);
            }
        
        
        /*
         * Don't schedule any more timeouts once we start decoding the
         * second image in a multipart image sequence.  Instead display
         * will take place when the entire image is decoded.
        */
        if (ic->multi &&
            ((uint32)img_header->height * img_header->width < 100000)) {
            return;
        }
    
        if (!ic->row_output_timeout){
            /* Set a timer that will actually display the image data. */
            ic->row_output_timeout = IL_SetTimeout(il_timeout_callback, ic,
                                                   ROW_OUTPUT_INITIAL_DELAY);
        }
    }
}


/*-----------------------------------------------------------------------------
 * Force the front-end to to display any lines in the image bitmap
 * that have been decoded, but haven't yet been sent to the screen.
 * (Progressively displayed images are normally displayed several
 * lines at a time for efficiency.  This routine flushes out the last
 * few undisplayed lines in the image.)
 *---------------------------------------------------------------------------*/
void
il_flush_image_data(il_container *ic)
{
    IL_GroupContext *img_cx = ic->img_cx;
    IL_Pixmap *image = ic->image;
    IL_Pixmap *mask = ic->mask;
    NI_PixmapHeader *img_header = &image->header;
    NI_PixmapHeader *mask_header = mask ? &mask->header : NULL;
    int row, start_row, end_row, row_interval;

    /* If we never called il_size(), we have no data for the FE.  There
       may also be no new data if a previous flush has occurred. */
    if (!image->bits || !ic->new_data_for_fe)
        return;

    start_row = ic->update_start_row;
    end_row = ic->update_end_row;
    row_interval = (2 * OUTPUT_CHUNK_SIZE) / img_header->widthBytes;
    row = start_row;

#ifdef XP_UNIX
    /* If the amount of image data becomes really large, break it
     * up into chunks to BLT out to the screen.  Otherwise, there
     * can be a noticeable delay as the FE processes a large image.
     * (In the case of the XFE, it can take a long time to send
     * it to the server.)
     */
    for (;row < (end_row - row_interval); row += row_interval) {
#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->UpdatePixmap(img_cx->dpy_cx, image, 0, row,
                                     img_header->width, row_interval);
#else
        IMGCBIF_UpdatePixmap(img_cx->img_cb, img_cx->dpy_cx, image, 0, row,
                           img_header->width, row_interval);
#endif
        if (mask) {
#ifdef STANDALONE_IMAGE_LIB
            img_cx->img_cb->UpdatePixmap(img_cx->dpy_cx, mask, 0, row,
                                         mask_header->width, row_interval);
#else
            IMGCBIF_UpdatePixmap(img_cx->img_cb, img_cx->dpy_cx, mask, 0, row,
                               mask_header->width, row_interval);
#endif /* STANDALONE_IMAGE_LIB */
		}
    }
#endif /* XP_UNIX */
   
    /* Draw whatever is leftover after sending the chunks */
#ifdef STANDALONE_IMAGE_LIB
    img_cx->img_cb->UpdatePixmap(img_cx->dpy_cx, image, 
                                 0, row, img_header->width, end_row - row + 1);
#else
    IMGCBIF_UpdatePixmap(img_cx->img_cb, img_cx->dpy_cx, image, 0, row,
                       img_header->width, end_row - row + 1);
#endif /* STANDALONE_IMAGE_LIB */

    if (mask) {
#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->UpdatePixmap(img_cx->dpy_cx, mask, 0, row,
                                     mask_header->width, end_row - row + 1);
#else
        IMGCBIF_UpdatePixmap(img_cx->img_cb, img_cx->dpy_cx, mask, 0, row,
                           mask_header->width, end_row - row + 1);
#endif /* STANDALONE_IMAGE_LIB */
    }

    /* Update the displayable area of the pixmap. */
    ic->displayable_rect.x_origin = 0;
    ic->displayable_rect.y_origin = 0;
    ic->displayable_rect.width = img_header->width;
    ic->displayable_rect.height = MAX(ic->displayable_rect.height,
                                      end_row + 1);

    /* Notify observers that the image pixmap has been updated. */
    il_pixmap_update_notify(ic);

    /* Notify observers of image progress. */
    il_progress_notify(ic);

    ic->new_data_for_fe = FALSE;
    ic->update_end_row = ic->update_start_row = 0;
}


/* Copy a packed RGB triple */
#define COPY_RGB(src, dest)                                 \
    {dest[0] = src[0]; dest[1] = src[1]; dest[2] = src[2];}

/*-----------------------------------------------------------------------------
 * Scale a row of packed RGB pixels using the Bresenham algorithm.
 * Output is also packed RGB pixels.
 *---------------------------------------------------------------------------*/
static void
il_scale_RGB_row(
    uint8 XP_HUGE *src, /* Source row of packed RGB pixels */
    int src_len,        /* Number of pixels in source row  */
    uint8 *dest,        /* Destination, packed RGB pixels */
    int dest_len)       /* Length of target row, in pixels */
{
    uint8 *dest_end = dest + (3 * dest_len);
    int n = 0;
    
    PR_ASSERT(dest);
    PR_ASSERT(src_len != dest_len);

    /* Two cases */

    /* Scaling down ? ... */
    if (src_len > dest_len)
    {
        while (dest < dest_end) {
            COPY_RGB(src, dest);
            dest += 3;
            n += src_len;
            while (n >= dest_len) {
                src += 3;
                n -= dest_len;
            }
        }
    }
    else
    /* Scaling up. */
    {
        while (dest < dest_end) {
            n += dest_len;
            while (n >= src_len) {
                COPY_RGB(src, dest);
                dest += 3;
                n -= src_len;
            }
            src += 3;
        }
    }
}

#ifdef M12N
/*-----------------------------------------------------------------------------
 * Scale a row of single-byte pixels using the Bresenham algorithm.
 * Output is also single-byte pixels.
 *---------------------------------------------------------------------------*/
static void
il_scale_CI_row(
    uint8 XP_HUGE *src, /* Source row of packed RGB pixels */
    int src_len,        /* Number of pixels in source row  */
    uint8 *dest,        /* Destination, packed RGB pixels */
    int dest_len,       /* Length of target row, in pixels */
    uint8* indirect_map,/* image-to-FE color mapping */
    int transparent_pixel_color)
{
    int src_pixel, mapped_src_pixel;
    uint8 *dest_end = dest + dest_len;
    int n = 0;
    
    PR_ASSERT(dest);
    PR_ASSERT(src_len != dest_len);

    /* Two cases */

    /* Scaling down ? ... */
    if (src_len > dest_len)
    {
        while (dest < dest_end) {
            if (*src != transparent_pixel_color)
                *dest = indirect_map[*src];
            dest ++;
            n += src_len;
            while (n >= dest_len) {
                src ++;
                n -= dest_len;
            }
        }
    }
    else
    /* Scaling up. */
    {
        while (dest < dest_end) {
            n += dest_len;
            src_pixel = *src;
            mapped_src_pixel = indirect_map[src_pixel];
            while (n >= src_len) {
                if (src_pixel != transparent_pixel_color)
                    *dest = mapped_src_pixel;
                dest ++;
                n -= src_len;
            }
            src++;
        }
    }
}
#endif /* M12N */

/* Convert row coordinate from image space to display space. */
#define SCALE_YCOORD(ih, sh, y)                        \
    ((int)((uint32)(y) * (ih)->height / (sh)->height))

#define SCALE_XCOORD(ih, sh, x)                        \
    ((int)((uint32)(x) * (ih)->width / (sh)->width))


/*-----------------------------------------------------------------------------
 * 24 bit transparency:
 * Create an alpha mask bitmap.  Perform horizontal scaling if
 * requested using a Bresenham algorithm. Accumulate the mask in
 * 32-bit chunks for efficiency.
 *---------------------------------------------------------------------------*/
static void
il_alpha_mask(
    int HasAlphaCh,            /* flag */
    uint8 *src,                 /* RGBa, input data */
    int src_len,                /* Number of pixels in source row */
    int x_offset,               /* Destination offset from left edge */
    uint8 XP_HUGE *maskp,       /* Output pointer, left-justified bitmask */
    int mask_len,               /* Number of pixels in output row */
    il_draw_mode draw_mode)     /* ilOverlay or ilErase */
{
    int not_transparent,n =0;
    int output_bits_remaining = mask_len;

    uint32 bgmask32 = 0;        /* 32-bit temporary mask accumulators */
    uint32 fgmask32 = 0;

    int mask_bit;               /* next bit to write in setmask32 */
    
    uint32 *m = ((uint32*)maskp) + (x_offset >> 5);
    mask_bit = ~x_offset & 0x1f;

    PR_ASSERT(mask_len);

    /* Handle case in which we have a mask for a non-transparent
       image.  This can happen when we have a LOSRC that is a transparent
       GIF and a SRC that is a JPEG.  For now, we avoid crashing.  Later
       we should fix that case so it does the right thing and gets rid
       of the mask. */
    if (!src)
        return;

/* Add a bit to the row of mask bits.  Flush accumulator to memory if full. */
#define SHIFT_IMAGE_MASK(not_transparent_flag)								  \
    {																		  \
        fgmask32 |=  ((uint32)not_transparent_flag    ) << M32(mask_bit);     \
        bgmask32 |=  ((uint32)not_transparent_flag ^ 1) << M32(mask_bit);     \
																			  \
        /* Filled up 32-bit mask word.  Write it to memory. */				  \
        if (mask_bit-- == 0) {                                                \
            uint32 mtmp = *m;                                                 \
            mtmp |= fgmask32;                                                 \
         	if (draw_mode == ilErase)                                         \
                mtmp &= ~bgmask32;                                            \
            *m++ = mtmp;                                                      \
            mask_bit = 31;													  \
            bgmask32 = 0;                                                     \
            fgmask32 = 0;                                                     \
        }																	  \
        output_bits_remaining--;                                              \
    }
 

    /* Two cases */
    /* Scaling down ? (or source and dest same size) ... */   

    if (src_len >= mask_len)
    {      
        while (output_bits_remaining ) {
            not_transparent = (*(src+3) > 0x60 );
            SHIFT_IMAGE_MASK(not_transparent);
            n += src_len;

            while ( n >= mask_len){
                src += 4;
                n -= mask_len;
            }
        }
    }
    else  
    /* Scaling up */
    {
        while (output_bits_remaining) {
            n += mask_len;
            not_transparent = (*src != 0);

            while (n >= src_len) {
                SHIFT_IMAGE_MASK(not_transparent);
                n -= src_len;
            }
            src++;
        }

    }
    
    /* End of scan line. Write out any remaining mask bits. */ 
    if (mask_bit < 31) {
        uint32 mtmp = *m;
        mtmp |= fgmask32;
        if (draw_mode == ilErase)
            mtmp &= ~bgmask32; 
        *m = mtmp; 
    }
  
}


/*-----------------------------------------------------------------------------
 * Create a transparency mask bitmap.  Perform horizontal scaling if
 * requested using a Bresenham algorithm. Accumulate the mask in
 * 32-bit chunks for efficiency.
 *---------------------------------------------------------------------------*/
static void
il_generate_scaled_transparency_mask(
    IL_IRGB *transparent_pixel,  /* The transparent pixel */
    uint8 *src,                 /* Row of pixels, 8-bit pseudocolor data */
    int src_len,                /* Number of pixels in source row */
    int x_offset,               /* Destination offset from left edge */
    uint8 XP_HUGE *maskp,       /* Output pointer, left-justified bitmask */
    int mask_len,               /* Number of pixels in output row */
    il_draw_mode draw_mode)     /* ilOverlay or ilErase */
{
    int not_transparent, n = 0;
    int src_trans_pixel_index =
        transparent_pixel ? transparent_pixel->index : -1;
    int output_bits_remaining = mask_len;

    uint32 bgmask32 = 0;        /* 32-bit temporary mask accumulators */
    uint32 fgmask32 = 0;

    int mask_bit;               /* next bit to write in setmask32 */

    uint32 *m = ((uint32*)maskp) + (x_offset >> 5);
    mask_bit = ~x_offset & 0x1f;

    PR_ASSERT(mask_len);

    /* Handle case in which we have a mask for a non-transparent
       image.  This can happen when we have a LOSRC that is a transparent
       GIF and a SRC that is a JPEG.  For now, we avoid crashing.  Later
       we should fix that case so it does the right thing and gets rid
       of the mask. */
    if (!src)
        return;

    /* Two cases */
    /* Scaling down ? (or source and dest same size) ... */
    if (src_len >= mask_len)
    {
        while (output_bits_remaining) {
            not_transparent = (*src != src_trans_pixel_index);
 
            SHIFT_IMAGE_MASK(not_transparent);
            n += src_len;

            while (n >= mask_len) {
                src++;
                n -= mask_len;
            }
        }
    }
    else
    /* Scaling up */
    {
        while (output_bits_remaining) {
            n += mask_len;
            not_transparent = (*src != src_trans_pixel_index);

            while (n >= src_len) {
                SHIFT_IMAGE_MASK(not_transparent);
                n -= src_len;
            }
            src++;
        }
    }
    
    /* End of scan line. Write out any remaining mask bits. */ 
    if (mask_bit < 31) {
        uint32 mtmp = *m;
        mtmp |= fgmask32;
        if (draw_mode == ilErase)
            mtmp &= ~bgmask32; 
        *m = mtmp; 
    }

#undef SHIFT_IMAGE_MASK    
}


/*-----------------------------------------------------------------------------
 * When color quantization (possibly accompanied by dithering) takes
 * place, the background pixels in a transparent image that overlays a
 * solid-color background, e.g. <BODY BGCOLOR=#c5c5c5>, will get
 * mapped to a color in the color-cube.  The real background color,
 * however, may not be one of these colors reserved for images.  This
 * routine serves to return transparent pixels to their background
 * color.  This routine must performing scaling because the source
 * pixels are in the image space and the target pixels are in the
 * display space.
 *---------------------------------------------------------------------------*/
static void
il_reset_background_pixels(
    il_container *ic,    /* The image container */
    uint8 *src,          /* Row of pixels, 8-bit pseudocolor data */
    int src_len,         /* Number of pixels in row */
    uint8 XP_HUGE *dest, /* Output pointer, 8-bit pseudocolor data */
    int dest_len)        /* Width of output pixel row */
{
    int is_transparent, n = 0;
    uint8 XP_HUGE *dest_end = dest + dest_len;
    NI_PixmapHeader *img_header = &ic->image->header;
    int src_trans_pixel_index = ic->src_header->transparent_pixel->index;
    int img_trans_pixel_index = img_header->transparent_pixel->index;
    int dpy_trans_pixel_index = img_header->color_space->cmap.index ?
        img_header->color_space->cmap.index[img_trans_pixel_index] :
	    il_identity_index_map[img_trans_pixel_index];

    /* Two cases */

    /* Scaling down ? (or not scaling ?) ... */
    if (src_len >= dest_len) {
        while (dest < dest_end) {
            is_transparent = (*src == src_trans_pixel_index);
            if (is_transparent)
                *dest = dpy_trans_pixel_index;
            dest++;
            n += src_len;

            while (n >= dest_len) {
                src++;
                n -= dest_len;
            }
        }
    } else {
    /* Scaling up */
        while (dest < dest_end) {
            n += dest_len;
            is_transparent = (*src++ == src_trans_pixel_index);
            
            if (is_transparent)
                while (n >= src_len) {
                    *dest++ = dpy_trans_pixel_index;
                    n -= src_len;
                }
            else
                while (n >= src_len) {
                    dest++;
                    n -= src_len;
                }
        }
    }
}

static void
il_generate_byte_mask(
    il_container *ic,    /* The image container */
    uint8 *src,          /* Row of pixels, 8-bit pseudocolor data */
    int src_len,         /* Number of pixels in row */
    uint8 *dest,         /* Output pointer, 8-bit pseudocolor data */
    int dest_len)        /* Width of output pixel row */
{
    int is_transparent, n = 0;
    uint8 XP_HUGE *dest_end = dest + dest_len;
    int src_trans_pixel_index = ic->src_header->transparent_pixel->index;

    /* Two cases */

    /* Scaling down ? (or not scaling ?) ... */
    if (src_len >= dest_len) {
        while (dest < dest_end) {
            is_transparent = (*src == src_trans_pixel_index);
            *dest = is_transparent - 1;
            dest++;
            n += src_len;

            while (n >= dest_len) {
                src++;
                n -= dest_len;
            }
        }
    } else {
    /* Scaling up */
        while (dest < dest_end) {
            n += dest_len;
            is_transparent = (*src++ == src_trans_pixel_index);
            
            if (is_transparent)
                while (n >= src_len) {
                    *dest++ = 0;
                    n -= src_len;
                }
            else
                while (n >= src_len) {
                    *dest++ = (uint8)-1;
                    n -= src_len;
                }
        }
    }
}

static void
il_overlay(uint8 *src, uint8 *dest, uint8 *byte_mask, int num_cols,
           int bytes_per_pixel)
{
    int i, col;
#if 0
    uint8 *s = src;
    uint8 *s_end = src + (num_cols * bytes_per_pixel);
#endif
    for (col = num_cols; col > 0; col--) {
        if (*byte_mask++) {
            for (i = bytes_per_pixel-1; i >= 0; i--) {
                dest[i] = src[i];
            }
        }
        dest += bytes_per_pixel;
        src += bytes_per_pixel;
    }
}

static uint8 il_tmpbuf[MAX_IMAGE_WIDTH];
    
/*-----------------------------------------------------------------------------
 *  Emit a complete row of pixel data into the image.  This routine
 *  provides any necessary conversion to the display depth, optional dithering
 *  for pseudocolor displays, scaling and transparency, including mask
 *  generation, if necessary.  If sufficient data is accumulated, the screen
 *  image is updated, as well.
 *---------------------------------------------------------------------------*/
void
il_emit_row(
    il_container *ic,   /* The image container */
    uint8 *cbuf,        /* Color index data source, or NULL if source
                           is RGB data */
    uint8 *rgbbuf,      /* Packed RGBa data or RGBa workspace if <cbuf> != NULL */
    int x_offset,       /* First column to write data into */
    int len,            /* Width of source image, in pixels */
    int row,            /* Starting row of image */
    int dup_row_count,          /* Number of times to duplicate row */
    il_draw_mode draw_mode,     /* ilOverlay or ilErase */
    int pass)           /* Zero, unless  interlaced GIF,
                           in which case ranges 1-4, or progressive JPEG,
                           in which case ranges from 1-n. */
{
    IL_GroupContext *img_cx = ic->img_cx;
	IL_Pixmap *image = ic->image;
    IL_Pixmap *mask = ic->mask;
    NI_PixmapHeader *src_header = ic->src_header;
    NI_PixmapHeader *img_header = &image->header;
    NI_PixmapHeader *mask_header = 0;
    NI_ColorSpace *src_color_space = src_header->color_space;
    NI_ColorSpace *img_color_space = img_header->color_space;
	uint8 XP_HUGE *out;
    uint8 XP_HUGE *dp;
    uint8 XP_HUGE *mp;
	uint8 XP_HUGE *maskp = NULL;
    uint8 *byte_mask = NULL;
	uint8 XP_HUGE *srcbuf = rgbbuf;
    uint8 *p = cbuf;
    uint8 *pl = cbuf+len;
	int drow_start, drow_end, row_count, color_index, dup, do_dither;
    int dcolumn_start, dcolumn_end, column_count, offset, src_len, dest_len;
   
	PR_ASSERT(row >= 0);

	if(row >= src_header->height) {
		ILTRACE(2,("il: ignoring extra row (%d)", row));
		return;
	}

	/* Set first and last destination row in the image.  Assume no scaling. */
	drow_start = row;
    drow_end = row + dup_row_count - 1;
    dcolumn_start = x_offset;
    dcolumn_end = x_offset + len - 1;

    /* If scaling, convert vertical image coordinates to display space. */
    if (img_header->height != src_header->height) {
        int d = drow_start;
        int next_drow_start = SCALE_YCOORD(img_header, src_header, drow_end+1);
        drow_start = SCALE_YCOORD(img_header, src_header, drow_start);

        /*
         * Don't emit a row of pixels that will be overwritten later.
         * (as may happen during when images are being reduced vertically).
         */
        if (drow_start == next_drow_start) {
            /*
             * Except that the bottom line of pixels can never be
             * overwritten by a subsequent line.
             */
            if (d != (src_header->height - 1))
                return;
            else
                drow_end = drow_start;
        } else {
            drow_end = next_drow_start - 1;
            if (drow_end >= img_header->height)
                drow_end = img_header->height - 1;
        }
    }

    /* If scaling, convert horizontal image coordinates to display space. */
    if (img_header->width != src_header->width) {
        int d = dcolumn_start;
        int next_dcolumn_start = SCALE_XCOORD(img_header, src_header,
                                              dcolumn_end+1);
        dcolumn_start = SCALE_XCOORD(img_header, src_header, dcolumn_start);

        /*
         * Don't emit a column of pixels that will be overwritten later.
         * (as may happen during when images are being reduced vertically).
         */
        if (dcolumn_start == next_dcolumn_start) {
            /*
             * Except that the right column of pixels can never be
             * overwritten by a subsequent column.
             */
            if (d != (src_header->width - 1))
                return;
            else
                dcolumn_end = dcolumn_start;
        } else {
            dcolumn_end = next_dcolumn_start - 1;
            if (dcolumn_end >= img_header->width)
                dcolumn_end = img_header->width - 1;
        }
    }

    /* Number of pixel rows and columns to emit into framebuffer */
    row_count = drow_end - drow_start + 1;
    column_count = dcolumn_end - dcolumn_start + 1;

    /* If a transparent image appears over a background image ... */
    if (mask) {
        mask_header = &mask->header;

        /* Bug, we retain the mask from a transparent
           LOSRC GIF when the SRC is a JPEG. */
        /* PR_ASSERT(cbuf); */
        
#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, mask,
                                          IL_LOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, mask,
                                IL_LOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */

#ifdef _USD
		maskp = (uint8 XP_HUGE *)mask->bits + 
            (mask_header->height - drow_start - 1) * mask_header->widthBytes;
#else
		maskp = (uint8 XP_HUGE *)mask->bits +
            drow_start * mask_header->widthBytes;
#endif
       
        if(!ic->image->header.is_interleaved_alpha){
            il_generate_scaled_transparency_mask(src_header->transparent_pixel,
                                             cbuf, (int)len,
                                             dcolumn_start,
                                             maskp, column_count,
                                             draw_mode);                
        }else{ /* is alpha */

            uint8 *tmpbuf;
            int i;
            il_alpha_mask(1,rgbbuf, (int)len, dcolumn_start, 
                maskp, column_count,draw_mode);                

            tmpbuf = rgbbuf;
            for(i=0; i<column_count; i++){
                *rgbbuf++ = *tmpbuf++;
                *rgbbuf++ = *tmpbuf++;
                *rgbbuf++ = *tmpbuf++;
                tmpbuf++;  /* strip off alpha channel */
            }
      
        }
#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, mask,
                                          IL_UNLOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, mask,
                                IL_UNLOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */
    }

	if (!ic->converter) {

#ifdef M12N
        int i;
        int src_trans_pixel_index;
        uint8 XP_HUGE * dest;
        uint8 *indirect_map = src_color_space->cmap.index;

		if (indirect_map == NULL) {
		    indirect_map = il_identity_index_map;
		}

        if ((draw_mode == ilErase) || !src_header->transparent_pixel)
            src_trans_pixel_index = -1; /* no transparency */
        else
            src_trans_pixel_index = src_header->transparent_pixel->index;

#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, image,
                                          IL_LOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, image,
                                IL_LOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */

		/* No converter, image is already rendered in pseudocolor. */
#ifdef _USD
		out = (uint8 XP_HUGE *)image->bits +
            (img_header->height - drow_start - 1) * img_header->widthBytes;
#else
		out = (uint8 XP_HUGE *)image->bits +
            drow_start * img_header->widthBytes;
#endif

        dest = out + dcolumn_start;
        
        /* If horizontal scaling ... */
        if (len != column_count) {
            il_scale_CI_row(cbuf, len, dest, column_count,
                            indirect_map, src_trans_pixel_index);
        } else {

            /* Convert to FE's palette indices */
            for (i = 0; i < len; i++)
                if (cbuf[i] != src_trans_pixel_index)
                    dest[i] = indirect_map[cbuf[i]];
        }

#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, image,
                                          IL_UNLOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, image,
                                IL_UNLOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */
#endif /* M12N */

	} else {

        /* Generate the output row in RGB space, regardless of screen depth. */
		if (cbuf) {
			uint8 *r = rgbbuf;
			IL_RGB *map = src_color_space->cmap.map, *entry;

			if (!src_header->transparent_pixel) {
				/* Simple case: no transparency */
				while (p < pl) {
                    color_index = *p++;
					entry = map + color_index;
					r[0] = entry->red;
					r[1] = entry->green;
					r[2] = entry->blue;
                    r += 3 ;
				}
			} else {
				/*
                 * There are two kinds of transparency, depending on whether
                 * the image is overlaying:
                 *   1) a solid color background, or
                 *   2) another image
                 *
                 * The first case is easy.  We just substitute the background
                 * color for all the transparent pixels in the image.  No mask
                 * is necessary.  The second case requires that we generate a
                 * bit mask (see the code above).  It also seems to require that
                 * all the transparent pixels in the image be set to black.
                 * XXX - Why ?  Is this some platform-specific thing ? - fur
                 */
                int background_r, background_g, background_b;
                IL_IRGB *src_trans_pixel = src_header->transparent_pixel;
				int src_trans_pixel_index = src_trans_pixel->index;

                background_r = background_g = background_b = 0;
				if (!ic->mask) {
                    /* Solid background color */
					background_r = src_trans_pixel->red;
					background_g = src_trans_pixel->green;
					background_b = src_trans_pixel->blue;
                }
                
                /* Remap transparent pixels */
                while (p < pl) {
                    color_index = *p++;
                    if (color_index == src_trans_pixel_index) {
                        r[0] = background_r;
                        r[1] = background_g;
                        r[2] = background_b;
                        r += 3;
                    } else {
                        entry = map + color_index;
                        r[0] = entry->red;
                        r[1] = entry->green;
                        r[2] = entry->blue;
                        r += 3;
                    }
                }
			}
		}


		/* Now we are in RGB space. */

		/* Simple anamorphic scaling (in RGB space for now) */
        src_len = len;
        dest_len = column_count;
		if (src_len != dest_len) {
			uint8 XP_HUGE *src = rgbbuf;
			uint8 *dest = ic->scalerow;
			srcbuf = dest;

			/* Scale the pixel data (mask data already scaled) */
            il_scale_RGB_row(src, src_len, dest, dest_len);
		}

#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, image,
                                          IL_LOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, image,
                                IL_LOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */

#ifdef _USD
		out = (uint8 XP_HUGE *)image->bits +
            (img_header->height-drow_start-1) * (uint32)img_header->widthBytes;
#else
		out = (uint8 XP_HUGE *)image->bits +
            drow_start * img_header->widthBytes;
#endif

        if (src_header->transparent_pixel && (draw_mode == ilOverlay))
        {
            if( cbuf ){
                il_generate_byte_mask(ic, cbuf, len, il_tmpbuf, column_count);
                byte_mask = il_tmpbuf;
            }
        }

        
        /*
         * Convert RGB to display depth.  If display is pseudocolor, this may
         * also color-quantize and dither.
         */
		(*ic->converter)(ic, byte_mask, srcbuf, dcolumn_start,
                         column_count, out);

#ifdef STANDALONE_IMAGE_LIB
        img_cx->img_cb->ControlPixmapBits(img_cx->dpy_cx, image,
                                          IL_UNLOCK_BITS);
#else
        IMGCBIF_ControlPixmapBits(img_cx->img_cb, img_cx->dpy_cx, image,
                                IL_UNLOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */

        /*
         * Have to reset transparent pixels to background color
         * because color quantization may have mutated them.
         */
		if (src_header->transparent_pixel &&
            (img_color_space->type == NI_PseudoColor) &&
            !mask && (draw_mode == ilErase))
            il_reset_background_pixels(ic, cbuf, len,
                                       out + dcolumn_start, column_count);
	}

	/*
     * We now have one row of pixels and, if required for transparency, a row
     * of mask data.  If the pixels in this row of the image cover span more
     * than one pixel vertically when displayed, the row needs to be
     * replicated in the framebuffer.  (This replication is necessary when
     * displaying interlaced GIFs and/or vertical scaling of any image type.)
     * Actually, pixel rows are not simply copied: Dithering may need to be
     * applied on a line-by-line basis.
     */
    dp = out;
    mp = maskp;
    dup = row_count - 1;
    offset = dcolumn_start * (img_color_space->pixmap_depth / 8);

#ifndef M12N                    /* Clean this up */
    if (ic->image->pixmap_depth == 1)
        do_dither = TRUE;
    else
        do_dither = ic->converter && (row_count <= 4) &&
            ((ic->dither_mode == IL_Dither) || (ic->type == IL_JPEG));
#else
    do_dither = (ic->dither_mode == IL_Dither);
    if ((ic->type == IL_GIF) && (!ic->converter || (row_count > 4)))
        do_dither = FALSE;
   
#endif /* M12N */   

    while (dup--) {
#ifdef _USD
        dp -= img_header->widthBytes;
        if (mask)
            mp -= mask_header->widthBytes;
#else
        dp += img_header->widthBytes;
        if (mask)
            mp += mask_header->widthBytes;
#endif
        /* Is dithering being done (either mono or pseudocolor) ... ? */
        if (do_dither) {
            
            /* Dither / color-quantize */
            (*ic->converter)(ic, byte_mask, srcbuf, dcolumn_start,
                             column_count, dp);

            /*
             * Have to reset transparent pixels to background color
             * because color quantization may have mutated them.
             */
            if (img_header->transparent_pixel &&
                (img_color_space->type == NI_PseudoColor) &&
                !mask && (draw_mode == ilErase))
                il_reset_background_pixels(ic, cbuf, len, dp + dcolumn_start,
                                           column_count);
        } else {
            /* If no dithering, each row of pixels is exactly the same. */
            if (byte_mask)
                il_overlay(out + offset, dp + offset, byte_mask, column_count,
                           (img_color_space->pixmap_depth/8));
            else
                XP_MEMCPY(dp + offset, out + offset,
                          (img_color_space->pixmap_depth/8) * column_count);
        }

        /* Duplicate the mask also. */  
        if (maskp) {
               XP_MEMCPY(mp, maskp, mask_header->widthBytes);
        }
    }


    /* If enough rows accumulated, send to the front-end for display. */
    il_partial(ic, drow_start, row_count, pass);
}


