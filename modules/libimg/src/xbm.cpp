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
 *   xbm.c --- Decoding of X bit-map format images
 * $Id: xbm.cpp,v 3.1 1998/07/27 16:09:52 hardts%netscape.com Exp $
 */



#include "if.h"

#define CR '\015'
#define LF '\012'
#include "merrors.h"

#include "il.h"

PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C

typedef enum {
    xbm_gather,
    xbm_init,
    xbm_width,
    xbm_height,
    xbm_start_data,
    xbm_data,
    xbm_hex,
    xbm_done,
	xbm_oom,
    xbm_error
} xstate;

#define MAX_HOLD 512

typedef void(*il_xbm_converter)(void *, unsigned char, unsigned int);

typedef struct xbm_struct {
    xstate state;
    unsigned char hold[MAX_HOLD];
    intn gathern;				/* gather n chars */
    unsigned char gatherc;		/* gather until char c */
    intn gathered;				/* bytes accumulated */
    xstate gathers;				/* post-gather state */
    uint32 xpos, ypos;
    unsigned char *p;			/* raster position */
    unsigned char *m;           /* mask position (if mask is used) */
    il_xbm_converter converter; /* colorspace converter */
    uint8 bg_pixel;             /* destination image background pixel index */
    uint8 fg_pixel;             /* destination image foreground pixel index */
} xbm_struct;

#define MAX_LINE MAX_HOLD		/* XXX really bad xbms will hose us */


#define GETN(n,s) {xs->state=xbm_gather; xs->gathern=n; xs->gathers=s;}
#define GETC(c,s) {xs->state=xbm_gather; xs->gatherc=c; xs->gathers=s;}

char hex_table_initialized = FALSE;
static uint8 hex[256];

static void
il_init_hex_table(void)
{
    hex['0'] = 0; hex['1'] = 8;
    hex['2'] = 4; hex['3'] = 12;
    hex['4'] = 2; hex['5'] = 10;
    hex['6'] = 6; hex['7'] = 14;
    hex['8'] = 1; hex['9'] = 9;
    hex['A'] = hex['a'] = 5;
    hex['B'] = hex['b'] = 13;
    hex['C'] = hex['c'] = 3;
    hex['D'] = hex['d'] = 11;
    hex['E'] = hex['e'] = 7;
    hex['F'] = hex['f'] = 15;

    hex_table_initialized = TRUE;
}

int il_xbm_init(il_container *ic)
{
    xbm_struct *xs;
    NI_ColorSpace *src_color_space = ic->src_header->color_space;

    if (!hex_table_initialized)
		il_init_hex_table();

    xs = PR_NEWZAP (xbm_struct);
    if (xs) 
	{
		xs->state = xbm_init;
		xs->gathers = xbm_error;
		ic->ds = xs;
    }

    /* Initialize the container's source image header. */
    src_color_space->type = NI_GreyScale;
    src_color_space->pixmap_depth = 1;
    src_color_space->bit_alloc.index_depth = 1;
    src_color_space->cmap.num_colors = 2;

    return xs != 0;
}


static int
il_xbm_init_transparency(il_container *ic)
{
    IL_IRGB *src_trans_pixel = ic->src_header->transparent_pixel;
    IL_IRGB *img_trans_pixel;
   
    if (!src_trans_pixel) {
        src_trans_pixel = PR_NEWZAP(IL_IRGB);
        if (!src_trans_pixel)
            return FALSE;
        ic->src_header->transparent_pixel = src_trans_pixel;

        /* Initialize the destination image's transparent pixel. */
        il_init_image_transparent_pixel(ic);

        /* Set the source image's transparent pixel color to be the preferred
           transparency color of the destination image. */
        img_trans_pixel = ic->image->header.transparent_pixel;
        src_trans_pixel->red = img_trans_pixel->red;
        src_trans_pixel->green = img_trans_pixel->green;
        src_trans_pixel->blue = img_trans_pixel->blue;
        
        /* Set the source image's transparent pixel index. */
        src_trans_pixel->index = 1;
    }

    return TRUE;
}


/* Since the XBM decoder writes directly to the image bits, we need to provide
   our own color space conversion routines.  ConvertDefault is the preferred
   routine from an efficiency standpoint, but it requires the Front Ends to
   agree to receive 1-bit deep data.  The other routines are provided since
   the Front Ends can request the Image Library to decode to the display
   colorspace, which may not be 1-bit deep. */
static void
ConvertDefault(void *ds, unsigned char val, unsigned int last_bit_mask)
{
    xbm_struct *xs = (xbm_struct *)ds;

    *xs->p = val;
    xs->p++;
}

static void
ConvertBWToRGB8(void *ds, unsigned char val, unsigned int last_bit_mask)
{
    unsigned int bit_mask;
    xbm_struct *xs = (xbm_struct *)ds;
    uint8 *ptr = (uint8 *)xs->p;

    for (bit_mask = 128; bit_mask >= last_bit_mask; bit_mask >>= 1) {
        if (val & bit_mask)
            *ptr = 0;
        else
            *ptr = (uint8)~0;
        ptr++;
    }
    xs->p = (unsigned char *)ptr;
}

static void
ConvertBWToRGB16(void *ds, unsigned char val, unsigned int last_bit_mask)
{
    unsigned int bit_mask;
    xbm_struct *xs = (xbm_struct *)ds;
    uint16 *ptr = (uint16 *)xs->p;

    for (bit_mask = 128; bit_mask >= last_bit_mask; bit_mask >>= 1) {
        if (val & bit_mask)
            *ptr = 0;
        else
            *ptr = (uint16)~0;
        ptr++;
    }
    xs->p = (unsigned char *)ptr;
}

static void
ConvertBWToRGB24(void *ds, unsigned char val, unsigned int last_bit_mask)
{
    unsigned int bit_mask;
    xbm_struct *xs = (xbm_struct *)ds;
    uint8 *ptr = (uint8 *)xs->p;

    for (bit_mask = 128; bit_mask >= last_bit_mask; bit_mask >>= 1) {
        if (val & bit_mask) {
            *ptr++ = 0;
            *ptr++ = 0;
            *ptr++ = 0;
        }
        else {
            *ptr++ = (uint8)~0;
            *ptr++ = (uint8)~0;
            *ptr++ = (uint8)~0;
        }
    }
    xs->p = (unsigned char *)ptr;
}

static void
ConvertBWToRGB32(void *ds, unsigned char val, unsigned int last_bit_mask)
{
    unsigned int bit_mask;
    xbm_struct *xs = (xbm_struct *)ds;
    uint32 *ptr = (uint32 *)xs->p;

    for (bit_mask = 128; bit_mask >= last_bit_mask; bit_mask >>= 1) {
        if (val & bit_mask)
            *ptr = 0;
        else
            *ptr = (uint32)~0;
        ptr++;
    }
    xs->p = (unsigned char *)ptr;
}

static void
ConvertBWToCI(void *ds, unsigned char val, unsigned int last_bit_mask)
{
    unsigned int bit_mask;
    xbm_struct *xs = (xbm_struct *)ds;
    uint8 *ptr = (uint8 *)xs->p;
    uint8 bg_pixel = xs->bg_pixel;
    uint8 fg_pixel = xs->fg_pixel;

    for (bit_mask = 128; bit_mask >= last_bit_mask; bit_mask >>= 1) {
        if (val & bit_mask)
            *ptr = fg_pixel;
        else
            *ptr = bg_pixel;
        ptr++;
    }
    xs->p = (unsigned char *)ptr;
}


static void
il_xbm_setup_color_space_converter(il_container *ic)
{
    IL_ColorSpace *img_color_space = ic->image->header.color_space;
    xbm_struct *xs = (xbm_struct *)ic->ds;

    switch (img_color_space->type) {
    case NI_GreyScale:
        switch (img_color_space->pixmap_depth) {
        case 1:
            xs->converter = ConvertDefault;
            break;

        case 8:
            xs->converter = ConvertBWToRGB8;
            break;
            
        default:
            PR_ASSERT(0);
            break;
        }
        break;

    case NI_TrueColor:
        switch (img_color_space->pixmap_depth) {
        case 8:    
            xs->converter = ConvertBWToRGB8;   
            break;

        case 16:    
            xs->converter = ConvertBWToRGB16;   
            break;

        case 24:    
            xs->converter = ConvertBWToRGB24;   
            break;

        case 32:    
            xs->converter = ConvertBWToRGB32;   
            break;
            
        default:
            PR_ASSERT(0);
            break;
        }
        break;
        
    case NI_PseudoColor:
        xs->converter = ConvertBWToCI;   
        break;
        
    default:
        PR_ASSERT(0);
        break;
    }
}


static void
copyline(char *d, const unsigned char *s)
{
    int i=0;
    while( i++ < MAX_LINE && *s && *s != LF )
		*d++ = *s++;
    *d=0;
}

int il_xbm_write(il_container *ic, const unsigned char *buf, int32 len)
{
    int status, input_exhausted;
    xbm_struct *xs = (xbm_struct *)ic->ds;
    const unsigned char *q, *p=buf, *ep=buf+len;
    IL_Pixmap *image = ic->image;
    IL_Pixmap *mask = ic->mask;
    NI_PixmapHeader *img_header = &image->header;
    NI_PixmapHeader *mask_header = mask ? &mask->header : NULL;
    NI_PixmapHeader *src_header = ic->src_header;
    char lbuf[MAX_LINE+1];

    /* If this assert fires, chances are the netlib screwed up and
       continued to send data after the image stream was closed. */
    PR_ASSERT(xs);
    if (!xs) {
#ifdef DEBUG
        ILTRACE(1,("Netlib just took a shit on the imagelib\n"));
#endif
        return MK_IMAGE_LOSSAGE;
    }

    q = NULL;                   /* Initialize to shut up gcc warnings */
    input_exhausted = FALSE;
	
    while(!input_exhausted)
    {
		ILTRACE(9,("il:xbm: state %d len %d buf %u p %u ep %u",xs->state,len,buf,p,ep));

		switch(xs->state)
       	{
			case xbm_data:
		    	GETN(2,xbm_hex);	
				break;
	
			case xbm_hex:
				{
                    int num_pixels; /* Number of pixels remaining to be
                                       processed in current scanline. */
                    unsigned char val = (hex[q[1]]<<4) + hex[q[0]];

                    if (xs->xpos == 0) {
                        /* XXX -kevina Locking of the bits pointers is
                           currently done on per-scanline basis.  The XBM
                           decoder, however, does not process a full scanline
                           at a time, (only a single byte,) so if we return in
                           the middle of a scanline, the bits pointers will
                           remain locked until the next write call.  I would
                           like to eventually make the XBM decoder process a
                           full scanline at a time, since this would eliminate
                           the locking anomaly, and it would also permit the
                           colorspace converters to be modelled along the
                           lines of (and hence bundled with) the per-scanline
                           colorspace converters in the core image library. */
#ifdef STANDALONE_IMAGE_LIB
                        ic->img_cx->img_cb->ControlPixmapBits(ic->img_cx->dpy_cx,
                                                              ic->image, IL_LOCK_BITS);
                        ic->img_cx->img_cb->ControlPixmapBits(ic->img_cx->dpy_cx,
                                                              ic->mask, IL_LOCK_BITS);
#else
                        IMGCBIF_ControlPixmapBits(ic->img_cx->img_cb,
                                                  ic->img_cx->dpy_cx,
                                                  ic->image, IL_LOCK_BITS);
                        IMGCBIF_ControlPixmapBits(ic->img_cx->img_cb,
                                                  ic->img_cx->dpy_cx,
                                                  ic->mask, IL_LOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */

#ifdef _USD
                            xs->p = (unsigned char *)image->bits +
                                (img_header->height-xs->ypos-1) *
                                (img_header->widthBytes);
                            if (mask)
                                xs->m = (unsigned char *)mask->bits +
                                    (mask_header->height-xs->ypos-1) *
                                    (mask_header->widthBytes);
#else
                            xs->p = (unsigned char *)image->bits +
                                xs->ypos * (img_header->widthBytes);
                            if (mask)
                                xs->m = (unsigned char *)mask->bits +
                                    xs->ypos * (mask_header->widthBytes);
#endif
                    }

                    if (mask) {
                        *xs->m = val;
                        xs->m++;
                    }
                        
                    num_pixels = img_header->width - xs->xpos;
                    if (num_pixels <= 8) {
                        xs->converter((void*)xs, val, 1<<(8-num_pixels));

#ifdef STANDALONE_IMAGE_LIB
                        ic->img_cx->img_cb->ControlPixmapBits(ic->img_cx->dpy_cx,
                                                              ic->image, IL_UNLOCK_BITS);
                        ic->img_cx->img_cb->ControlPixmapBits(ic->img_cx->dpy_cx,
                                                              ic->mask, IL_UNLOCK_BITS);
#else
                        IMGCBIF_ControlPixmapBits(ic->img_cx->img_cb,
                                                  ic->img_cx->dpy_cx,
                                                  ic->image, IL_UNLOCK_BITS);
                        IMGCBIF_ControlPixmapBits(ic->img_cx->img_cb,
                                                  ic->img_cx->dpy_cx,
                                                  ic->mask, IL_UNLOCK_BITS);
#endif /* STANDALONE_IMAGE_LIB */
                        
						xs->xpos=0;
						xs->ypos++;
                        /* The image library does not scale XBMs, so if the
                           requested target height does not match the source
                           height, we stop when the smaller of the two is
                           reached. */
                        if (xs->ypos == src_header->height ||
                            xs->ypos == img_header->height) {
                            xs->state = xbm_done;
                        }
                        else {
                            GETC('x',xbm_data);
                        }
                    }
                    else {
                        xs->converter((void*)xs, val, 1);
                        xs->xpos+=8;
                        GETC('x',xbm_data);
                    }
				}
				break;
		
			case xbm_init:
				GETC(LF,xbm_width);
				break;

			case xbm_width:
				{
					copyline(lbuf, q);
					if(sscanf(lbuf, "#define %*s %d",
                              (int *)&ic->src_header->width)==1) 
					{
						GETC(LF,xbm_height);
					}
					else
					{
                        /* Accept any of CR/LF, CR, or LF.
                           Allow multiple instances of each. */
						GETC(LF,xbm_width);
					}
				}
				break;
		
			case xbm_height:
				{
					copyline(lbuf, q);
					if(sscanf(lbuf, "#define %*s %d",
                              (int *)&ic->src_header->height)==1)
					{
						IL_RGB* map;

                        if (!il_xbm_init_transparency(ic))
                            return MK_OUT_OF_MEMORY; 
						
						if ((status = il_size(ic)) < 0)
						{
							if (status == MK_OUT_OF_MEMORY)
                                xs->state = xbm_oom;
                            else
                                xs->state = xbm_error;
							break;
						}

                        /* Determine the background and foreground pixel
                           indices of the destination image if this is a
                           PseudoColor display.  Note: it is assumed that
                           the index of the black pixel in the IL_ColorMap
                           is 0. */
                        if (img_header->color_space->type == NI_PseudoColor) {
                            xs->bg_pixel =
                                img_header->transparent_pixel->index;
                            xs->fg_pixel = img_header->color_space->cmap.index ?
                                img_header->color_space->cmap.index[0] :
							    il_identity_index_map[0];
                        }

                        if (ic->mask) {
                            mask = ic->mask;
                            mask_header = &mask->header;
                        }

						if((map = (IL_RGB*)PR_Calloc(2, sizeof(IL_RGB)))!=0)
                            {
							src_header->color_space->cmap.map = map;
							map++; /* first index is black */
							map->red = src_header->transparent_pixel->red;
							map->green = src_header->transparent_pixel->green;
							map->blue = src_header->transparent_pixel->blue;
						}
						GETC('{',xbm_start_data);
                        il_xbm_setup_color_space_converter(ic);
					}
					else
					{
                        /* Accept any of CR/LF, CR, or LF.
                           Allow multiple instances of each. */
						GETC(LF,xbm_height);
					}
				}
				break;

			case xbm_start_data:
				GETC('x',xbm_data);
				break;
	
			case xbm_gather:	
				{
					if(xs->gatherc)
					{
						const unsigned char *s;
                        /* We may need to look ahead one character, so don't point
                           at the last character in the buffer. */
						for(s = p; s < ep; s++)
						{
                            if (xs->gatherc == LF) 
                            {
                                if ((s[0] == LF) || (s[0] == CR))
                                {
                                    xs->gatherc = 0;
                                    break;
                                }
                            }
                            else if (s[0] == xs->gatherc)
                            {
                                xs->gatherc = 0;
                                break;
                            }
						}
                            
						if(xs->gatherc)
						{
							if((xs->gathered+ep-p) > MAX_HOLD)
							{
								/* we will never find it */
								xs->state = xbm_error;
							}
							else
							{
								XP_MEMCPY(xs->hold+xs->gathered, p, ep-p);
								xs->gathered += ep-p;
								input_exhausted = TRUE;
							}
						}
						else
						{		/* found it */
							if(xs->gathered)
							{
								XP_MEMCPY(xs->hold+xs->gathered, p, s-p+1);
								q = xs->hold;
							}
							else
							{
								q = p;
							}
							p = s+1;
							xs->gathered=0;
							xs->state=xs->gathers;
						}
					}

					if(xs->gathern)
					{
						if( (ep - p) >= xs->gathern)
						{		/* there is enough */
							if(xs->gathered)
							{	/* finish a prior gather */
								XP_MEMCPY(xs->hold+xs->gathered, p, xs->gathern);
								q = xs->hold;
								xs->gathered=0;
							}
							else
							{
								q = p;
							}
							p += xs->gathern;
							xs->gathern=0;
							xs->state=xs->gathers;
						}
						else
						{ 
							XP_MEMCPY(xs->hold+xs->gathered, p, ep-p);
							xs->gathered += ep-p;
							xs->gathern -= ep-p;
							input_exhausted = TRUE;
						}
					}
				}
				break;

			case xbm_done:
                il_partial(ic, 0, img_header->height, 0);
                return 0;

			case xbm_oom: 
				ILTRACE(1,("il:xbm: reached oom state"));
				return MK_OUT_OF_MEMORY;

			case xbm_error: 
				ILTRACE(2,("il:xbm: reached error state"));
				return MK_IMAGE_LOSSAGE;

			default: 
				ILTRACE(0,("il:xbm: unknown state %d", xs->state));
				PR_ASSERT(0);
				break;
			}
    }
    return 0;
}

void
il_xbm_abort(il_container *ic)
{
    if (ic->ds) 
	{
		xbm_struct *xs = (xbm_struct*) ic->ds;
		PR_FREEIF(xs);
		ic->ds = 0;
    }
}

void
il_xbm_complete(il_container *ic)
{
    il_xbm_abort(ic);
    il_image_complete(ic);
    il_frame_complete_notify(ic);
}
