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
#include "xp.h"
*/
#include "if.h"
#include "il.h"

#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"
#include "jpegint.h"


/* BEGIN code adapted from jpeg library */

typedef INT32 FSERROR;		/* 16 bits should be enough */
typedef int LOCFSERROR;		/* use 'int' for calculation temps */

typedef FSERROR FAR *FSERRPTR;	/* pointer to error array (in FAR storage!) */

typedef struct my_cquantize_str {
	/* Variables for Floyd-Steinberg dithering */
	FSERRPTR fserrors[3];		/* accumulated errors */
	boolean on_odd_row;		/* flag to remember which row we are on */
} my_cquantize;

typedef my_cquantize *my_cquantize_ptr;

static JSAMPLE *the_sample_range_limit = NULL;

/* allocate and fill in the sample_range_limit table */
int
il_setup_quantize(void)
{
	JSAMPLE *table;
	int i;

	if(the_sample_range_limit)
		return TRUE;

	/* lost for ever */
	table = (JSAMPLE *)XP_ALLOC((5 * (MAXJSAMPLE+1) + CENTERJSAMPLE) * SIZEOF(JSAMPLE));
	if (!table) 
	{
		XP_TRACE(("il: range limit table lossage"));
		return FALSE;
	}

	table += (MAXJSAMPLE+1);	/* allow negative subscripts of simple table */
	the_sample_range_limit = table;

	/* First segment of "simple" table: limit[x] = 0 for x < 0 */
	XP_BZERO(table - (MAXJSAMPLE+1), (MAXJSAMPLE+1) * SIZEOF(JSAMPLE));

	/* Main part of "simple" table: limit[x] = x */
	for (i = 0; i <= MAXJSAMPLE; i++)
		table[i] = (JSAMPLE) i;

	table += CENTERJSAMPLE;	/* Point to where post-IDCT table starts */

	/* End of simple table, rest of first half of post-IDCT table */
	for (i = CENTERJSAMPLE; i < 2*(MAXJSAMPLE+1); i++)
		table[i] = MAXJSAMPLE;

	/* Second half of post-IDCT table */
	XP_BZERO(table + (2 * (MAXJSAMPLE+1)),
			(2 * (MAXJSAMPLE+1) - CENTERJSAMPLE) * SIZEOF(JSAMPLE));
	XP_MEMCPY(table + (4 * (MAXJSAMPLE+1) - CENTERJSAMPLE),
			the_sample_range_limit, CENTERJSAMPLE * SIZEOF(JSAMPLE));

	return TRUE;
}


/* Must remain idempotent.  Also used to make sure that the ic->quantize has the same 
colorSpace info as the rest of ic. */
int
il_init_quantize(il_container *ic)
{
    size_t arraysize;
    int i, j;
    my_cquantize_ptr cquantize;

    if (ic->quantize)
        il_free_quantize(ic);

    ic->quantize = XP_NEW_ZAP(my_cquantize);
    if (!ic->quantize) 
	{
	loser:
		ILTRACE(0,("il: MEM il_init_quantize"));
		return FALSE;
    }

    cquantize = (my_cquantize_ptr) ic->quantize;
    arraysize = (size_t) ((ic->image->header.width + 2) * SIZEOF(FSERROR));
    for (i = 0; i < 3; i++) 
	{
		cquantize->fserrors[i] = (FSERRPTR) XP_CALLOC(1, arraysize);
		if (!cquantize->fserrors[i]) 
		{
			/* ran out of memory part way thru */
			for (j = 0; j < i; j++) 
			{
				if (cquantize->fserrors[j])
				{
					XP_FREE(cquantize->fserrors[j]);
					cquantize->fserrors[j]=0;
				}
			}
			if (cquantize)
			{
				XP_FREE(cquantize);
				ic->quantize = 0;
			}
			goto loser;
		}
    }

    return TRUE;
}

/*
** Free up quantizer information attached to ic. If this is the last
** quantizer then free up the sample range limit table.
*/
void
il_free_quantize(il_container *ic)
{
    my_cquantize_ptr cquantize = (my_cquantize_ptr) ic->quantize;
    int i;

    if (cquantize) 
    {
#ifdef DEBUG
		if (il_debug > 5) 
			XP_TRACE(("il: 0x%x: free quantize", ic));
#endif
		for (i = 0; i < 3; i++) 
		{
			if (cquantize->fserrors[i]) 
			{
				XP_FREE(cquantize->fserrors[i]);
				cquantize->fserrors[i] = 0;
			}
		}

		XP_FREE(cquantize);
		ic->quantize = 0;
    }
}


/* floyd-steinberg dithering */

#ifdef XP_MAC
#ifndef powerc
#pragma peephole on
#endif
#endif

void
il_quantize_fs_dither(il_container *ic, const uint8 *mask,
                      const uint8 *input_buf, int x_offset,
                      uint8 XP_HUGE *output_buf, int width)
{
    my_cquantize_ptr cquantize;
    register LOCFSERROR r_cur, g_cur, b_cur;       /* current error or pixel
                                                      value */
    LOCFSERROR r_belowerr, g_belowerr, b_belowerr; /* error for pixel below
                                                      cur */
    LOCFSERROR r_bpreverr, g_bpreverr, b_bpreverr; /* error for below/prev
                                                      col */
    LOCFSERROR r_bnexterr, g_bnexterr, b_bnexterr; /* error for below/next
                                                      col */
    LOCFSERROR delta;
    FSERRPTR r_errorptr, g_errorptr, b_errorptr;   /* fserrors[] at column
                                                      before current */
    const JSAMPLE* input_ptr;
    JSAMPLE XP_HUGE * output_ptr;
    IL_ColorMap *cmap = &ic->image->header.color_space->cmap;
    IL_RGB *map = cmap->map;              /* The colormap array. */
    IL_RGB *map_entry;                    /* Current entry in the colormap. */
    uint8 *lookup_table = cmap->table;    /* Lookup table for the colormap. */
    const uint8 *maskp;
    uint8 map_index;
    int dir;                   /* 1 for left-to-right, -1 for right-to-left */
    JDIMENSION col;
    JSAMPLE *range_limit = the_sample_range_limit;
    SHIFT_TEMPS

    cquantize = (my_cquantize_ptr) ic->quantize;
    
    output_buf += x_offset;

    /* Initialize output values to 0 so can process components separately */
    if (mask) {
        output_ptr = output_buf;
        maskp = mask;
        for (col = width; col > 0; col--)
            *output_ptr++ &= ~*maskp++;
    } else {
        XP_BZERO((void XP_HUGE *) output_buf,
                 (size_t) (width * SIZEOF(JSAMPLE)));
    }

    input_ptr = input_buf;
    output_ptr = output_buf;
    maskp = mask;
    if (cquantize->on_odd_row) {
        int total_offset;

        /* work right to left in this row */
        input_ptr += 3 * width - 1; /* so point to the blue sample of the
                                       rightmost pixel */
        output_ptr += width-1;
        dir = -1;
        /* => entry after last column */
        total_offset = x_offset + (width + 1);
        r_errorptr = cquantize->fserrors[0] + total_offset;
        g_errorptr = cquantize->fserrors[1] + total_offset;
        b_errorptr = cquantize->fserrors[2] + total_offset;
        maskp += (width - 1);
    } 
    else {
        /* work left to right in this row */
        dir = 1;
        /* => entry before first column */
        r_errorptr = cquantize->fserrors[0] + x_offset;
        g_errorptr = cquantize->fserrors[1] + x_offset;
        b_errorptr = cquantize->fserrors[2] + x_offset;
    }

    /* Preset error values: no error propagated to first pixel from left */
    r_cur = g_cur = b_cur = 0;

    /* and no error propagated to row below yet */
    r_belowerr = g_belowerr = b_belowerr = 0;
    r_bpreverr = g_bpreverr = b_bpreverr = 0;

    for (col = width; col > 0; col--) {
        /* cur holds the error propagated from the previous pixel on the
         * current line.  Add the error propagated from the previous line
         * to form the complete error correction term for this pixel, and
         * round the error term (which is expressed * 16) to an integer.
         * RIGHT_SHIFT rounds towards minus infinity, so adding 8 is correct
         * for either sign of the error value.
         * Note: errorptr points to *previous* column's array entry.
         */
        r_cur = RIGHT_SHIFT(r_cur + r_errorptr[dir] + 8, 4);
        g_cur = RIGHT_SHIFT(g_cur + g_errorptr[dir] + 8, 4);
        b_cur = RIGHT_SHIFT(b_cur + b_errorptr[dir] + 8, 4);

        /* Form pixel value + error, and range-limit to 0..MAXJSAMPLE.
         * The maximum error is +- MAXJSAMPLE; this sets the required size
         * of the range_limit array.
         */
        if (dir > 0) {
            r_cur += GETJSAMPLE(*input_ptr);
            r_cur = GETJSAMPLE(range_limit[r_cur]);
            input_ptr++;
            g_cur += GETJSAMPLE(*input_ptr);
            g_cur = GETJSAMPLE(range_limit[g_cur]);
            input_ptr++;
            b_cur += GETJSAMPLE(*input_ptr);
            b_cur = GETJSAMPLE(range_limit[b_cur]);
            input_ptr++;
        }
        else {
            b_cur += GETJSAMPLE(*input_ptr);
            b_cur = GETJSAMPLE(range_limit[b_cur]);
            input_ptr--;
            g_cur += GETJSAMPLE(*input_ptr);
            g_cur = GETJSAMPLE(range_limit[g_cur]);
            input_ptr--;
            r_cur += GETJSAMPLE(*input_ptr);
            r_cur = GETJSAMPLE(range_limit[r_cur]);
            input_ptr--;
        }

        /* Select output value, accumulate into output code for this pixel */
        map_index = COLORMAP_INDEX(lookup_table, r_cur, g_cur, b_cur);
        if (mask) {
            if (*maskp)
                *output_ptr = map_index;
            maskp += dir;
        } else {
            *output_ptr = map_index;
        }

        /* Compute the actual representation error at this pixel */
        map_entry = map + map_index;
        r_cur -= GETJSAMPLE(map_entry->red);
        g_cur -= GETJSAMPLE(map_entry->green);
        b_cur -= GETJSAMPLE(map_entry->blue);

        /* Compute error fractions to be propagated to adjacent pixels.
         * Add these into the running sums, and simultaneously shift the
         * next-line error sums left by 1 column.
         */
        r_bnexterr = r_cur;
        delta = r_cur * 2;
        r_cur += delta;		/* form error * 3 */
        r_errorptr[0] = (FSERROR) (r_bpreverr + r_cur);
        r_cur += delta;		/* form error * 5 */
        r_bpreverr = r_belowerr + r_cur;
        r_belowerr = r_bnexterr;
        r_cur += delta;		/* form error * 7 */

        g_bnexterr = g_cur;
        delta = g_cur * 2;
        g_cur += delta;		/* form error * 3 */
        g_errorptr[0] = (FSERROR) (g_bpreverr + g_cur);
        g_cur += delta;		/* form error * 5 */
        g_bpreverr = g_belowerr + g_cur;
        g_belowerr = g_bnexterr;
        g_cur += delta;		/* form error * 7 */

        b_bnexterr = b_cur;
        delta = b_cur * 2;
        b_cur += delta;		/* form error * 3 */
        b_errorptr[0] = (FSERROR) (b_bpreverr + b_cur);
        b_cur += delta;		/* form error * 5 */
        b_bpreverr = b_belowerr + b_cur;
        b_belowerr = b_bnexterr;
        b_cur += delta;		/* form error * 7 */

        /* At this point cur contains the 7/16 error value to be propagated
         * to the next pixel on the current line, and all the errors for the
         * next line have been shifted over. We are therefore ready to move on.
         * Note: the input_ptr has already been advanced.
         */
        output_ptr += dir;	/* advance output ptr to next column */
        r_errorptr += dir;	/* advance errorptr to current column */
        g_errorptr += dir;	/* advance errorptr to current column */
        b_errorptr += dir;	/* advance errorptr to current column */
    }
    
    /* Post-loop cleanup: we must unload the final error value into the
     * final fserrors[] entry.  Note we need not unload belowerr because
     * it is for the dummy column before or after the actual array.
     */
    r_errorptr[0] = (FSERROR) r_bpreverr; /* unload prev err into array */
    g_errorptr[0] = (FSERROR) g_bpreverr; /* unload prev err into array */
    b_errorptr[0] = (FSERROR) b_bpreverr; /* unload prev err into array */

    cquantize->on_odd_row = (cquantize->on_odd_row ? FALSE : TRUE);
}

#ifdef XP_MAC
#ifndef powerc
#pragma peephole reset
#endif
#endif

/* END code adapted from jpeg library */

