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
 *   $Id: colormap.cpp,v 3.2 1999/11/04 23:10:59 sfraser%netscape.com Exp $
 */


#include "if.h"


/* Force il_set_color_palette() to load a new colormap for an image */
PRBool
il_reset_palette(il_container *ic)
{
    PRBool ret = PR_TRUE;
    NI_ColorMap *cmap;

    if(ic->src_header){
        if(ic->src_header->color_space){
            cmap = &ic->src_header->color_space->cmap;
            if(cmap->num_colors > 0){
                cmap->num_colors=0;
            }
        }else{
            ret = PR_FALSE;
        }
    }else{
        ret = PR_FALSE;
    }

    ic->colormap_serial_num = -1;
    ic->dont_use_custom_palette = FALSE;
    ic->rendered_with_custom_palette = FALSE;
    
    return ret;
   
}

