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
 *  il_util.c Colormap and colorspace utilities.
 *             
 *   $Id: il_util.cpp,v 3.8 2000/07/20 01:52:02 pnunn%netscape.com Exp $
 */


#include "nsCRT.h"

#include "prtypes.h"
#include "prmem.h"

#include "il_util.h"            /* Public API. */
#include "il_utilp.h"           /* Private header file. */

/************************* Colormap utilities ********************************/

/* Create a new color cube with the specified dimensions, starting at the
   given base_offset.  The total number of colors in the colormap will be
   base_offset + red_size * green_size * blue_size.  The caller is
   responsible for filling in its reserved colors, 0, 1, ..., base_offset-1.

   Note: the lookup table used here is of the form we will use when dithering
   to an arbitrary palette. */
static IL_ColorMap *
il_NewColorCube(PRUint32 red_size, PRUint32 green_size, PRUint32 blue_size,
                PRUint32 base_offset)
{
    PRUint8 r, g, b, map_index;
    PRUint32 i, j, k, size, red_offset, green_offset, dmax_val;
    PRUint32 trm1, tgm1, tbm1, dtrm1, dtgm1, dtbm1;
    PRUint32 crm1, cgm1, cbm1, dcrm1, dcgm1, dcbm1;
    PRUint8 *lookup_table, *ptr, *done;
    IL_RGB *map;
    IL_ColorMap *cmap;

    /* Colormap size and offsets for computing the colormap indices. */
    size = base_offset + red_size * green_size * blue_size;
    if (size > CUBE_MAX_SIZE)
        return PR_FALSE;
    red_offset = green_size * blue_size;
    green_offset = blue_size;

     /* Operation on lookup table dimensions. */
    trm1 = LOOKUP_TABLE_RED - 1;    dtrm1 = trm1 << 1;
    tgm1 = LOOKUP_TABLE_GREEN - 1;  dtgm1 = tgm1 << 1;
    tbm1 = LOOKUP_TABLE_BLUE - 1;   dtbm1 = tbm1 << 1; 

    /* Operations on color cube dimensions. */
    crm1 = red_size - 1;    dcrm1 = crm1 << 1;
    cgm1 = green_size - 1;  dcgm1 = cgm1 << 1;
    cbm1 = blue_size - 1;   dcbm1 = cbm1 << 1; 

    /* Operation on target RGB color space dimensions. */
    dmax_val = 255 << 1;

    /* We may want to add entries to the map array subsequently, so always
       allocate space for a full palette. */
    map = (IL_RGB *)PR_Calloc(256, sizeof(IL_RGB));
    if (!map)
        return PR_FALSE;

    lookup_table = (PRUint8 *)PR_Calloc(LOOKUP_TABLE_SIZE, 1);
    if (!lookup_table)
        return PR_FALSE;

    done = (PRUint8 *)PR_Calloc(size, 1);
    if (!done)
        return PR_FALSE;
    
    ptr = lookup_table;
    for (i = 0; i < LOOKUP_TABLE_RED; i++)
        for (j = 0; j < LOOKUP_TABLE_GREEN; j++)
            for (k = 0; k < LOOKUP_TABLE_BLUE; k++) {
                /* Scale indices down to cube coordinates. */
                r = (PRUint8) (CUBE_SCALE(i, dcrm1, trm1, dtrm1));
                g = (PRUint8) (CUBE_SCALE(j, dcgm1, tgm1, dtgm1));
                b = (PRUint8) (CUBE_SCALE(k, dcbm1, tbm1, dtbm1));

                /* Compute the colormap index. */
                map_index =(PRUint8)( r * red_offset + g * green_offset + b +
                    base_offset);

                /* Fill out the colormap entry for this index if we haven't
                   already done so. */
                if (!done[map_index]) {
                    /* Scale from cube coordinates up to 8-bit RGB values. */
                    map[map_index].red =
                        (PRUint8) (CUBE_SCALE(r, dmax_val, crm1, dcrm1));
                    map[map_index].green = 
                        (PRUint8) (CUBE_SCALE(g, dmax_val, cgm1, dcgm1));
                    map[map_index].blue =
                        (PRUint8) (CUBE_SCALE(b, dmax_val, cbm1, dcbm1));

                    /* Mark as done. */
                    done[map_index] = 1;
                }

                /* Fill in the lookup table entry with the colormap index. */
                *ptr++ = map_index;
            }
    PR_FREEIF(done);


    cmap = PR_NEWZAP(IL_ColorMap);
    if (!cmap) {
        PR_FREEIF(map);
        PR_FREEIF(lookup_table);
        return NULL;
    }
    cmap->num_colors = size;
    cmap->map = map;
    cmap->index = NULL;
    cmap->table = (void *)lookup_table;

    return cmap;
}

/* Determine allocation of desired colors to components, and fill in Ncolors[]
   array to indicate choice.  Return value is total number of colors (product
   of Ncolors[] values). */
static int
select_ncolors(int Ncolors[],
               int out_color_components,
               int desired_number_of_colors)
{
    int nc = out_color_components; /* number of color components */
    int max_colors = desired_number_of_colors;
    int total_colors, iroot, i, j;
    long temp;

        /* XXX - fur .  Is this right ? */
        static const int RGB_order[3] = { 2, 1, 0 };

    /* We can allocate at least the nc'th root of max_colors per component. */
    /* Compute floor(nc'th root of max_colors). */
    iroot = 1;
    do {
        iroot++;
        temp = iroot;       /* set temp = iroot ** nc */
        for (i = 1; i < nc; i++)
            temp *= iroot;
    } while (temp <= (long) max_colors); /* repeat till iroot exceeds root */
    iroot--;            /* now iroot = floor(root) */

    /* Must have at least 2 color values per component */
    if (iroot < 2)
        return -1;

    /* Initialize to iroot color values for each component */
    total_colors = 1;
    for (i = 0; i < nc; i++)
    {
        Ncolors[i] = iroot;
        total_colors *= iroot;
    }

    /* We may be able to increment the count for one or more components without
     * exceeding max_colors, though we know not all can be incremented.
     * In RGB colorspace, try to increment G first, then R, then B.
     */
    for (i = 0; i < nc; i++)
    {
        j = RGB_order[i];
        /* calculate new total_colors if Ncolors[j] is incremented */
        temp = total_colors / Ncolors[j];
        temp *= Ncolors[j]+1;   /* done in long arith to avoid oflo */
        if (temp > (long) max_colors)
            break;          /* won't fit, done */
        Ncolors[j]++;       /* OK, apply the increment */
        total_colors = (int) temp;
    }
    return total_colors;
}

/* Create a new color map consisting of a given set of reserved colors, and
   a color cube.  num_colors represents the requested size of the colormap,
   including the reserved colors.  The actual number of colors in the colormap
   could be less depending on the color cube that is allocated.

   The Image Library will only make use of entries in the color cube.  This
   function represents the current state of affairs, and it will eventually
   be replaced when the Image Library has the capability to dither to an
   arbitrary palette. */
IL_IMPLEMENT(IL_ColorMap *)
IL_NewCubeColorMap(IL_RGB *reserved_colors, PRUint16 num_reserved_colors,
                   PRUint16 num_colors)
{
    int i;
    IL_RGB *map;
    IL_ColorMap *cmap;
    int Ncolors[3];             /* Size of the color cube. */
    int num_cube_colors;

    /* Determine the size of the color cube. */
    num_cube_colors = select_ncolors(Ncolors, 3,
                                     num_colors - num_reserved_colors);
    
    /* Create the color cube.  */
    cmap = il_NewColorCube(Ncolors[0], Ncolors[1], Ncolors[2],
                           num_reserved_colors);
    if(!cmap)
        return NULL;

    /* Fill in the reserved colors. */
    map = cmap->map;
    for (i = 0; i < num_reserved_colors; i++) {
        map[i].red = reserved_colors[i].red;
        map[i].green = reserved_colors[i].green;
        map[i].blue = reserved_colors[i].blue;
    }

    return cmap;
}

/* Create an optimal fixed palette of the specified size, starting with
   the given set of reserved colors. */
IL_IMPLEMENT(IL_ColorMap *)
IL_NewOptimalColorMap(IL_RGB *reserved_colors, PRUint16 num_reserved_colors,
                      PRUint16 num_colors)
{
    /* XXXM12N Implement me. */
    return NULL;
}

/* Create an empty colormap.  The caller is responsible for filling in the
   colormap entries. */
IL_IMPLEMENT(IL_ColorMap *)
IL_NewColorMap(void)
{
    IL_RGB *map;
    IL_ColorMap *cmap;

    cmap = PR_NEWZAP(IL_ColorMap);
    if (!cmap)
        return NULL;

    /* We always allocate space for a full palette. */
    map = (IL_RGB *)PR_Calloc(256, sizeof(IL_RGB));
    if (!map) {
        PR_FREEIF(cmap);
        return NULL;
    }
    
    cmap->num_colors = 0;
    cmap->map = map;
    cmap->index = NULL;
    cmap->table = NULL;

    return cmap;
}

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
IL_IMPLEMENT(int)
IL_AddColorToColorMap(IL_ColorMap *cmap, IL_IRGB *new_color)
{
    int max_colors = 256;
    int32 num_colors = cmap->num_colors;
    IL_RGB *map = cmap->map;
    IL_RGB *map_entry;

    if (num_colors > max_colors)
        return PR_FALSE;

    map_entry = map + num_colors;
    map_entry->red = new_color->red;
    map_entry->green = new_color->green;
    map_entry->blue = new_color->blue;

    new_color->index = (PRUint8) num_colors;

    cmap->num_colors++;

    return PR_TRUE;
}

/* Free all memory associated with a given colormap.
   Note: This should *not* be used to destroy a colormap once it has been
   passed into IL_CreatePseudoColorSpace.  Use IL_ReleaseColorSpace instead */
IL_IMPLEMENT(void)
IL_DestroyColorMap (IL_ColorMap *cmap)
{
    if (cmap) {
        PR_FREEIF(cmap->map);
        PR_FREEIF(cmap->index);
        PR_FREEIF(cmap->table);
        PR_FREEIF(cmap);
    }
}

/* Reorder the entries in a colormap.  new_order is an array mapping the old
   indices to the new indices. */
IL_IMPLEMENT(void)
IL_ReorderColorMap(IL_ColorMap *cmap, PRUint16 *new_order)
{
}


/************************** Colorspace utilities *****************************/

/* Create a new True-colorspace of the dimensions specified by IL_RGBBits and
   set the reference count to 1.  The pixmap_depth is the sum of the bits
   assigned to the three color channels, plus any additional allowance that
   might be necessary, e.g. for an alpha channel, or for alignment.  Note: the
   contents of the IL_RGBBits structure will be copied, so they need not be
   preserved after the call to IL_CreateTrueColorSpace. */
IL_IMPLEMENT(IL_ColorSpace *)
IL_CreateTrueColorSpace(IL_RGBBits *rgb, PRUint8 pixmap_depth)
{
    IL_ColorSpace *color_space;

    color_space = PR_NEWZAP(IL_ColorSpace);
    if (!color_space)
        return NULL;

    color_space->type = NI_TrueColor;

    /* RGB bit allocation and offsets. */
    nsCRT::memcpy(&color_space->bit_alloc.rgb, rgb, sizeof(IL_RGBBits));

    color_space->pixmap_depth = pixmap_depth; /* Destination image depth. */

    /* Create the private part of the color_space */
    color_space->private_data = (void *)PR_NEWZAP(il_ColorSpaceData);
    if (!color_space->private_data) {
        PR_FREEIF(color_space);
        return NULL;
    }
        
    color_space->ref_count = 1;
    return color_space;
}

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
IL_IMPLEMENT(IL_ColorSpace *)
IL_CreatePseudoColorSpace(IL_ColorMap *cmap, PRUint8 index_depth,
                          PRUint8 pixmap_depth)
{
    IL_ColorSpace *color_space;

    color_space = PR_NEWZAP(IL_ColorSpace);
    if (!color_space)
        return NULL;

    color_space->type = NI_PseudoColor;
    color_space->bit_alloc.index_depth = index_depth;
    color_space->pixmap_depth = pixmap_depth;

   /* Copy the contents of the IL_ColorMap structure.  This copies the map
      and table pointers, not the arrays themselves. */
    nsCRT::memcpy(&color_space->cmap, cmap, sizeof(IL_ColorMap)); 
    PR_FREEIF(cmap);

    /* Create the private part of the color_space */
    color_space->private_data = (void *)PR_NEWZAP(il_ColorSpaceData);
    if (!color_space->private_data) {
        PR_FREEIF(color_space);
        return NULL;
    }

    color_space->ref_count = 1;
    return color_space;
}

/* Create a new Greyscale-colorspace of depth specified by index_depth and
   set the reference count to 1.  The pixmap_depth is the index_depth plus
   any additional allowance that might be necessary e.g. for an alpha channel,
   or for alignment. */
PRBool
IL_CreateGreyScaleColorSpace(PRUint8 index_depth, PRUint8 pixmap_depth, IL_ColorSpace **color_space_ptr)
{
    IL_ColorSpace *color_space;

    *color_space_ptr = PR_NEWZAP(IL_ColorSpace);
    if (!*color_space_ptr)
        return PR_FALSE;

    color_space = *color_space_ptr;

    color_space->type = NI_GreyScale;
    color_space->bit_alloc.index_depth = index_depth;
    color_space->pixmap_depth = pixmap_depth;
    color_space->cmap.num_colors = (1 << index_depth);

    /* Create the private part of the color_space */
    color_space->private_data = (void *)PR_NEWZAP(il_ColorSpaceData);
    if (!color_space->private_data) {
        PR_FREEIF(*color_space_ptr);
        return PR_FALSE;
    }

    color_space->ref_count = 1;
    return PR_TRUE;
}

/* Decrements the reference count for an IL_ColorSpace.  If the reference
   count reaches zero, all memory associated with the colorspace (including
   any colormap associated memory) will be freed. */
IL_IMPLEMENT(void)
IL_ReleaseColorSpace(IL_ColorSpace *color_space)
{
    color_space->ref_count--;
        
    if (color_space->ref_count == 0) {
        IL_ColorMap *cmap = &color_space->cmap;
        il_ColorSpaceData *private_data =
            (il_ColorSpaceData *)color_space->private_data;

        /* Free any colormap associated memory. */
        if (cmap->map)  {
            PR_FREEIF(cmap->map);
            cmap->map = NULL;
        }
        if (cmap->index)    {
            PR_FREEIF(cmap->index);
            cmap->index = NULL;
        }
        if (cmap->table)    {
            PR_FREEIF(cmap->table);
            cmap->table = NULL;
        }
        
        if (private_data) {

            /* Free any RGB depth conversion maps. */
            if (private_data->r8torgbn) {
                PR_FREEIF(private_data->r8torgbn);
                private_data->r8torgbn = NULL;
            }
            if (private_data->g8torgbn) {
                PR_FREEIF(private_data->g8torgbn);
                private_data->g8torgbn = NULL;
            }
            if (private_data->b8torgbn) {
                PR_FREEIF(private_data->b8torgbn);
                private_data->b8torgbn = NULL;
            }

            /* Free the il_ColorSpaceData */
            PR_FREEIF(private_data);
            color_space->private_data = NULL;
        }

        /* Free the IL_ColorSpace structure. */
        PR_FREEIF(color_space);
    }
}


/* Increment the reference count for an IL_ColorSpace. */
IL_IMPLEMENT(void)
IL_AddRefToColorSpace(IL_ColorSpace *color_space)
{
    color_space->ref_count++;
}
