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
 *  il_util.h Colormap and colorspace utilities.
 *             
 *   $Id: il_util.h,v 3.2 1998/07/27 16:09:08 hardts%netscape.com Exp $
 */


#ifndef _IL_UTIL_H
#define _IL_UTIL_H

#include "ni_pixmp.h"  /* Cross-platform pixmap structure. */
#include "il_types.h"

XP_BEGIN_PROTOS

/************************* Colormap utilities ********************************/

/* Create a new color map consisting of a given set of reserved colors, and
   a color cube.  num_colors represents the requested size of the colormap,
   including the reserved colors.  The actual number of colors in the colormap
   could be less depending on the color cube that is allocated.

   The Image Library will only make use of entries in the color cube.  This
   function represents the current state of affairs, and it will eventually
   be replaced when the Image Library has the capability to dither to an
   arbitrary palette. */
IL_EXTERN(IL_ColorMap *)
IL_NewCubeColorMap(IL_RGB *reserved_colors, uint16 num_reserved_colors,
                   uint16 num_colors);

/* Create an optimal fixed palette of the specified size, starting with
   the given set of reserved colors.
   XXX - This will not be implemented initially. */
IL_EXTERN(IL_ColorMap *)
IL_NewOptimalColorMap(IL_RGB *reserved_colors, uint16 num_reserved_colors,
                      uint16 num_colors);

/* Create an empty colormap.  The caller is responsible for filling in the
   colormap entries. */
IL_EXTERN(IL_ColorMap *)
IL_NewColorMap(void);

/* Append the specified color to an existing IL_ColorMap, returning TRUE if
   successful.  The position of the new color in the IL_ColorMap's map array
   is returned in new_color->index.  The caller should also update the
   corresponding entry in the IL_ColorMap's index array,
   cmap->index[new_color->index], if the actual colormap indices do not
   correspond to the order of the entries in the map array.

   Note: For now, at least, this function does not cause the Image Library's
   lookup table to be altered, so the Image Library will continue to dither
   to the old colormap.  Therefore, the current purpose of this function is
   to add colors (such as a background color for transparent images) which
   are not a part of the Image Library's color cube. */
IL_EXTERN(int)
IL_AddColorToColorMap(IL_ColorMap *cmap, IL_IRGB *new_color);

/* Free all memory associated with a given colormap.
   Note: This should *not* be used to destroy a colormap once it has been
   passed into IL_CreatePseudoColorSpace.  Use IL_ReleaseColorSpace instead. */
IL_EXTERN(void)
IL_DestroyColorMap (IL_ColorMap *cmap);

/* Reorder the entries in a colormap.  new_order is an array mapping the old
   indices to the new indices.
   XXX Is this really necessary? */
IL_EXTERN(void)
IL_ReorderColorMap(IL_ColorMap *cmap, uint16 *new_order);


/************************** Colorspace utilities *****************************/

/* Create a new True-colorspace of the dimensions specified by IL_RGBBits and
   set the reference count to 1.  The pixmap_depth is the sum of the bits
   assigned to the three color channels, plus any additional allowance that
   might be necessary, e.g. for an alpha channel, or for alignment.  Note: the
   contents of the IL_RGBBits structure will be copied, so they need not be
   preserved after the call to IL_CreateTrueColorSpace. */
IL_EXTERN(IL_ColorSpace *)
IL_CreateTrueColorSpace(IL_RGBBits *rgb, uint8 pixmap_depth);

/* Create a new Pseudo-colorspace using the given colormap and set the
   reference count to 1.  The index_depth is the bit-depth of the colormap
   indices (typically 8), while the pixmap_depth is the index_depth plus any
   additional allowance that might be necessary e.g. for an alpha channel, or
   for alignment.  Note: IL_ColorMaps passed into IL_CreatePseudoColorSpace
   become a part of the IL_ColorSpace structure.  The IL_ColorMap pointer is
   invalid after the the call to IL_CreatePseudoColorSpace, so it should
   neither be accessed, nor destroyed using IL_DestroyColorMap.  Access to
   the colormap, *is* available through the colormap member of the
   IL_ColorSpace.  Memory associated with the colormap will be freed by
   IL_ReleaseColorSpace when the reference count reaches zero. */
IL_EXTERN(IL_ColorSpace *)
IL_CreatePseudoColorSpace(IL_ColorMap *cmap, uint8 index_depth,
                          uint8 pixmap_depth);

/* Create a new Greyscale-colorspace of depth specified by index_depth and
   set the reference count to 1.  The pixmap_depth is the index_depth plus
   any additional allowance that might be necessary e.g. for an alpha channel,
   or for alignment. */
IL_EXTERN(IL_ColorSpace *)
IL_CreateGreyScaleColorSpace(uint8 index_depth, uint8 pixmap_depth);

/* Decrements the reference count for an IL_ColorSpace.  If the reference
   count reaches zero, all memory associated with the colorspace (including
   any colormap associated memory) will be freed. */
IL_EXTERN(void)
IL_ReleaseColorSpace(IL_ColorSpace *color_space);

/* Increment the reference count for an IL_ColorSpace. */
IL_EXTERN(void)
IL_AddRefToColorSpace(IL_ColorSpace *color_space);

XP_END_PROTOS
#endif /* _IL_UTIL_H */
