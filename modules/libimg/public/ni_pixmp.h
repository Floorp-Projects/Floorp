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
 *  ni_pixmp.h --- Cross platform pixmap data structure.
 *  $Id: ni_pixmp.h,v 3.2 1998/07/27 16:09:11 hardts%netscape.com Exp $
 */


#ifndef _NI_PIXMAP_H
#define _NI_PIXMAP_H
#include "xp_mem.h"
/************** Colorimetry/Gamma Correction Information. ********************
* This doesn't need to be implemented initially, but it's here for future    * 
* usage.                                                                     */

/* The chrominance part of a CIE xyY color specification */
typedef struct CIEchroma {
    float x;
    float y;
} CIEchroma;


/* Define a device-independent color space using the white point and
   primary colors of a gamut formed by RGB tristimulus values. */
typedef struct NI_RGBColorSpace {
    CIEchroma white_point;
    CIEchroma primary_red;
    CIEchroma primary_green;
    CIEchroma primary_blue;
} NI_RGBColorSpace;


typedef struct NI_ColorSpec {
    double gamma;
    NI_RGBColorSpace rgb_colorspace;
} NI_ColorSpec;


/***************** Colorspace and Colormap Information ***********************/

/* Possible colorspace types. */
typedef enum _NI_ColorSpaceType
{
    NI_TrueColor   = 0x01,         /* RGB data.       */
    NI_PseudoColor = 0x02,         /* Indexed data.   */
    NI_GreyScale   = 0x04          /* Greyscale data. */
} NI_ColorSpaceType;


/* RGB bit allocation and offsets. */
typedef struct _NI_RGBBits {
    uint8 red_bits;             /* Number of bits assigned to red channel. */
    uint8 red_shift;            /* Offset for red channel bits. */
    uint8 green_bits;           /* Number of bits assigned to green channel. */
    uint8 green_shift;          /* Offset for green channel bits. */
    uint8 blue_bits;            /* Number of bits assigned to blue channel. */
    uint8 blue_shift;           /* Offset for blue channel bits. */
} NI_RGBBits;


/* An indexed RGB triplet. */
typedef struct _NI_IRGB {
    uint8 index;
    uint8 red, green, blue;
} NI_IRGB;


/* A RGB triplet representing a single pixel in the image's colormap
   (if present.) */
typedef struct _NI_RGB
{
    uint8 red, green, blue, pad; /* Windows requires the fourth byte &
                                    many compilers pad it anyway. */
    uint16 hist_count;           /* Histogram frequency count. */
} NI_RGB;


/* Colormap information. */
typedef struct _NI_ColorMap {
    int32 num_colors;           /* Number of colors in the colormap.
                                   A negative value can be used to denote a
                                   possibly non-unique set. */
    NI_RGB *map;                /* Colormap colors. */
    uint8 *index;               /* NULL, if map is in index order.  Otherwise
                                   specifies the indices of the map entries. */
    void *table;                /* Lookup table for this colormap.  Private to
                                   the Image Library. */
} NI_ColorMap;


/* Special purpose flags for OS-specific problems. */
typedef enum {
    WIN95_ROUNDING = 0x01       /* Windows 95 color quantization bug. */
} NI_OSFlags;


/* Colorspace information. */
typedef struct _NI_ColorSpace {
    NI_ColorSpaceType type;     /* NI_Truecolor, NI_Pseudocolor or
                                   NI_Greyscale. */

    /* The dimensions of the colorspace. */
    union {
        NI_RGBBits rgb;         /* For TrueColor. */
        uint8 index_depth;      /* For PseudoColor and GreyScale. */
    } bit_alloc;                /* Allocation of bits. */
    uint8 pixmap_depth;         /* Total bit depth (including alpha or pad.) */

    /* Colormap information.  This may be used for one of three purposes:
       - If the colorspace belongs to a PseudoColor source image, then the
         colormap represents the mapping from the source image indices to
         the corresponding RGB components.
       - If the colorspace belongs to a TrueColor source image, then a
         colormap may be provided as a suggested palette for displaying the
         image on PseudoColor displays.
       - If the colorspace belongs to a PseudoColor Display Front End or a
         destination image for a PseudoColor Display Front End, then the
         colormap represents the mapping from the display's palette indices
         to the corresponding RGB components. */
    NI_ColorMap cmap;

    /* Image Library private data for this colorspace. */
    void *private_data;

    /* Special purpose flags for OS-specific problems. */
    uint8 os_flags;             /* Flags are of type NI_OSFlags. */

    /* Reference counter. */
    uint32 ref_count;
} NI_ColorSpace;


/* A pixmap's header information. */
typedef struct _NI_PixmapHeader
{
    /* Size. */
    uint32 width;               /* Width. */
    uint32 height;              /* Height. */
    uint32 widthBytes;          /* width * depth / 8.  May be aligned for
                                   optimizations. */
    
    /* Colorspace. */
    NI_ColorSpace *color_space; /* Colorspace and colormap information. */

    /* Transparency. */
    NI_IRGB *transparent_pixel; /* The image's transparent pixel
                                   (if present.) */
    uint8 alpha_bits;           /* Number of bits assigned to alpha channel. */
    uint8 alpha_shift;          /* Offset for alpha channel bits. */
    int32 is_interleaved_alpha; /* Is alpha channel interleaved with
                                   image data? */

    /* Gamma/color correction. */
    NI_ColorSpec color_spec;
} NI_PixmapHeader;


/* A pixmap. */
typedef struct _NI_Pixmap
{
    NI_PixmapHeader header;     /* Header information. */
    void XP_HUGE *bits;         /* Pointer to the bits. */
    void *client_data;          /* Pixmap-specific data opaque to the Image
								   Library e.g. display front-ends which
								   support scaling may use this to store the
								   actual size at which the image is to be
								   displayed. */
} NI_Pixmap;

#endif /* _NI_PIXMAP_H */
