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
 *  colormap.c
 *             
 *   $Id: colormap.cpp,v 3.1 1998/07/27 16:09:21 hardts%netscape.com Exp $
 */


#include "if.h"

/*
int
IL_ColormapTag(const char* image_url, MWContext* cx)
{
	return 0;
}
*/

/* Force il_set_color_palette() to load a new colormap for an image */
void
il_reset_palette(il_container *ic)
{
    NI_ColorMap *cmap = &ic->src_header->color_space->cmap;

    ic->colormap_serial_num = -1;
    ic->dont_use_custom_palette = FALSE;
    ic->rendered_with_custom_palette = FALSE;

    if (cmap->num_colors > 0)
        cmap->num_colors = 0;
}

