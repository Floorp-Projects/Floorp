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
 */

/* -*- Mode: C; tab-width: 4 -*-
 *  il_utilp.h Colormap and colorspace utilities - types and definitions
 *             private to Image Library.            
 *
 *   $Id: il_utilp.h,v 1.6 2001/06/19 11:15:49 pavlov%netscape.com Exp $
 */


/************************* Colormap utilities ********************************/

/* Parameters for building a color cube and its associated lookup table. */
#define LOOKUP_TABLE_SIZE  32768
#define LOOKUP_TABLE_RED   32
#define LOOKUP_TABLE_GREEN 32
#define LOOKUP_TABLE_BLUE  32
#define CUBE_MAX_SIZE      256

/* Macro to convert 8-bit/channel RGB data into an 8-bit colormap index. */
#define COLORMAP_INDEX(lookup_table, red, green, blue)     \
        lookup_table[LOOKUP_TABLE_INDEX(red, green, blue)]

/* Macro to convert 8-bit/channel RGB data into a 16-bit lookup table index.
   The lookup table value is the index to the colormap. */
#define LOOKUP_TABLE_INDEX(red, green, blue) \
        ((USE_5_BITS(red) << 10) |           \
        (USE_5_BITS(green) << 5) |           \
        USE_5_BITS(blue))

/* Take the 5 most significant bits of an 8-bit value. */
#define USE_5_BITS(x) ((x) >> 3)

/* Scaling macro for creating color cubes. */
#define CUBE_SCALE(val, double_new_size_minus1, old_size_minus1, \
                   double_old_size_minus1)                       \
        ((val) * (double_new_size_minus1) + (old_size_minus1)) / \
        (double_old_size_minus1)


/************************** Colorspace utilities *****************************/

/* Image Library private part of an IL_ColorSpace structure. */
typedef struct il_ColorSpaceData {
    /* RGB24 to RGBN depth conversion maps.  Each of these maps take an
       8-bit input for a color channel and converts it into that channel's
       contribution to a depth N pixmap e.g. for a 24 to 16-bit color
       conversion, the output pixel is given by

       PRUint8 red, green, blue;
       PRUint16 output_pixel;
       output_pixel = r8torgbn[red] + g8torgbn[green] + b8torgbn[blue];

       Depth conversion maps are created for the following destination image
       pixmap depths: N = 8, 16 and 32.  The type of the array elements is a
       PRUintn. */
    void *r8torgbn;
    void *g8torgbn;
    void *b8torgbn;
} il_ColorSpaceData;


