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

/* -*- Mode: C; tab-width: 4 -*-
   color.c --- Responsible for conversion from image depth to screen depth.
               Includes dithering for B&W displays, but not dithering
               for PseudoColor displays which can be found in dither.c.
               
   $Id: color.cpp,v 3.16 2000/07/10 07:13:27 cls%seawood.org Exp $
*/


#include "if.h"
#include "nsQuickSort.h"
#include "xp_str.h" //for XP_RANDOM

#ifdef PROFILE
#pragma profile on
#endif

PRUint8 il_identity_index_map[] = { 0, 1, 2, 3, 4, 5, 6, 7, 
8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 
104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 
119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 
134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 
149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 
164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 
179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 
194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 
209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 
224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 
239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 
254, 255 };

static void
ConvertRGBToCI(il_container *ic,
               const PRUint8 *mask,
               const PRUint8 *sp,
               int x_offset,
               int num,
               void XP_HUGE *vout)
{
    PRUint8 red, green, blue, map_index;
    PRUint8 XP_HUGE *out = (PRUint8 XP_HUGE *) vout + x_offset;
    const PRUint8 *end = sp + 3*num;
    IL_ColorMap *cmap = &ic->image->header.color_space->cmap;
    PRUint8 *lookup_table = (PRUint8 *)cmap->table;
    PRUint8 *index_map = cmap->index;

	if (index_map == NULL) 
	{
	    index_map = il_identity_index_map;
	}

    if (!mask)
    {
        while (sp < end) 
        {
            red = sp[0];
            green = sp[1];
            blue = sp[2];
            map_index = COLORMAP_INDEX(lookup_table, red, green, blue);
            *out = index_map[map_index];
            out++;
            sp += 3;
        }
    }
    else
    {
        while (sp < end) 
        {
            if (*mask++) {
                red = sp[0];
                green = sp[1];
                blue = sp[2];
                map_index = COLORMAP_INDEX(lookup_table, red, green, blue);
                *out = index_map[map_index];
            }
            out++;
            sp += 3;
        }
    }
}

static void
DitherConvertRGBToCI(il_container *ic,
                     const PRUint8 *mask,
                     const PRUint8 *sp,
                     int x_offset,
                     int num,
                     void XP_HUGE *vout)
{
    PRUint8 XP_HUGE *out = (PRUint8 XP_HUGE *) vout + x_offset;
    const PRUint8 XP_HUGE *end = out + num;
    PRUint8 *index_map = ic->image->header.color_space->cmap.index;

	if (index_map == NULL) 
	{
	    index_map = il_identity_index_map;
	}

    il_quantize_fs_dither(ic, mask, sp, x_offset, (PRUint8 XP_HUGE *) vout, num);
    if (mask) {
        while (out < end) {
            if (*mask++)
                *out = index_map[*out];
            out++;
        }
    } else {
        while (out < end) {
            *out = index_map[*out];
            out++;
        }
    }
}

struct fs_data {
    long* err1;
    long* err2;
    long* err3;
    PRUint8 *greypixels;
    PRUint8 *bwpixels;
	int width;
    int direction;
    long threshval, sum;
};


static struct fs_data *
init_fs_dither(il_container *ic)
{
	struct fs_data *fs;
	fs = PR_NEWZAP(struct fs_data);
    if (! fs)
        return NULL;

	fs->width = ic->image->header.width;
	fs->direction = 1;
	fs->err1 = (long*) PR_Calloc(fs->width+2, sizeof(long));
	fs->err2 = (long*) PR_Calloc(fs->width+2, sizeof(long));
	fs->greypixels = (PRUint8 *)PR_Calloc(fs->width+7, 1);
	fs->bwpixels = (PRUint8 *)PR_Calloc(fs->width+7, 1);
#ifdef XP_UNIX
    {
        int i;
        /* XXX should not be unix only */
        for(i=0; i<fs->width+2; i++)
            {
                fs->err1[i] = (XP_RANDOM() % 1024 - 512)/4;
            }
    }
#endif
	fs->threshval = 512;
	ic->quantize = (void *)fs;
	return fs;
}

static void
ConvertRGBToBW(il_container *ic,
               const PRUint8 *mask,
               const PRUint8 *sp,
               int x_offset,
               int num,
               void XP_HUGE *vout)
{
    PRUint32 fgmask32, bgmask32;
    PRUint32 *m;
    int mask_bit;
	struct fs_data *fs = (struct fs_data *)ic->quantize;
    PRUint8 XP_HUGE *out = (PRUint8 XP_HUGE *)vout;
	PRUint8 *gp, *bp;
	int col, limitcol;
	long grey;
	long sum;

	if(!fs)
		fs = init_fs_dither(ic);

    /* Silently fail if memory exhausted */
    if (! fs)
        return;

	gp = fs->greypixels;
	for(col=0; col<fs->width; col++)
	{
        /* CCIR 709 */
		PRUint8 r = *sp++;
		PRUint8 g = *sp++;
		PRUint8 b = *sp++;
        grey = ((PRUintn)(0.299 * 4096) * r +
                (PRUintn)(0.587 * 4096) * g +
                (PRUintn)(0.114 * 4096) * b) / 4096;

		*gp++ = (PRUint8)grey;
	}

#if 0
	/* thresholding */
	gp = fs->greypixels;
	bp = fs->bwpixels;
	for(col=0; col<fs->width; col++)
	{
		*bp++ = (*gp++<128);
	}

#else

	for(col=0; col<fs->width+2; col++)
	{
		fs->err2[col] =0;
	}

	if (fs->direction)
	{
		col = 0;
		limitcol = fs->width;
		gp = fs->greypixels;
		bp = fs->bwpixels;
	}
	else
	{
		col = fs->width - 1;
		limitcol = -1;
		gp = &(fs->greypixels[col]);
		bp = &(fs->bwpixels[col]);
	}

	do
	{
		sum = ( (long) *gp * 1024 ) / 256 + fs->err1[col + 1];
        if ( sum >= 512)
        {
            *bp = 0;
            sum = sum - 1024;
        }
        else
            *bp = 1;
    
		if ( fs->direction )
		{
			fs->err1[col + 2] += ( sum * 7 ) / 16;
			fs->err2[col    ] += ( sum * 3 ) / 16;
			fs->err2[col + 1] += ( sum * 5 ) / 16;
			fs->err2[col + 2] += ( sum     ) / 16;
			col++; gp++; bp++;
		}
		else
		{
			fs->err1[col    ] += ( sum * 7 ) / 16;
			fs->err2[col + 2] += ( sum * 3 ) / 16;
			fs->err2[col + 1] += ( sum * 5 ) / 16;
			fs->err2[col    ] += ( sum     ) / 16;
			col--; gp--; bp--;
		}
	}
	while ( col != limitcol );

	fs->err3 = fs->err1;
	fs->err1 = fs->err2;
	fs->err2 = fs->err3;

	fs->direction = !fs->direction;

#endif
    
	bp = fs->bwpixels;
    bgmask32 = 0;        /* 32-bit temporary mask accumulators */
    fgmask32 = 0;

    m = ((PRUint32*)out) + (x_offset >> 5);
    mask_bit = ~x_offset & 0x1f; /* next bit to write in 32-bit mask */

/* Add a bit to the row of mask bits.  Flush accumulator to memory if full. */
#define SHIFT_IMAGE_MASK(opaqueness, pixel)					  		          \
    {																		  \
        fgmask32 |=  ((PRUint32)pixel & opaqueness) << M32(mask_bit);           \
        bgmask32 |=  ((PRUint32)((pixel ^ 1) & opaqueness)) << M32(mask_bit);   \
																			  \
        /* Filled up 32-bit mask word.  Write it to memory. */				  \
        if (mask_bit-- == 0) {                                                \
            PRUint32 mtmp = *m;                                                 \
            mtmp |= fgmask32;                                                 \
            mtmp &= ~bgmask32;                                                \
            *m++ = mtmp;                                                      \
            mask_bit = 31;													  \
            bgmask32 = 0;                                                     \
            fgmask32 = 0;                                                     \
        }																	  \
    }

    for(col=0; col < num; col++)
    {
        int opaqueness = 1;
        int pixel = *bp++;

        if (mask)
            opaqueness = *mask++;
        SHIFT_IMAGE_MASK(opaqueness, pixel);
    }

    /* End of scan line. Write out any remaining mask bits. */ 
    if (mask_bit < 31) {
        PRUint32 mtmp = *m;
        mtmp |= fgmask32;
        mtmp &= ~bgmask32; 
        *m = mtmp; 
    }
}

static void
ConvertRGBToGrey8(il_container *ic,
                  const PRUint8 *mask,
                  const PRUint8 *sp,
                  int x_offset,
                  int num,
                  void XP_HUGE *vout)
{
    PRUintn r, g, b;
    PRUint8 XP_HUGE *out = (PRUint8 XP_HUGE *)vout + x_offset;
    const PRUint8 *end = sp + num*3;
    PRUint32 grey;

    if (!mask)
    {
        while (sp < end)
        {
            /* CCIR 709 */
            r = sp[0];
            g = sp[1];
            b = sp[2];
            grey = ((PRUintn)(0.299 * 4096) * r +
                    (PRUintn)(0.587 * 4096) * g +
                    (PRUintn)(0.114 * 4096) * b) / 4096;

            *out = (PRUint8)grey;
            out++;
            sp += 3;
        }
    }
    else
    {
        while (sp < end)
        {
            if (*mask++)
            {
                
                /* CCIR 709 */
                r = sp[0];
                g = sp[1];
                b = sp[2];
                grey = ((PRUintn)(0.299 * 4096) * r +
                        (PRUintn)(0.587 * 4096) * g +
                        (PRUintn)(0.114 * 4096) * b) / 4096;
                *out = (PRUint8)grey;
            }
            out++;
            sp += 3;
        }
    }
}

static void
ConvertRGBToRGB8(il_container *ic,
                 const PRUint8 *mask,
                 const PRUint8 *sp,
                 int x_offset,
                 int num,
                 void XP_HUGE *vout)
{
    PRUintn r, g, b, pixel;
    PRUint8 XP_HUGE *out = (PRUint8 XP_HUGE *) vout + x_offset;
    const PRUint8 *end = sp + num*3;
    il_ColorSpaceData *private_data =
        (il_ColorSpaceData *)ic->image->header.color_space->private_data;
    PRUint8 *rm = (PRUint8*)private_data->r8torgbn;
    PRUint8 *gm = (PRUint8*)private_data->g8torgbn;
    PRUint8 *bm = (PRUint8*)private_data->b8torgbn;
    if (!mask)
    {
        while (sp < end) 
        {
            r = sp[0];
            g = sp[1];
            b = sp[2];
            pixel = rm[r] + gm[g] + bm[b];
            *out = pixel;
            out++;
            sp+=3;
        }
    }
    else
    {
        while (sp < end) 
        {
            if (*mask++) {
                r = sp[0];
                g = sp[1];
                b = sp[2];
                pixel = rm[r] + gm[g] + bm[b];
                *out = pixel;
            }
            out++;
            sp += 3;
        }
    }
}

static void
ConvertRGBToRGB16(il_container *ic,
                  const PRUint8 *mask,
                  const PRUint8 *sp,
                  int x_offset,
                  int num,
                  void XP_HUGE *vout)
{
    PRUintn r, g, b, pixel;
    PRUint16 XP_HUGE *out = (PRUint16 XP_HUGE *) vout + x_offset;
    const PRUint8 *end = sp + num*3;
    il_ColorSpaceData *private_data =
        (il_ColorSpaceData *)ic->image->header.color_space->private_data;
    PRUint16 *rm = (PRUint16*)private_data->r8torgbn;
    PRUint16 *gm = (PRUint16*)private_data->g8torgbn;
    PRUint16 *bm = (PRUint16*)private_data->b8torgbn;

    if (!mask)
    {
        while (sp < end) 
        {
            r = sp[0];
            g = sp[1];
            b = sp[2];
            pixel = rm[r] + gm[g] + bm[b];
            *out = pixel;
            out++;
            sp+=3;
        }
    }
    else
    {
        while (sp < end) 
        {
            if (*mask++) {
                r = sp[0];
                g = sp[1];
                b = sp[2];
                pixel = rm[r] + gm[g] + bm[b];
                *out = pixel;
            }
            out++;
            sp += 3;
        }
    }
}

static void
ConvertRGBToRGB24(il_container *ic,
                  const PRUint8 *mask,
                  const PRUint8 *sp,
                  int x_offset,
                  int num,
                  void XP_HUGE *vout)
{
    PRUint8 XP_HUGE *out = (PRUint8 XP_HUGE *) vout + (3 * x_offset);
    const PRUint8 *end = sp + num*3;
	/* XXX this is a hack because it ignores the shifts */

    // The XP_UNIX define down here is needed because the unix gtk
    // image munging code expects the image data to be in RGB format.
    //
    // The conversion below is obviously a waste of time on unix and 
    // probably mac.
    //
    // Unfortunately, the alternative seems to require significant IMGLIB
    // work.  Simply seeting to ic->converter to NULL for the 24 bit case
    // did not work as expected.
    //
    // The correct fix might be to not do any conversion if the image data
    // is already in the format expected on the nsIImage end.
    //
    // -ramiro
    if (!mask)
    {
        while (sp < end) {
#if !defined(XP_UNIX)  || defined(NTO)
            out[2] = sp[0];
            out[1] = sp[1];
            out[0] = sp[2];
#else
            out[0] = sp[0];
            out[1] = sp[1];
            out[2] = sp[2];
#endif
            out+=3;
            sp+=3;
        }
    } else {
        while (sp < end) {
            if (*mask++)
            {
#if !defined(XP_UNIX) || defined(NTO)
            out[2] = sp[0];
            out[1] = sp[1];
            out[0] = sp[2];
#else
            out[0] = sp[0];
            out[1] = sp[1];
            out[2] = sp[2];
#endif
            }
            out+=3;
            sp+=3;
        }
    }
}

static void
ConvertRGBToRGB32(il_container *ic,
                  const PRUint8 *mask,
                  const PRUint8 *sp,
                  int x_offset,
                  int num,
                  void XP_HUGE *vout)
{
    PRUintn r, g, b, pixel;
    PRUint32 XP_HUGE *out = (PRUint32 XP_HUGE *) vout + x_offset;
    const PRUint8 *end = sp + num*3;
    il_ColorSpaceData *private_data =
        (il_ColorSpaceData *)ic->image->header.color_space->private_data;
    PRUint32 *rm = (PRUint32*)private_data->r8torgbn;
    PRUint32 *gm = (PRUint32*)private_data->g8torgbn;
    PRUint32 *bm = (PRUint32*)private_data->b8torgbn;

    if (!mask)
    {
        while (sp < end) 
        {
            r = sp[0];
            g = sp[1];
            b = sp[2];
            pixel = rm[r] + gm[g] + bm[b];
            *out = pixel;
            out++;
            sp+=3;
        }
    }
    else
    {
        while (sp < end) 
        {
            if (*mask++) {
                r = sp[0];
                g = sp[1];
                b = sp[2];
                pixel = rm[r] + gm[g] + bm[b];
                *out = pixel;
            }
            out++;
            sp += 3;
        }
    }
}

/* Sorting predicate for NS_QuickSort() */
int PR_CALLBACK compare_PRUint32(const void *a, const void *b, void *unused)
{
    PRUint32 a1 = *(PRUint32*)a;
    PRUint32 b1 = *(PRUint32*)b;
    
    return (a1 < b1) ? -1 : ((a1 > b1) ? 1 : 0);
}

/* Count colors in the source image's color map.  Remove duplicate colors. */
static void
unique_map_colors(NI_ColorMap *cmap)
{
    PRUintn i;
    PRUint32 ind[256];
    int32 num_colors = cmap->num_colors;
    IL_RGB *map = cmap->map;
    PRUintn max_colors;
    PRUintn unique_colors = 1;
    
    /* A -ve value for cmap->num_colors indicates that the colors may be
       non-unique. */
    if (num_colors > 0)         /* Colors are unique. */
        return;

    max_colors = -num_colors;

    PR_ASSERT(max_colors <= 256);
    PR_ASSERT(map);

    /* Convert RGB values into indices. */
    for (i = 0; i < max_colors; i++) {
        ind[i] = (map[i].red << 16) + (map[i].green << 8) + map[i].blue;
    }

    /* Sort by color, so identical colors will be grouped together. */
    NS_QuickSort(ind, max_colors, sizeof(*ind), compare_PRUint32, NULL);

    /* Look for adjacent colors with different values */
    for (i = 0; i < max_colors-1; i++)
        if (ind[i] != ind[i + 1])
            unique_colors++;

    cmap->num_colors = unique_colors;
}
        

/* Create RGB24 to RGBN depth conversion tables for a TrueColor destination
   image colorspace.  N is the pixmap_depth of the colorspace, which is the
   sum of the bits assigned to the three color channels, plus any additional
   allowance that might be necessary, e.g. for an alpha channel, or for
   alignment.  Possible pixmap depths are N = 8, 16, or 32 bits. */
static int32
il_init_rgb_depth_tables(IL_ColorSpace *color_space)
{
    register PRUint8 red_bits, green_bits, blue_bits;
    register PRUint8 red_shift, green_shift, blue_shift;
    int32 j, k;
    int32 pixmap_depth = color_space->pixmap_depth;
    IL_RGBBits *rgb = &color_space->bit_alloc.rgb;
    il_ColorSpaceData *private_data =
        (il_ColorSpaceData *)color_space->private_data;

    /* If depth conversion tables exist already, we have nothing to do. */
    if (private_data->r8torgbn &&
        private_data->g8torgbn &&
        private_data->b8torgbn)
        return TRUE;

    red_bits = rgb->red_bits;
    red_shift = rgb->red_shift;
    green_bits = rgb->green_bits;
    green_shift = rgb->green_shift;
    blue_bits = rgb->blue_bits;
    blue_shift = rgb->blue_shift;
    
    switch(pixmap_depth) {
    case 8:                     /* 8-bit TrueColor. */
    {
        PRUint8 *tmp_map;         /* Array type corresponds to pixmap depth. */

        private_data->r8torgbn = PR_MALLOC(256);
        private_data->g8torgbn = PR_MALLOC(256);
        private_data->b8torgbn = PR_MALLOC(256);
        
        if (!(private_data->r8torgbn &&
              private_data->g8torgbn &&
              private_data->b8torgbn)) {
            ILTRACE(0,("il: MEM il_init_rgb_tables"));
            return FALSE;
        }

        /* XXXM12N These could be optimized. */
        tmp_map = (PRUint8*)private_data->r8torgbn;
		for (j = 0; j < (1 << red_bits); j++) 
			for (k = 0; k < (1 << (8 - red_bits)); k++) 
				*tmp_map++ = (PRUint8)(j << red_shift);

		tmp_map = (PRUint8*)private_data->g8torgbn;
		for (j = 0; j < (1 << green_bits); j++)
			for (k = 0; k < (1 << (8 - green_bits)); k++)
				*tmp_map++ = (PRUint8)(j << green_shift);

		tmp_map = (PRUint8*)private_data->b8torgbn;
		for (j = 0; j < (1 << blue_bits); j++)
			for (k = 0; k < (1 << (8 - blue_bits)); k++)
				*tmp_map++ = (PRUint8)(j << blue_shift);

        break;
	} 

    case 16:                    /* Typically 15 or 16-bit TrueColor. */
    {
        PRBool win95_rounding;  /* True if Win95 color quatization bug is
                                   present. */
        PRUint16 *tmp_map;        /* Array type corresponds to pixmap depth. */
        
        private_data->r8torgbn = PR_MALLOC(sizeof(PRUint16) * 256);
        private_data->g8torgbn = PR_MALLOC(sizeof(PRUint16) * 256);
        private_data->b8torgbn = PR_MALLOC(sizeof(PRUint16) * 256);
        
        if (!(private_data->r8torgbn &&
              private_data->g8torgbn &&
              private_data->b8torgbn)) {
            ILTRACE(0,("il: MEM il_init_rgb_tables"));
            return FALSE;
        }

/* Compensate for Win95's sometimes-weird color quantization. */
        win95_rounding = (PRBool)((color_space->os_flags & WIN95_ROUNDING) != 0);

#define _W1(v, b)         ((v) - (1 << (7 - (b))))
#define WACKY(v, b)       ((_W1(v, b) < 0) ? 0 : _W1(v, b))

#define ROUND(v, b)       (win95_rounding ? WACKY(v, b) : (v))

        /* XXXM12N These could be optimized. */
        tmp_map = (PRUint16*)private_data->r8torgbn;
        for (j = 0; j < 256; j++)
            *tmp_map++ = (PRUint16)(ROUND(j, red_bits) >> (8 - red_bits) <<
                                  red_shift);
            
        tmp_map = (PRUint16*)private_data->g8torgbn;
        for (j = 0; j < 256; j++)
            *tmp_map++ = (PRUint16)(ROUND(j, green_bits) >> (8 - green_bits) <<
                                  green_shift);

        tmp_map = (PRUint16*)private_data->b8torgbn;
        for (j = 0; j < 256; j++)
            *tmp_map++ = (PRUint16)(ROUND(j, blue_bits) >> (8 - blue_bits) <<
                                  blue_shift);

#undef _W1
#undef WACKY
#undef ROUND
        break;
    }
    
    case 32:                    /* Typically 24-bit TrueColor with an 8-bit
                                   (leading) pad. */
    {
        PRUint32 *tmp_map;        /* Array type corresponds to pixmap depth. */

        private_data->r8torgbn = PR_MALLOC(sizeof(PRUint32) * 256);
        private_data->g8torgbn = PR_MALLOC(sizeof(PRUint32) * 256);
        private_data->b8torgbn = PR_MALLOC(sizeof(PRUint32) * 256);
        
        if (!(private_data->r8torgbn &&
              private_data->g8torgbn &&
              private_data->b8torgbn)) {
            ILTRACE(0,("il: MEM il_init_rgb_tables"));
            return FALSE;
        }

        tmp_map = (PRUint32*)private_data->r8torgbn;
        for (j = 0; j < (1 << red_bits); j++)
            *tmp_map++ = j << red_shift;

		tmp_map = (PRUint32*)private_data->g8torgbn;
        for (j = 0; j < (1 << green_bits); j++)
            *tmp_map++ = j << green_shift;

		tmp_map = (PRUint32*)private_data->b8torgbn;
        for (j = 0; j < (1 << blue_bits); j++)
            *tmp_map++ = j << blue_shift;

        break;
    }
    

    default:
        ILTRACE(0,("il: unsupported truecolor pixmap_depth: %d bpp",
                   pixmap_depth));
        PR_ASSERT(0);
    }

    return TRUE;
}


/*
 * A greater number of colors than this causes dithering to be used
 * rather than closest-color rendering if dither_mode == ilAuto.
 */
#define AUTO_DITHER_COLOR_THRESHOLD    16


/* XXX - need to do this for every client of a container*/
PRBool
il_setup_color_space_converter(il_container *ic)
{
    PRUint8 conversion_type;
    IL_GroupContext *img_cx = ic->img_cx;
    IL_DitherMode dither_mode = img_cx->dither_mode;
    il_converter converter = NULL;
    NI_ColorSpace *img_color_space = ic->image->header.color_space;
    NI_ColorSpace *src_color_space = ic->src_header->color_space;

    /* Make sure that the num_colors field of the source image's colormap
       represents the number of unique colors.  In the case of GIFs, for
       example, colormap sizes are actually rounded up to a power of two. */
    if (src_color_space->type == NI_PseudoColor)
        unique_map_colors(&src_color_space->cmap);


#ifndef M12N                    /* XXXM12N Fix me. */
#ifdef XP_MAC
    if ((ic->type == IL_GIF) && (ic->image->depth != 1)) {
        ic->converter = NULL;

        /* Allow for custom color palettes */
        il_set_color_palette(ic->cx, ic);
        
        return;
    }
#endif
#endif /* M12N */

    conversion_type = (src_color_space->type << 3) | img_color_space->type;
    switch (conversion_type) {

        /* Always dither when converting from truecolor to pseudocolor. */
    case IL_TrueToPseudo:
        dither_mode = IL_Dither;
        /* Build the range limit table if it doesn't exist. */
        if (!il_setup_quantize())
            return PR_FALSE;
        converter = DitherConvertRGBToCI;
        break;
        
        /* These conversions are accomplished by first converting to RGB,
           followed by closest color matching or dithering. */
    case IL_GreyToPseudo:
    case IL_PseudoToPseudo:

        if (src_color_space == img_color_space) {
            ic->converter = NULL;
            return PR_TRUE;
        } else
        /* Resolve dither_mode if necessary. */
        if (dither_mode == IL_Auto) {
            int num_colors = src_color_space->cmap.num_colors;

            /* Use a simple heuristic to decide whether or not we dither. */
            if ((num_colors <= AUTO_DITHER_COLOR_THRESHOLD) &&
                (num_colors <= (img_color_space->cmap.num_colors / 2))) {
                dither_mode = IL_ClosestColor;

                ILTRACE(1,("Dithering turned off; Image has %d colors: %s",
                           num_colors, ic->url ? ic->url_address : ""));
            }
            else {
                dither_mode = IL_Dither;
            }
        }
        switch (dither_mode) {
        case IL_ClosestColor:
            converter = ConvertRGBToCI;
            break;
            
        case IL_Dither:
            /* Build the range limit table if it doesn't exist. */
            if (!il_setup_quantize())
                return PR_FALSE;
            converter = DitherConvertRGBToCI;
            break;
	default:
	    break;
        }
        break;
        
        /* These conversions are accomplished by first converting to RGB24,
           followed by conversion to the actual image pixmap depth. */
    case IL_TrueToTrue:
    case IL_PseudoToTrue:
    case IL_GreyToTrue:
        switch (img_color_space->pixmap_depth) {
        case 8:
            /* Build the depth conversion tables if they do not exist. */
            if (!il_init_rgb_depth_tables(img_color_space))
                return PR_FALSE;
            converter = ConvertRGBToRGB8;
            break;

        case 16:
            /* Build the depth conversion tables if they do not exist. */
            if (!il_init_rgb_depth_tables(img_color_space))
                return PR_FALSE;
            converter = ConvertRGBToRGB16;
            break;

        case 24:
            converter = ConvertRGBToRGB24;
            break;
            
        case 32:
            /* Build the depth conversion tables if they do not exist. */
            if (!il_init_rgb_depth_tables(img_color_space))
                return PR_FALSE;
            converter = ConvertRGBToRGB32;
            break;

        default:
            PR_ASSERT(0);
            break;
        }
        break;
        
        /* These conversions are accomplished by first converting to RGB24,
           followed by conversion to the actual image pixmap depth. */
    case IL_TrueToGrey:
    case IL_PseudoToGrey:
    case IL_GreyToGrey:
        switch (img_color_space->pixmap_depth) {
        case 1:
            dither_mode = IL_Dither;
            converter = ConvertRGBToBW;
            break;
            
        case 8:
            converter = ConvertRGBToGrey8;
            break;
        }
        break;

    default:
        converter = NULL;
        PR_ASSERT(0);
        break;
    }
    
    ic->dither_mode = dither_mode;
    ic->converter = converter;

#ifndef M12N                    /* XXXM12N Fix me. */
    /* Allow for custom color palettes */
    if (img_header->cs_type == NI_Pseudocolor)
        il_set_color_palette(ic->cx, ic);
#endif /* M12N */

    return PR_TRUE;
}

#ifdef PROFILE
#pragma profile off
#endif
